#include <msp430.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "lcd.h"
#include "utility.h"

int8_t debounce_switch1() {
    static uint16_t state = 0;                                // holds present state
    state = (state << 1) | bit_is_set(P1IN, BIT2) | 0xE000;   // shifts 1 into state and checks if pin 0 is pressed
    if (state == 0xF000)                                      // after 12 cycles of pin 0 being pressed, a single press will be registered
        return 1;
    return 0;
}

int8_t debounce_switch2() {
    static uint16_t state = 0;                                // holds present state
    state = (state << 1) | bit_is_set(P2IN, BIT6) | 0xE000;   // shifts 1 into state and checks if pin 0 is pressed
    if (state == 0xF000)                                      // after 12 cycles of pin 0 being pressed, a single press will be registered
        return 1;
    return 0;
}

int8_t debounce_front_light() {
    static uint16_t state = 0;                                // holds present state
    state = (state << 1) | bit_is_set(P1IN, BIT3) | 0xE000;   // shifts 1 into state and checks if pin 0 is pressed
    if (state == 0xF000)                                      // after 12 cycles of pin 0 being pressed, a single press will be registered
        return 1;
    return 0;
}

char display[6] = {' ', ' ', ' ', ' ', ' ', ' '};

void initGpio() {
    // Configure outputs
    // green led
    P1DIR |= BIT0;                                // P1.0 output  
    P1OUT &= ~BIT0;                               // P1.0 low

    // red led
    P4DIR |= BIT0;                            // P4.0 output
    P4OUT &= ~BIT0;                               // P4.0 low

    // white led
    P5DIR |= BIT2 | BIT3;                                // P1.0 output  
    P5OUT &= ~(BIT2 | BIT3);                               // P1.0 low

    // mot1/mot2 pwm
    P1DIR |= BIT6 | BIT7;                     // P1.6 and P1.7 output
    P1SEL0 |= BIT6 | BIT7;                    // P1.6 and P1.7 options select

    // mot1/mot2 direction pins
    P8DIR |= BIT0 | BIT1 | BIT2 | BIT3;
    P8OUT &= ~(BIT0 | BIT1 | BIT2 | BIT3);

    // configure inputs
    // s1
    P1DIR &= ~BIT2;
    P1REN |= BIT2;                            // Enable P1.4 internal resistance
    P1OUT |= BIT2;                            // Set P1.4 as pull-Up resistance
    P1IES &= ~BIT2;                            // P1.4 Lo/Hi edge

    // s2
    P2DIR &= ~BIT6;
    P2REN |= BIT6;                            // Enable P1.4 internal resistance
    P2OUT |= BIT6;                            // Set P1.4 as pull-Up resistance
    P2IES &= ~BIT6;                            // P1.4 Low/Hi edge

    // front light
    P1DIR &= ~BIT3;
    // P1REN |= BIT3;                            // Enable P1.4 internal resistance
    // P1OUT |= BIT3;                            // Set P1.4 as pull-Up resistance
    // P1IES &= ~BIT3;                            // P1.4 Lo/Hi edge

    // ir detector
    P1DIR &= ~BIT4;
    P1REN |= BIT4;                            // Enable P1.4 internal resistance
    P1OUT |= BIT4;                            // Set P1.4 as pull-Up resistance
    P1IES &= ~BIT4;                            // P1.4 Lo/Hi edge

    // Configure ADC A1 pin
    SYSCFG2 |= ADCPCTL5;
}

void initRtcCrystal() {
    // Initialize XT1 32kHz crystal
    P4SEL0 |= BIT1 | BIT2;                  // set XT1 pin as second function
    do
    {
        CSCTL7 &= ~(XT1OFFG | DCOFFG);      // Clear XT1 and DCO fault flag
        SFRIFG1 &= ~OFIFG;
    } while (SFRIFG1 & OFIFG);              // Test oscillator fault flag
}

void initRtc() {
    // Initialize RTC
    // 1/32768 * 4 = 122 us
    RTCMOD = 4-1;
    // Source = 32kHz crystal, divided by 1
    RTCCTL = RTCSS__XT1CLK | RTCSR | RTCPS__1 | RTCIE;
}

void initTimer0() {
    TA0CCR0 = 1000-1;                         // PWM Period
    TA0CCTL1 = OUTMOD_7;                      // CCR1 reset/set
    TA0CCR1 = 0;                            // CCR1 PWM duty cycle
    TA0CCTL2 = OUTMOD_7;                      // CCR2 reset/set
    TA0CCR2 = 0;                            // CCR2 PWM duty cycle
    TA0CTL = TASSEL__SMCLK | MC__UP | TACLR;  // SMCLK, up mode, clear TAR
}

void initAdc() {
    // Configure ADC10
    ADCCTL0 |= ADCSHT_2 | ADCON;                              // ADCON, S&H=16 ADC clks
    ADCCTL1 |= ADCSHP;                                        // ADCCLK = MODOSC; sampling timer
    ADCCTL2 |= ADCRES;                                        // 10-bit conversion results
    ADCMCTL0 |= ADCINCH_5 | ADCSREF_0;                        // A1 ADC input select; Vref=Avcc

    // Configure reference
    PMMCTL0_H = PMMPW_H;                                      // Unlock the PMM registers
    PMMCTL2 |= INTREFEN;                                      // Enable internal reference
    __delay_cycles(400);                                      // Delay for reference settling
}

int mode = 0;
int runMotors = 0;
unsigned int motorSpeed = 2000;
int strategyMode = 0;
int runForwardStart = 0;
int backingUp = 0;

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;                     // Stop WDT

    initGpio();
    initRtcCrystal();
    initOscillator();
  
    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

    initLcd();
    initRtc();    
    initTimer0();
    initAdc();

    display[0] = 'M';
    char *modeStr = intToString(mode);
    display[1] = modeStr[0];
    free(modeStr);

    __bis_SR_register(LPM3_bits | GIE);           // Enter LPM3, enable interrupts
    __no_operation();                             // For debugger
}

void checkInputs() {
    static int checkTime = 0;
    checkTime++;
    if((checkTime % 10) == 0 && checkTime > 0) {
        checkTime = 0;
        
        showChar(display[0], pos1);
        showChar(display[1], pos2);
        showChar(display[2], pos3);
        showChar(display[3], pos4);
        showChar(display[4], pos5);
        showChar(display[5], pos6);
    }

    int8_t s1 = debounce_switch1(); // check if s1 is pressed
    int8_t s2 = debounce_switch2(); // check if s2 is pressed

    // if both buttons pressed, reset
    // if(bit_is_clear(P1IN, BIT2) & bit_is_clear(P2IN, BIT6)) {
    if(s2) {
        mode++;
        if(mode >= 3) {
            mode = 0;
        }

        runMotors = 0;

        P1OUT &= ~BIT0;
        P4OUT &= ~BIT0;

        // stop motor
        TA0CCR1 = 0;
        P8OUT &= ~BIT0;
        P8OUT &= ~BIT1;

        TA0CCR2 = 0;
        P8OUT &= ~BIT2;
        P8OUT &= ~BIT3;
    }

    display[0] = 'M';
    char *modeStr = intToString(mode);
    display[1] = modeStr[0];
    free(modeStr);

    if(mode == 0) { // start/stop robot
        static int forwardTime = 0;
        static int backTime = 0;

        if(s1) {
            runMotors = !runMotors;

            if(runMotors) {
                runForwardStart = 1;
                forwardTime = 0;

                // go forwards
                TA0CCR1 = 700;
                P8OUT |= BIT1;
                P8OUT &= ~BIT0;

                TA0CCR2 = 700;
                P8OUT |= BIT3;
                P8OUT &= ~BIT2;
            } else {
                runForwardStart = 0;
                forwardTime = 0;
            }
        }

        if(runMotors) {
            forwardTime++;
            backTime++;

            // read in the light sensor input
            int frontLight = !(P1IN & BIT3); //debounce_front_light();
            // read in ir detection
            int irDetect = !(P1IN & BIT4);

            display[3] = 'G';
            display[4] = 'O';


            display[5] = frontLight ? '1' : '0';

            P1OUT &= ~BIT0;
            P4OUT |= BIT0;
            
            if(runForwardStart) {
                if((forwardTime % (550)) == 0 && forwardTime > 0) {
                    runForwardStart = 0;    // roughly 1.5s
                    forwardTime = 0;
                }
            } else if(frontLight) {
                backingUp = 1;
                backTime = 0;

                // go backwards
                TA0CCR1 = 700;
                P8OUT |= BIT0;
                P8OUT &= ~BIT1;

                TA0CCR2 = 700;
                P8OUT |= BIT2;
                P8OUT &= ~BIT3;
            } else if((backTime % (550)) == 0 && backTime > 0) {
                backingUp = 0;    // roughly 1.5s
                backTime = 0;
            }

            if(runForwardStart || backingUp) {
                P5OUT |= BIT3;
            } else { // if not backing up, then continue strategy mode
                P5OUT &= ~BIT3;

                // play out strategy mode
                if(strategyMode == 0) {
                    if(irDetect) {
                        P5OUT |= BIT2;

                        // go forwards
                        TA0CCR1 = motorSpeed*1.5;
                        P8OUT |= BIT1;
                        P8OUT &= ~BIT0;

                        TA0CCR2 = motorSpeed*1.5;
                        P8OUT |= BIT3;
                        P8OUT &= ~BIT2;
                    } else {
                        P5OUT &= ~BIT2;

                        // spin clockwise slowly
                        TA0CCR1 = 700;
                        P8OUT |= BIT1;
                        P8OUT &= ~BIT0;

                        TA0CCR2 = 700;
                        P8OUT |= BIT2;
                        P8OUT &= ~BIT3;
                    }
                } else if(strategyMode == 1) {

                } else if(strategyMode == 2) {

                }
            }
        } else {
            display[3] = 'S';
            display[4] = 'T';
            display[5] = 'P';

            P1OUT |= BIT0;
            P4OUT &= ~BIT0;

            TA0CCR1 = 0;
            P8OUT &= BIT1;
            P8OUT &= ~BIT0;

            TA0CCR2 = 0;
            P8OUT &= BIT3;
            P8OUT &= ~BIT2;
        }
    } else if(mode == 1) {  // update motor speed
        // read the potentiometer to set motor speed
        ADCCTL0 |= ADCENC | ADCSC;                              // Sampling and conversion start
        while ((ADCCTL1 & ADCBUSY));                            // Wait until ADC12 is ready
        motorSpeed = ADCMEM0;                                   // store result of ADC operation
        motorSpeed = map(motorSpeed, 0, 1023, 0, 1000);         // constrain adc result
        // motorSpeed = 999;

        int displaySpeed = motorSpeed / 10;
        
        int digit1 = displaySpeed % 10;
        char* digit1Str = intToString(digit1);
        display[5] = digit1Str[0];
        free(digit1Str);
        displaySpeed /= 10;

        int digit2 = displaySpeed % 10;
        char* digit2Str = intToString(digit2);
        display[4] = digit2Str[0];
        free(digit2Str);
        displaySpeed /= 10;

        if(motorSpeed / 10 < 10) {
            display[4] = ' ';
        }

        int digit3 = displaySpeed % 10;
        char* digit3Str = intToString(digit3);
        display[3] = digit3Str[0];
        free(digit3Str);

        if(motorSpeed / 10 < 100) {
            display[3] = ' ';
        }
    } else if(mode == 2) {
        if(s1) {
            strategyMode++;
        }

        // 3 strategy modes
        if(strategyMode > 2) {
            strategyMode = 0;
        }

        display[3] = 'S';
        display[4] = 'T';
        char *strategyModeStr = intToString(strategyMode);
        display[5] = strategyModeStr[0];
        free(strategyModeStr);
    }

}

// RTC interrupt service routine
#pragma vector=RTC_VECTOR
__interrupt void RTC_ISR(void)
{
    switch(__even_in_range(RTCIV, RTCIV_RTCIF))
    {
        case  RTCIV_NONE: break;            // No interrupt
        case  RTCIV_RTCIF:                  // RTC Overflow
            checkInputs();
            // P1OUT ^= BIT0;
            break;
        default: break;
    }
}

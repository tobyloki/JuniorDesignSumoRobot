#pragma once

#define _BV(n) (1 << n)
#define bit_is_set(sfr, bit) (sfr & bit)
#define bit_is_clear(sfr, bit) (!(sfr & bit))

char *intToString(int val);
long map(long x, long in_min, long in_max, long out_min, long out_max);



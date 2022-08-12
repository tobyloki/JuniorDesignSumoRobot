#include "utility.h"
#include <stdio.h>
#include <stdlib.h>

char *intToString(int val) {
    char *str = malloc(10 * sizeof(char));
    sprintf(str, "%d", val);
    return str;
}

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


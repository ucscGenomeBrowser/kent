/* asmLong - some run time routines for manipulating longs for
 * processors that don't directly support longs */

#include "common.h"
#include "../compiler/pfPreamble.h"


long long _pfLongMul(long long a, long long b)
{
return a*b;
}

long long _pfLongDiv(long long p, long long q)
{
return p/q;
}

long long _pfLongMod(long long p, long long q)
{
return p%q;
}

long long _pfLongShiftRight(long long a, long long b)
{
return a>>b;
}


void _printByte(_pf_Byte x)
{
printf("%d\n", x);
}

void _printShort(_pf_Short x)
{
printf("%d\n", x);
}

void _printInt(_pf_Int x)
{
printf("%d\n", x);
}

void _printLong(long long x)
{
printf("%lld\n", x);
}

void _printFloat(_pf_Float x)
{
printf("%f\n", x);
}

void _printDouble(_pf_Double x)
{
printf("%f\n", x);
}


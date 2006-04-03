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

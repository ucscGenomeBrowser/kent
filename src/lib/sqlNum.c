/* sqlnum.c - Routines to convert from ascii to integer
 * representation of numbers. */

#include "common.h"
#include "sqlNum.h"

unsigned sqlUnsigned(char *s)
/* Convert series of digits to unsigned integer about
 * twice as fast as atoi (by not having to skip white 
 * space or stop except at the null byte.) */
{
unsigned res = 0;
char c;

res = *s - '0';
while ((c = *(++s)) != 0)
    {
    res *= 10;
    res += c - '0';
    }
return res;
}

int sqlSigned(char *s)
/* Convert string to signed integer.  Unlike atol assumes 
 * all of string is number. */
{
int res = 0;
boolean neg = FALSE;
char c;

if ((c = *s) == '-')
    {
    neg = TRUE;
    c = *(++s);
    }
res = c - '0';
while ((c = *(++s)) != 0)
    {
    res *= 10;
    res += c - '0';
    }
if (neg)
    res = -res;
return res;
}


/* sqlnum.c - Routines to convert from ascii to integer
 * representation of numbers. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "sqlNum.h"

unsigned sqlUnsigned(char *s)
/* Convert series of digits to unsigned integer about
 * twice as fast as atoi (by not having to skip white 
 * space or stop except at the null byte.) */
{
unsigned res = 0;
char *p = s;
char c;

while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
if (c != '\0')
    errAbort("invalid unsigned number: \"%s\"", s);
return res;
}

int sqlSigned(char *s)
/* Convert string to signed integer.  Unlike atol assumes 
 * all of string is number. */
{
int res = 0;
char *p = s;
char c;

if (*p == '-')
    p++;
while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
if (c != '\0')
    errAbort("invalid signed number: \"%s\"", s);
if (*s == '-')
    return -res;
else
    return res;
}

off_t sqlOffset(char *s)
/* Convert string to an off_t.  Unlike atol assumes all of string is
 * number. */
{
off_t res = 0;
char *p = s;
char c;

if (*p == '-')
    p++;
while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
if (c != '\0')
    errAbort("invalid signed number: \"%s\"", s);
if (*s == '-')
    return -res;
else
    return res;
}

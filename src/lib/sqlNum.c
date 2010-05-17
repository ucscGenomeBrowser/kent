/* sqlnum.c - Routines to convert from ascii to integer
 * representation of numbers. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: sqlNum.c,v 1.17 2010/05/17 18:01:41 hiram Exp $";

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
    errAbort("invalid unsigned integer: \"%s\"", s);
return res;
}

unsigned long sqlUnsignedLong(char *s)
/* Convert series of digits to unsigned long about
 * twice as fast as atol (by not having to skip white 
 * space or stop except at the null byte.) */
{
unsigned long res = 0;
char *p = s;
char c;

while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
if (c != '\0')
    errAbort("invalid unsigned integer: \"%s\"", s);
return res;
}

int sqlSigned(char *s)
/* Convert string to signed integer.  Unlike atol assumes 
 * all of string is number. */
{
int res = 0;
char *p, *p0 = s;

if (*p0 == '-')
    p0++;
p = p0;
while ((*p >= '0') && (*p <= '9'))
    {
    res *= 10;
    res += *p - '0';
    p++;
    }
/* test for invalid character, empty, or just a minus */
if ((*p != '\0') || (p == p0))
    errAbort("invalid signed integer: \"%s\"", s);
if (*s == '-')
    return -res;
else
    return res;
}

long long sqlLongLong(char *s)
/* Convert string to a long long.  Unlike atol assumes all of string is
 * number. */
{
long long res = 0;
char *p, *p0 = s;

if (*p0 == '-')
    p0++;
p = p0;
while ((*p >= '0') && (*p <= '9'))
    {
    res *= 10;
    res += *p - '0';
    p++;
    }
/* test for invalid character, empty, or just a minus */
if ((*p != '\0') || (p == p0))
    errAbort("invalid signed integer: \"%s\"", s);
if (*s == '-')
    return -res;
else
    return res;
}

float sqlFloat(char *s)
/* Convert string to a float.  Assumes all of string is number
 * and aborts on an error. */
{
char* end;
/*	used to have an ifdef here to use strtof() but that doesn't
 *	actually exist on all systems and since strtod() does, may as
 *	well use it since it will do the job here.
 */
float val = (float) strtod(s, &end);

if ((end == s) || (*end != '\0'))
    errAbort("invalid float: %s", s);
return val;
}

double sqlDouble(char *s)
/* Convert string to a double.  Assumes all of string is number
 * and aborts on an error. */
{
char* end;
double val = strtod(s, &end);

if ((end == s) || (*end != '\0'))
    errAbort("invalid double: %s", s);
return val;
}

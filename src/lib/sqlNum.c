/* sqlnum.c - Routines to convert from ascii to integer
 * representation of numbers. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "sqlNum.h"

/* The sql<Type>InList functions allow for fast thread-safe processing of dynamic arrays in sqlList */


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
--p;
/* test for invalid character or empty */
if ((c != '\0') || (p == s))
    errAbort("invalid unsigned integer: \"%s\"", s);
return res;
}

unsigned sqlUnsignedInList(char **pS)
/* Convert series of digits to unsigned integer about
 * twice as fast as atoi (by not having to skip white 
 * space or stop except at the null byte.) 
 * All of string is number. Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */
{
char *s = *pS;
unsigned res = 0;
char *p = s;
char c;

while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
--p;
if (!(c == '\0' || c == ',') || (p == s))
    {
    char *e = strchr(s, ',');
    if (e)
	*e = 0;
    errAbort("invalid unsigned integer: \"%s\"", s);
    }
*pS = p;
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
--p;
if ((c != '\0') || (p == s))
    errAbort("invalid unsigned long: \"%s\"", s);
return res;
}

unsigned long sqlUnsignedLongInList(char **pS)
/* Convert series of digits to unsigned long about
 * twice as fast as atol (by not having to skip white 
 * space or stop except at the null byte.) 
 * All of string is number. Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */
{
char *s = *pS;
unsigned long res = 0;
char *p = s;
char c;

while (((c = *(p++)) >= '0') && (c <= '9'))
    {
    res *= 10;
    res += c - '0';
    }
--p;
if (!(c == '\0' || c == ',') || (p == s))
    {
    char *e = strchr(s, ',');
    if (e)
	*e = 0;
    errAbort("invalid unsigned long: \"%s\"", s);
    }
*pS = p;
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

int sqlSignedInList(char **pS)
/* Convert string to signed integer.  Unlike atol assumes 
 * all of string is number. Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */
{
char *s = *pS;
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
if (!(*p == '\0' || *p == ',') || (p == p0))
    {
    char *e = strchr(s, ',');
    if (e)
	*e = 0;
    errAbort("invalid signed integer: \"%s\"", s);
    }
*pS = p;
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
    errAbort("invalid signed long long: \"%s\"", s);
if (*s == '-')
    return -res;
else
    return res;
}

long long sqlLongLongInList(char **pS)
/* Convert string to a long long.  Unlike atol, assumes 
 * all of string is number. Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */
{
char *s = *pS;
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
if (!(*p == '\0' || *p == ',') || (p == p0))
    {
    char *e = strchr(s, ',');
    if (e)
	*e = 0;
    errAbort("invalid signed long long: \"%s\"", s);
    }
*pS = p;
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

float sqlFloatInList(char **pS)
/* Convert string to a float.  Assumes all of string is number
 * and aborts on an error. 
 * Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */
{
char *s = *pS;
char* end;
/*	used to have an ifdef here to use strtof() but that doesn't
 *	actually exist on all systems and since strtod() does, may as
 *	well use it since it will do the job here.
 */
float val = (float) strtod(s, &end);

if ((end == s) || !(*end == '\0' || *end == ','))
    {
    char *e = strchr(s, ',');
    if (e)
	*e = 0;
    errAbort("invalid float: %s", s);
    }
*pS = end;
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

double sqlDoubleInList(char **pS)
/* Convert string to a double.  Assumes all of string is number
 * and aborts on an error.
 * Number may be delimited by a comma.
 * Returns the position of the delimiter or the terminating 0. */
{
char *s = *pS;
char* end;
double val = strtod(s, &end);

if ((end == s) || !(*end == '\0' || *end == ','))
    {
    char *e = strchr(s, ',');
    if (e)
        *e = 0;
    errAbort("invalid double: %s", s);
    }
*pS = end;
return val;
}


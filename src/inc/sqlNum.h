/* sqlNum.h - routines to convert from ascii to
 * unsigned/integer a bit more quickly than atoi. 
 * Called sqlNum because it was first developed for use with
 * SQL databases, which tend to require a lot of conversion from
 * string to binary representation of numbers. In particular the
 * code generator AutoSQL puts in lots of calls to these routines
 * into it's parsers.  Other parser in the source tree have come
 * to use these too though since they are fast and have good error
 * checking.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef SQLNUM_H
#define SQLNUM_H

/* get off_t */
#include <sys/types.h>

unsigned sqlUnsigned(char *s);
/* Convert series of digits to unsigned integer about
 * twice as fast as atoi (by not having to skip white 
 * space or stop except at the null byte.) */

unsigned sqlUnsignedOrError(char *s, char *format, ...)
 /* like sqlUnsigned, with an optional error message as a printf-style vararg */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

unsigned sqlUnsignedInList(char **pS);
/* Convert series of digits to unsigned integer about
 * twice as fast as atoi (by not having to skip white 
 * space or stop except at the null byte.) 
 * All of string is number. Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */

unsigned long sqlUnsignedLong(char *s);
/* Convert series of digits to unsigned long about
 * twice as fast as atol (by not having to skip white 
 * space or stop except at the null byte.) */

unsigned long sqlUnsignedLongInList(char **pS);
/* Convert series of digits to unsigned long about
 * twice as fast as atol (by not having to skip white 
 * space or stop except at the null byte.) 
 * All of string is number. Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */

int sqlSigned(char *s);
/* Convert string to signed integer.  Unlike atol assumes 
 * all of string is number. */

int sqlSignedInList(char **pS);
/* Convert string to signed integer.  Unlike atol assumes 
 * all of string is number. Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */

long long sqlLongLong(char *s);
/* Convert string to a long long.  Unlike atol assumes all of string is
 * number. */

long long sqlLongLongInList(char **pS);
/* Convert string to a long long.  Unlike atol, assumes 
 * all of string is number. Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */

float sqlFloat(char *s);
/* Convert string to a float.  Assumes all of string is number
 * and aborts on an error. */

float sqlFloatInList(char **pS);
/* Convert string to a float.  Assumes all of string is number
 * and aborts on an error. 
 * Number may be delimited by a comma. 
 * Returns the position of the delimiter or the terminating 0. */

double sqlDouble(char *s);
/* Convert string to a double.  Assumes all of string is number
 * and aborts on an error. */

double sqlDoubleInList(char **pS);
/* Convert string to a double.  Assumes all of string is number
 * and aborts on an error.
 * Number may be delimited by a comma.
 * Returns the position of the delimiter or the terminating 0. */

#endif /* SQLNUM_H */
 

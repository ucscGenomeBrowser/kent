/* sqlNum.h - routines to convert from ascii to
 * unsigned/integer a bit more quickly than atoi. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef SQLNUM_H
#define SQLNUM_H

unsigned sqlUnsigned(char *s);
/* Convert series of digits to unsigned integer about
 * twice as fast as atoi (by not having to skip white 
 * space or stop except at the null byte.) */

int sqlSigned(char *s);
/* Convert string to signed integer.  Unlike atol assumes 
 * all of string is number. */


#endif /* SQLNUM_H */
 

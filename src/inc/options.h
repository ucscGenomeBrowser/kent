/* Stuff to process options out of command line. */

#ifndef OPTIONS_H

#ifndef HASH_H
#include "hash.h"
#endif

char *optionVal(char *name, char *defaultVal);
/* Return named option if in options hash, otherwise default. */

int optionInt(char *name, int defaultVal);
/* Return integer value of named option, or default value
 * if not set. */

float optionFloat(char *name, float defaultVal);
/* Return float value of named option or default. */

boolean optionExists(char *name);
/* Return TRUE if option has been set. */

void optionHash(int *pArgc, char *argv[]);
/* Read options in command line into options hash.   
 * Options come in three forms:
 *      -option         words starting with dash
 *      option=val      words with = in the middle
 *      -option=val     combining the two.
 * The resulting hash will be keyed by the option name with the val
 * string for value.  For '-option' types the value is 'on'. */

void optionHashSome(int *pArgc, char *argv[], boolean justFirst);
/* Set up option hash from command line, optionally only adding
 * up to first non-optional word. */

struct hash *optionParseIntoHash(int *pArgc, char *argv[], boolean justFirst);
/* Read options in argc/argv into a hash of your own choosing. */

#endif /* OPTIONS_H */


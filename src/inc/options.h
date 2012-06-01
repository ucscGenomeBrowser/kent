/* Stuff to process options out of command line. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef OPTIONS_H
#define OPTIONS_H

#include "common.h"

/* Types for options */
#define OPTION_BOOLEAN    0x01
#define OPTION_STRING     0x02
#define OPTION_INT        0x04
#define OPTION_FLOAT      0x10
#define OPTION_LONG_LONG  0x20
#define OPTION_MULTI      0x40
#define OPTION_DOUBLE	  0x80

/* Mask for option types (excluding OPTION_MULTI) */
#define OPTION_TYPE_MASK (OPTION_BOOLEAN|OPTION_STRING|OPTION_INT|OPTION_FLOAT|OPTION_LONG_LONG|OPTION_DOUBLE)

struct optionSpec
/* Specification of a single option.  An array of these are passed
 * to optionInit() to validate options. */
{
    char *name;      /* option name */
    unsigned flags;  /* Flags for option, specifies types */
};

char *optionVal(char *name, char *defaultVal);
/* Return named option if in options hash, otherwise default. */

int optionInt(char *name, int defaultVal);
/* Return integer value of named option, or default value
 * if not set. */

long long optionLongLong(char *name, long long defaultVal);
/* Return long long value of named option, or default value
 * if not set. */

float optionFloat(char *name, float defaultVal);
/* Return floating point value or default value if not set. */

struct slName *optionMultiVal(char *name, struct slName *defaultVal);
/* Returns a list of the values assocated with a named option if in options hash, otherwise default. */

double optionDouble(char *name, double defaultVal);
/* Return double value or default value if not set */

boolean optionExists(char *name);
/* Return TRUE if option has been set. */

void optionMustExist(char *name);
/* Abort if option has not been set. */

void optionInit(int *pArgc, char *argv[], struct optionSpec *optionSpecs);
/* Read options in command line into options hash.
 * Options come in three forms:
 *      -option         words starting with dash
 *      option=val      words with = in the middle
 *      -option=val     combining the two.
 * The resulting hash will be keyed by the option name with the val
 * string for value.  For '-option' types the value is 'on'.
 * The words in argv are parsed in assending order.  If a word of
 * "--" is encountered, argument parsing stops.
 * If optionSpecs is not NULL, it is an array of optionSpec that are
 * used to validate the options.  An option must exist in the array
 * and the value must be convertable to the type specified in flags.
 * Boolean options have must no value, all other options must have one.
 * Array is terminated by a optionSpec with a NULL name.
 * If array NULL, no validation is done.
 */

void optionHash(int *pArgc, char *argv[]);
/* Read options in command line into options hash.   
 * Options come in three forms:
 *      -option         words starting with dash
 *      option=val      words with = in the middle
 *      -option=val     combining the two.
 * The resulting hash will be keyed by the option name with the val
 * string for value.  For '-option' types the value is 'on'.
 * The words in argv are parsed in assending order.  If a word of
 * "--" is encountered, argument parsing stops. */

void optionHashSome(int *pArgc, char *argv[], boolean justFirst);
/* Set up option hash from command line, optionally only adding
 * up to first non-optional word. */

struct hash *optionParseIntoHash(int *pArgc, char *argv[], boolean justFirst);
/* Read options in argc/argv into a hash of your own choosing. */

void optionFree();
/* free the option hash */

#endif /* OPTIONS_H */


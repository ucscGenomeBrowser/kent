/* Options.c - stuff to handle command line options.
 * This is smaller and more flexible than the cgiSpoof
 * routines we used to use - though cgiSpoof is still the
 * method of choice for actual CGI routine. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: options.c,v 1.12 2003/07/08 05:58:41 kent Exp $";

#ifdef MACHTYPE_alpha
    #define strtoll strtol
#endif

/* mask for type in optionSpec.flags */
#define OPTION_TYPE_MASK (OPTION_BOOLEAN|OPTION_STRING|OPTION_INT|OPTION_FLOAT|OPTION_LONG_LONG)

static void validateOption(char *name, char *val, struct optionSpec *optionSpecs)
/* validate an option against a list of values */
{
struct optionSpec *optionSpec = optionSpecs;
char *valEnd;

while ((optionSpec->name != NULL) && !sameString(optionSpec->name, name))
    optionSpec++;

if (optionSpec->name == NULL)
    errAbort("-%s is not a valid option", name);

switch (optionSpec->flags & OPTION_TYPE_MASK) {
case OPTION_BOOLEAN:
    if (val != NULL)
        errAbort("boolean option -%s must not have value", name);
    break;
case OPTION_STRING:
    if (val == NULL)
        errAbort("string option -%s must have a value", name);
    break;
case OPTION_INT:
    if (val == NULL)
        errAbort("int option -%s must have a value", name);
    strtol(val, &valEnd, 10);
    if ((*val == '\0') || (*valEnd != '\0'))
        errAbort("value of -%s is not a valid integer: \"%s\"",
                 name, val);
    break;
case OPTION_LONG_LONG:
    if (val == NULL)
        errAbort("int option -%s must have a value", name);
    strtoll(val, &valEnd, 10);
    if ((*val == '\0') || (*valEnd != '\0'))
        errAbort("value of -%s is not a valid long long: \"%s\"",
                 name, val);
    break;
case OPTION_FLOAT:
    if (val == NULL)
        errAbort("float option -%s must have a value", name);
    strtod(val, &valEnd);
    if ((*val == '\0') || (*valEnd != '\0'))
        errAbort("value of -%s is not a valid float: \"%s\"",
                 name, val);
    break;
default:
    errAbort("bug: invalid type in optionSpec for %s", optionSpec->name);
}
}

static boolean parseAnOption(struct hash *hash, char *arg, struct optionSpec *optionSpecs)
/* Parse a single option argument and add to the hash, validating if
 * optionSpecs is not NULL.  Return TRUE if it's arg is an option argument
 * FALSE if it's not.
 */
{
char *name, *val;
char *eqPtr = strchr(arg, '=');

if (!((eqPtr != NULL) || (arg[0] == '-')))
    return FALSE;  /* not an option */

name = arg;
if (name[0] == '-')
    name++;
if (eqPtr != NULL)
    {
    *eqPtr = '\0';
    val = eqPtr+1;
    }
else
    val = NULL;

if (optionSpecs != NULL)
    validateOption(name, val, optionSpecs);
if (val == NULL)
    val = "on";
hashAdd(hash, name, val);
if (eqPtr != NULL)
    *eqPtr = '=';
return TRUE;
}


static struct hash *parseOptions(int *pArgc, char *argv[], boolean justFirst,
                                 struct optionSpec *optionSpecs)
/* Parse and optionally validate options */
{
int i, origArgc, newArgc = 1;
char **rdPt = argv+1, **wrPt = argv+1;
struct hash *hash = newHash(6);

origArgc = *pArgc;

/* parse arguments */
for (i=1; i<origArgc; ++i)
    {
    if (sameString(*rdPt, "--"))
        {
        rdPt++;
        break;
        }
    if (!parseAnOption(hash, *rdPt, optionSpecs))
        {
        /* not an option */
        if (justFirst)
            break;
        *wrPt++ = *rdPt;
        newArgc++;
        }
    rdPt++;
    }

/* copy any remaining positional args */
for (; i<origArgc; ++i)
    {
    *wrPt++ = *rdPt++;
    newArgc++;
    }

*pArgc = newArgc;
*wrPt = NULL; 
return hash;
}

struct hash *optionParseIntoHash(int *pArgc, char *argv[], boolean justFirst)
/* Read options in command line (only up to first real argument) into
 * options hash.   Options come in three forms:
 *      -option         words starting with dash
 *      option=val      words with = in the middle
 *      -option=val     combining the two.
 * The resulting hash will be keyed by the option name with the val
 * string for value.  For '-option' types the value is 'on'. */
{
return parseOptions(pArgc, argv, justFirst, NULL);
}

static struct hash *options = NULL;

void optionHashSome(int *pArgc, char *argv[], boolean justFirst)
/* Set up option hash from command line, optionally only adding
 * up to first non-optional word. */
{
if (options == NULL)
    options = parseOptions(pArgc, argv, justFirst, NULL);
}

void optionHash(int *pArgc, char *argv[])
/* Read options in command line into options hash.   
 * Options come in three forms:
 *      -option         words starting with dash
 *      option=val      words with = in the middle
 *      -option=val     combining the two.
 * The resulting hash will be keyed by the option name with the val
 * string for value.  For '-option' types the value is 'on'. */
{
optionHashSome(pArgc, argv, FALSE);
}

void optionInit(int *pArgc, char *argv[], struct optionSpec *optionSpecs)
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
 * Boolean options must no value, all other options must have one.
 * Array is terminated by a optionSpec with a NULL name.
 * If array NULL, no validation is done.
 */
{
if (options == NULL)
    options = parseOptions(pArgc, argv, FALSE, optionSpecs);
}

static char *optGet(char *name)
/* Lookup option name.  Complain if options hash not set. */
{
if (options == NULL)
    errAbort("optGet called before optionHash");
return hashFindVal(options, name);
}
 
char *optionVal(char *name, char *defaultVal)
/* Return named option if in options hash, otherwise default. */
{
char *ret = optGet(name);
if (ret == NULL)
     ret = defaultVal;
return ret;
}

int optionInt(char *name, int defaultVal)
/* Return integer value of named option, or default value
 * if not set. */
{
char *s = optGet(name);
char *valEnd;
int val;
if (s == NULL)
    return defaultVal;
if (sameString(s,"on"))
    return defaultVal;
val = strtol(s, &valEnd, 10);
if ((*s == '\0') || (*valEnd != '\0'))
    errAbort("value of -%s is not a valid integer: \"%s\"", name, s);
return val;
}

long long optionLongLong(char *name, long long defaultVal)
/* Return long long value of named option, or default value
 * if not set. */
{
char *s = optGet(name);
char *valEnd;
long long val;
if (s == NULL)
    return defaultVal;
if (sameString(s,"on"))
    return defaultVal;
val = strtoll(s, &valEnd, 10);
if ((*s == '\0') || (*valEnd != '\0'))
    errAbort("value of -%s is not a valid long long: \"%s\"", name, s);
return val;
}

float optionFloat(char *name, float defaultVal)
/* Return floating point value or default value if not set. */
{
char *s = optGet(name);
char *valEnd;
float val;
if (s == NULL)
    return defaultVal;

val = strtod(s, &valEnd);
if ((*s == '\0') || (*valEnd != '\0'))
    errAbort("value of -%s is not a valid float: \"%s\"", name, s);
return val;
}

boolean optionExists(char *name)
/* Return TRUE if option has been set. */
{
return optGet(name) != NULL;
}



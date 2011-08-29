/* Options.c - stuff to handle command line options.
 * This is smaller and more flexible than the cgiSpoof
 * routines we used to use - though cgiSpoof is still the
 * method of choice for actual CGI routines that want to
 * be tested from the command line. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "hash.h"
#include "verbose.h"
#include "options.h"
#include <limits.h>

static char const rcsid[] = "$Id: options.c,v 1.29 2009/12/02 19:10:38 kent Exp $";

#ifdef MACHTYPE_alpha
    #define strtoll strtol
#endif

/* mask for type in optionSpec.flags */
#define OPTION_TYPE_MASK (OPTION_BOOLEAN|OPTION_STRING|OPTION_INT|OPTION_FLOAT|OPTION_LONG_LONG|OPTION_DOUBLE)

static struct optionSpec commonOptions[] = {
   {"verbose", OPTION_INT},
   {NULL, 0},
};

static struct optionSpec *matchingOption(char *name, struct optionSpec *optionSpecs)
/* Go through spec table and return spec that matches name, or NULL
 * if none. */
{
while (optionSpecs->name != NULL)
    {
    if (sameString(optionSpecs->name, name))
        return optionSpecs;
    optionSpecs += 1;
    }
return NULL;
}

static void validateOption(char *name, char *val, struct optionSpec *optionSpecs)
/* validate an option against a list of values */
{
char *valEnd;
struct optionSpec *optionSpec = matchingOption(name, optionSpecs);
if (optionSpec == NULL)
    optionSpec = matchingOption(name, commonOptions);
if (optionSpec == NULL)
    errAbort("-%s is not a valid option", name);

long long discardMe = 0;
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
    discardMe = strtol(val, &valEnd, 10);
    if ((*val == '\0') || (*valEnd != '\0'))
        errAbort("value of -%s is not a valid integer: \"%s\"",
                 name, val);
    break;
case OPTION_LONG_LONG:
    if (val == NULL)
        errAbort("int option -%s must have a value", name);
    discardMe = strtoll(val, &valEnd, 10);
    if ((*val == '\0') || (*valEnd != '\0'))
        errAbort("value of -%s is not a valid long long: \"%s\"",
                 name, val);
    break;
case OPTION_FLOAT:
    if (val == NULL)
        errAbort("float option -%s must have a value", name);
    discardMe = (long long)strtod(val, &valEnd);
    if ((*val == '\0') || (*valEnd != '\0'))
        errAbort("value of -%s is not a valid float: \"%s\"",
                 name, val);
    break;
case OPTION_DOUBLE:
    if (val == NULL)
        errAbort("double option -%s must have a value", name);
    discardMe = (long long)strtod(val, &valEnd);
    if ((*val == '\0') || (*valEnd != '\0'))
        errAbort("value of -%s is not a valid double: \"%s\"",
                 name, val);
    break;
default:
    errAbort("bug: invalid type in optionSpec for %s", optionSpec->name);
}
}

static void parseMultiOption(struct hash *hash, char *name, char* val, struct optionSpec *spec)
/* process multiple instances of an option, requres that the optionSpec of the option */
{
struct slName *valList;
switch (spec->flags & OPTION_TYPE_MASK)
    {
    case OPTION_STRING:
        valList = hashFindVal(hash, name);
        if (valList == NULL)   /* first multi option */
            {
            valList = newSlName(val);
            hashAdd(hash, name, valList);
            }
        else
            {
            struct slName *el = newSlName(val);
            slAddTail(valList, el); /* added next multi option */
            }
        break;
    default:
        errAbort("UNIMPLEMENTED: multiple instances of a non-string option is not currently implemented");
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

/* A dash by itself is not an option.   It can mean
 * negative strand for some of the DNA oriented utilities. */
if (arg[0] == '-' && (arg[1] == 0 || isspace(arg[1])))
    return FALSE;

/* We treat this=that as an option only if the '=' happens before any non-alphanumeric
 * characters.  This lets us have URLs and SQL statements in the command line even though
 * they can have equals in them. */
if (eqPtr != NULL)
    {
    char *s, c;
    for (s=arg; s < eqPtr; ++s)
        {
	c = *s;
	if (c != '_' && c != '-' && !isalnum(c))
	    return FALSE;
	}
    }

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
if (optionSpecs == NULL)
    hashAdd(hash, name, val);
else
    {
    struct optionSpec *spec = matchingOption(name, optionSpecs);
    if (spec != NULL && (spec->flags & OPTION_MULTI))    /* process multiple instances of option */
        parseMultiOption(hash, name, val, spec);
    else
        hashAdd(hash, name, val);
    }

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
        i++;
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
static struct optionSpec *optionSpecification = NULL;

static void setOptions(struct hash *hash)
/* Set global options hash to hash, and also do processing
 * of log file and other common options. */
{
options = hash;
if (optionExists("verbose"))
    verboseSetLevel(optionInt("verbose", 0));
}

void optionHashSome(int *pArgc, char *argv[], boolean justFirst)
/* Set up option hash from command line, optionally only adding
 * up to first non-optional word. */
{
if (options == NULL)
    {
    struct hash *hash = parseOptions(pArgc, argv, justFirst, NULL);
    setOptions(hash);
    }
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

void optionFree()
/* free the option hash */
{
freeHash(&options);
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
    {
    struct hash *hash = parseOptions(pArgc, argv, FALSE, optionSpecs);
    setOptions(hash);
    optionSpecification = optionSpecs;
    }
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
char *ret;
/* if a optionSpec was used, make sure this option is not a multi option */
if(optionSpecification != NULL) {
    struct optionSpec *spec = matchingOption(name, optionSpecification);
    if(spec != NULL && (spec->flags & OPTION_MULTI))    
        errAbort("ERROR: optionVal cannot be used to get the value of an OPTION_MULTI");
}

ret = optGet(name);
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
long lval;
if (s == NULL)
    return defaultVal;
if (sameString(s,"on"))
    return defaultVal;
lval = strtol(s, &valEnd, 10);  // use strtol since strtoi does not exist
if ((*s == '\0') || (*valEnd != '\0'))
    errAbort("value of -%s is not a valid integer: \"%s\"", name, s);
if (lval > INT_MAX)
    errAbort("value of -%s is is too large: %ld, integer maximum is %d", name, lval, INT_MAX);
if (lval < INT_MIN)
    errAbort("value of -%s is is too small: %ld, integer minimum is %d", name, lval, INT_MIN);
return lval;
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

struct slName *optionMultiVal(char *name, struct slName *defaultVal)
/* Return named option if in options hash, otherwise default. */
{
struct slName *ret;
if(optionSpecification == NULL)
    errAbort("ERROR: optionMultiVal can only be used after optionInit is called "
             "with a non-NULL optionSpecs");

ret = hashFindVal(options, name);
if (ret == NULL)
     ret = defaultVal;
return ret;
}

double optionDouble(char *name, double defaultVal)
/* Return double value or default value if not set */
{
char *s = optGet(name);
char *valEnd;
double val;
if (s == NULL)
    return defaultVal;

val = strtod(s, &valEnd);
if ((*s == '\0') || (*valEnd != '\0'))
    errAbort("value of -%s is not a valid double: \"%s\"", name, s);
return val;
}

boolean optionExists(char *name)
/* Return TRUE if option has been set. */
{
return optGet(name) != NULL;
}

void optionMustExist(char *name)
/* Abort if option has not been set. */
{
if (optGet(name) == NULL)
    errAbort("Missing required command line flag %s", name);
}

/* Options.c - stuff to handle command line options.
 * This is smaller and more flexible than the cgiSpoof
 * routines we used to use - though cgiSpoof is still the
 * method of choice for actual CGI routine. */

#include "common.h"
#include "hash.h"
#include "options.h"

struct hash *optionParseIntoHash(int *pArgc, char *argv[], boolean justFirst)
/* Read options in command line (only up to first real argument) into
 * options hash.   Options come in three forms:
 *      -option         words starting with dash
 *      option=val      words with = in the middle
 *      -option=val     combining the two.
 * The resulting hash will be keyed by the option name with the val
 * string for value.  For '-option' types the value is 'on'. */
{
int i, origArgc, curArgc;
char **rdPt = argv+1, **wrPt = argv+1;
boolean gotReal = FALSE;
struct hash *hash = newHash(6);

origArgc = curArgc = *pArgc;

for (i=1; i<origArgc; ++i)
    {
    boolean startDash;
    if (gotReal)
        *wrPt++ = *rdPt++;
    else
        {
	char *name = *rdPt++;
	boolean startDash = (name[0] == '-');
	char *eqPos = strchr(name, '=');
	if (startDash || eqPos != NULL)
	    {
	    char *val;
	    if (startDash)
	        ++name;
	    if (eqPos != NULL)
	        {
		val = eqPos+1;
		*eqPos = 0;
		}
	    else
	        val = "on";
	    hashAdd(hash, name, val);
	    curArgc -= 1;
	    }
	else
	    {
	    *wrPt++ = name;
	    if (justFirst)
		gotReal = TRUE;
	    }
	}
    }
*pArgc = curArgc;
return hash;
}

static struct hash *options;

void optionHashSome(int *pArgc, char *argv[], boolean justFirst)
/* Set up option hash from command line, optionally only adding
 * up to first non-optional word. */
{
if (options == NULL)
    options = optionParseIntoHash(pArgc, argv, justFirst);
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
if (s == NULL)
    return defaultVal;
if ((s[0] == '-' && isdigit(s[1])) || isdigit(s[0]))
    return atoi(s);
errAbort("option %s has to be integer valued", name);
return 0;
}

float optionFloat(char *name, float defaultVal)
/* Return floating point value or default value if not set. */
{
char *s = optGet(name);
if (s == NULL)
    return defaultVal;
if ((s[0] == '-' && isdigit(s[1])) || isdigit(s[0]))
    return atof(s);
errAbort("option %s has to be float valued", name);
return 0;
}

boolean optionExists(char *name)
/* Return TRUE if option has been set. */
{
return optGet(name) != NULL;
}



/* tagStormCheck - Check that a tagStorm conforms to a schema.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "sqlReserved.h"
#include "tagStorm.h"
#include "tagSchema.h"
#include "errAbort.h"
#include "obscure.h"
#include "csv.h"

int clMaxErr = 10;
boolean clSqlSymbols = FALSE;

int gErrCount = 0;  // Number of errors so far
struct hash *gReservedHash = NULL;  // SQL reserved words here

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormCheck - Check that a tagStorm conforms to a schema.\n"
  "usage:\n"
  "   tagStormCheck schema.txt tagStorm.txt\n"
  "The format of schema.txt is one line per allowed tag, with blank lines and lines\n"
  "starting with the # char treated as comments. Each line begins with the tag name\n"
  "and is followed by a symbol to indicate the tag type, one of\n"
  "    # - integer\n"
  "    %% - floating point number\n"
  "    $ - string\n"
  "Integer and floating point types may be followed by the min and max allowed vals if desired\n"
  "String types may be followed by a space-separated list of allowed values.  If the values\n"
  "themselves have spaces, they should be quoted.  Values may include * and ? wildcards.\n"
  "The tag names can also include wildcards.\n"
  "options:\n"
  "   -maxErr=N - maximum number of errors to report, defaults to %d\n"
  "   -sqlSymbols - if set, this will tolerate SQL reserved words in tag names\n"
  , clMaxErr);
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"maxErr", OPTION_INT},
   {"sqlSymbols", OPTION_BOOLEAN},
   {NULL, 0},
};

void reportError(char *fileName, int startLine, char *format, ...)
/* Report error and abort if there are too many errors. */
{
va_list args;
va_start(args, format);
if (++gErrCount > clMaxErr)
    noWarnAbort();
vaWarn(format, args);
warn(" in stanza starting line %d of %s", startLine, fileName);
}

static void rCheck(struct tagStanza *stanzaList, char *fileName, 
    struct slRef *wildList, struct hash *hash, struct slRef *requiredList)
/* Recurse through tagStorm */
{
struct tagStanza *stanza;
struct dyString *csvScratch = dyStringNew(0);
for (stanza = stanzaList; stanza != NULL; stanza = stanza->next)
    {
    struct slPair *pair;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
	{
	/* Break out tag and value */
	char *tag = pair->name;
	char *val = pair->val;

	/* Make sure val exists and is non-empty */
	if (isEmpty(val))
	    {
	    reportError(fileName, stanza->startLineIx, 
		"%s tag has no value", tag);
	    continue;
	    }

	/* Check against SQL reserved words */
	if (gReservedHash != NULL)
	    {
	    if (sqlReservedCheck(gReservedHash, tag))
	        {
		reportError(fileName, stanza->startLineIx, 
		    "%s in tag name is a SQL reserved word", tag);
		continue;
		}
	    }

	/* Find schema in hash or wildSchemaList */
	struct tagSchema *schema = hashFindVal(hash, tag);
	if (schema == NULL)
	    {
	    struct slRef *ref;
	    for (ref = wildList; ref != NULL; ref = ref->next)
		{
		struct tagSchema *s = ref->val;
		if (wildMatch(s->name, tag))
		    {
		    schema = s;
		    break;
		    }
		}
	    }

	/* Do checking on tag */
	if (schema == NULL)
	    reportError(fileName, stanza->startLineIx, "Unrecognized tag %s", tag);
	else
	    {
	    char type = schema->type;
	    char *pos = val;
	    char *oneVal;
	    while ((oneVal =csvParseNext(&pos, csvScratch)) != NULL)
		{
		if (type == '#')
		    {
		    char *end;
		    long long v = strtoll(oneVal, &end, 10);
		    if (end == oneVal || *end != 0)	// oneVal is not integer
			reportError(fileName, stanza->startLineIx, 
			    "Non-integer value %s for %s", oneVal, tag);
		    else if (v < schema->minVal)
			reportError(fileName, stanza->startLineIx, 
			    "Value %s too low for %s", oneVal, tag);
		    else if (v > schema->maxVal)
			 reportError(fileName, stanza->startLineIx, 
			    "Value %s too high for %s", oneVal, tag);
		    }
		else if (type == '%')
		    {
		    char *end;
		    double v = strtod(oneVal, &end);
		    if (end == oneVal || *end != 0)	// val is not just a floating point number
			reportError(fileName, stanza->startLineIx, 
			    "Non-numerical value %s for %s", oneVal, tag);
		    else if (v < schema->minVal)
			reportError(fileName, stanza->startLineIx, 
			    "Value %s too low for %s", oneVal, tag);
		    else if (v > schema->maxVal)
			reportError(fileName, stanza->startLineIx, 
			    "Value %s too high for %s", oneVal, tag);
		    }
		else
		    {
		    boolean gotMatch = FALSE;
		    struct slName *okVal;
		    for (okVal = schema->allowedVals; okVal != NULL; okVal = okVal->next)
			{
			if (wildMatch(okVal->name, oneVal))
			    {
			    gotMatch = TRUE;
			    break;
			    }
			}
		    if (!gotMatch)
			reportError(fileName, stanza->startLineIx, 
			    "Unrecognized value '%s' for tag %s", oneVal, tag);
		    }

		struct hash *uniqHash = schema->uniqHash;
		if (uniqHash != NULL)
		    {
		    if (hashLookup(uniqHash, oneVal))
			reportError(fileName, stanza->startLineIx, 
			    "Non-unique value '%s' for tag %s", oneVal, tag);
		    else
			hashAdd(uniqHash, oneVal, NULL);
		    }
		}
	    }
	}
    if (stanza->children)
	{
	rCheck(stanza->children, fileName, wildList, hash, requiredList);
	}
    else
	{
	struct slRef *ref;
	for (ref = requiredList; ref != NULL; ref = ref->next)
	    {
	    struct tagSchema *schema = ref->val;
	    if (tagFindVal(stanza, schema->name) == NULL)
	        reportError(fileName, stanza->startLineIx, 
		    "Missing required '%s' tag", schema->name);
	    }
	}
    }
dyStringFree(&csvScratch);
}

void tagStormCheck(char *schemaFile, char *tagStormFile)
/* tagStormCheck - Check that a tagStorm conforms to a schema.. */
{
/* Load up schema from file.  Make a hash of all non-wildcard
 * tags, and a list of wildcard tags. */
struct tagSchema *schema, *schemaList = tagSchemaFromFile(schemaFile);
struct slRef *wildSchemaList = NULL, *requiredSchemaList = NULL;

/* Split up schemaList into hash and wildSchemaList.  Calculate schemaSize */
struct hash *hash = hashNew(0);
int schemaSize = 0;
for (schema = schemaList; schema != NULL; schema = schema->next)
    {
    ++schemaSize;
    if (anyWild(schema->name))
	{
	refAdd(&wildSchemaList, schema);
	}
    else
	hashAdd(hash, schema->name, schema);
    if (schema->required != 0)
        refAdd(&requiredSchemaList, schema);
    }
slReverse(&wildSchemaList);
schemaList = NULL;

struct tagStorm *tagStorm = tagStormFromFile(tagStormFile);
rCheck(tagStorm->forest, tagStormFile, wildSchemaList, hash, requiredSchemaList);

if (gErrCount > 0)
    noWarnAbort();
else
    verbose(1, "No problems detected.\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clMaxErr = optionInt("maxErr", clMaxErr);
clSqlSymbols = optionExists("sqlSymbols");
if (!clSqlSymbols)
    gReservedHash = sqlReservedHash();
tagStormCheck(argv[1], argv[2]);
return 0;
}

/* tagStormCheck - Check that a tagStorm conforms to a schema.. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
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

static boolean matchAndExtractIndexes(char *dottedName, struct tagSchema *schema,
    struct dyString *scratch, char **retIndexes, boolean *retMatchEnd)
/* Dotted name is something like this.that.12.more.2.ok
 * The objArrayPieces is something like "this.that." ".more." ".notOk"
 * Crucially it holds at least two elements, some of which may be ""
 * This function will return TRUE if all but maybe the last of the objArrayPieces is
 * found in the dottedName.  If this is the case it will put something like .12.2 in
 * scratch and *retIndexes.  If the last one matches it will set *retMatchEnd.
 * Sort of a complex routine but it plays a key piece in checking required array
 * elements */
{
struct slName *objArrayPieces = schema->objArrayPieces;
dyStringClear(scratch);
struct slName *piece = objArrayPieces;
char *pos = dottedName;
boolean gotNum = FALSE;
for (;;)
    {
    /* Check to see if we match next piece, and return FALSE if not */
    char *pieceString = piece->name;
    int pieceLen = strlen(pieceString);
    if (pieceLen != 0 && memcmp(pieceString, pos, pieceLen) != 0)
        return FALSE;
    pos += pieceLen;

    /* Put the number into scratch with a leading dot separator. */
    int digits = tagSchemaDigitsUpToDot(pos);
    if (digits == 0)
        return FALSE;
    dyStringAppendC(scratch, '.');
    dyStringAppendN(scratch, pos, digits);
    pos += digits;
    gotNum = TRUE;

    /* Go to next piece,saving last piece for outside of the loop. */
    piece = piece->next;
    if (piece->next == NULL)
        break;
    }

/* One more special case, where last piece needs to agree on emptiness at least in
 * terms of matching */
if (isEmpty(piece->name) != isEmpty(pos))
    return FALSE;

/* Otherwise both have something.  We return true/false depending on whether it matches */
*retMatchEnd = (strcmp(piece->name, pos) == 0);
*retIndexes = scratch->string;
return gotNum;
}


struct arrayCheckHelper
/* An object to help check required items in an array. */
    {
    struct arrayCheckHelper *next;
    char *indexSig;   // Not allocated here.  This will be something like .1.2.1, a dot separated list of index vals
    char *prefix;   // Everything but last field
    boolean gotRequired;  // If true we got our required field
    };


void checkInAllArrayItems(char *fileName, struct tagStanza *stanza, struct tagSchema *schema,
    struct dyString *scratch)
/* Given a schema that is an array, make sure all items in array have the required value */
{
struct hash *hash = hashNew(4);
struct lm *lm = hash->lm;   // Memory short cut
struct arrayCheckHelper *helperList = NULL, *helper;
struct slPair *pair;
struct tagStanza *ancestor;
for (ancestor = stanza; ancestor != NULL; ancestor = ancestor->parent)
    {
    for (pair = ancestor->tagList; pair != NULL; pair = pair->next)
	{
	boolean fullMatch;
	char *indexes;
	if (matchAndExtractIndexes(pair->name, schema, scratch, &indexes, &fullMatch))
	    {
	    helper = hashFindVal(hash, indexes);
	    if (helper == NULL)
		{
		lmAllocVar(lm, helper);
		hashAddSaveName(hash, indexes, helper, &helper->indexSig);
		helper->prefix = lmCloneString(lm, pair->name);
		chopSuffix(helper->prefix);
		slAddHead(&helperList, helper);
		}
	    if (fullMatch)
		helper->gotRequired = TRUE;
	    }
	}
    }
if (helperList == NULL)
    reportError(fileName, stanza->startLineIx, "Missing required '%s' tag", schema->name);
else
    {
    for (helper = helperList; helper != NULL; helper = helper->next)
        {
	if (!helper->gotRequired)
	    reportError(fileName, stanza->startLineIx, 
		"Missing required '%s' tag for element %s", schema->name, helper->prefix);
	}
    }
hashFree(&hash);
}

static void rCheck(struct tagStanza *stanzaList, char *fileName, 
    struct slRef *wildList, struct hash *hash, struct slRef *requiredList,
    struct dyString *scratch)
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
	char *tag = tagSchemaFigureArrayName(pair->name, scratch);
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
	rCheck(stanza->children, fileName, wildList, hash, requiredList, scratch);
	}
    else
	{
	struct slRef *ref;
	for (ref = requiredList; ref != NULL; ref = ref->next)
	    {
	    struct tagSchema *schema = ref->val;
	    if (schema->objArrayPieces != NULL)  // It's an array, complex to handle, needs own routine
	        {
		checkInAllArrayItems(fileName, stanza, schema, scratch);
		}
	    else
		{
		if (tagFindVal(stanza, schema->name) == NULL)
		    reportError(fileName, stanza->startLineIx, 
			"Missing required '%s' tag", schema->name);
		}
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
struct dyString *scratch = dyStringNew(0);
rCheck(tagStorm->forest, tagStormFile, wildSchemaList, hash, requiredSchemaList, scratch);

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

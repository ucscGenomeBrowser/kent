/* tagSchema - help deal with tagStorm schemas */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "obscure.h"
#include "tagSchema.h"


static struct slName *makeObjArrayPieces(char *name)
/* Given something like this.[].that return a list of "this." ".that".  That is
 * return a list of all strings before between and after the []'s Other
 * examples:
 *        [] returns "" ""
 *        this.[] return "this." ""
 *        [].that returns "" ".that"
 *        this.[].that.and.[].more returns "this." ".that.and." ".more" */
{
struct slName *list = NULL;	// Result list goes here
char *pos = name;

/* Handle special case of leading "[]" */
if (startsWith("[]", name))
     {
     slNameAddHead(&list, "");
     pos += 2;
     }

char *aStart;
for (;;)
    {
    aStart = strchr(pos, '[');
    if (aStart == NULL)
        {
	slNameAddHead(&list, pos);
	break;
	}
    else
        {
	struct slName *el = slNameNewN(pos, aStart-pos);
	slAddHead(&list, el);
	pos = aStart + 2;
	}
    }
slReverse(&list);
return list;
}


struct tagSchema *tagSchemaFromFile(char *fileName)
/* Read in a tagSchema file */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
struct tagSchema *schema, *list = NULL;
while (lineFileNextReal(lf, &line))
    {
    /* Parse out name field and optional requirement field */
    char *name = nextWord(&line);
    char required = name[0];
    if (required == '!' || required == '^')
        {
	/* Allow req char to be either next to name, or separated  by space */
	if (name[1] != 0)
	    name = name+1;
	else
	    name = nextWord(&line);
	}
    else
        required = 0;

    /* Parse out type field */
    boolean isArray = FALSE;
    char *typeString = nextWord(&line);
    if (typeString == NULL)
        errAbort("truncated line %d of %s", lf->lineIx, lf->fileName);
    char type = typeString[0];
    if (type == '[')
	{
        isArray = TRUE;
	type = typeString[1];
	if (typeString[2] != ']')
	    errAbort("Expecting ']' in type field line %d of %s\n", lf->lineIx, lf->fileName);
	}

    /* Allocate schema struct and fill in several fields. */
    AllocVar(schema);
    schema->name = cloneString(name);
    schema->required = required;
    schema->type = type;
    schema->isArray = isArray;
    if (required == '^')
        schema->uniqHash = hashNew(0);

    /* Parse out rest of it depending on field type */
    if (type == '#' || type == '%') // numeric
        {
	char *minString = nextWord(&line);
	char *maxString = nextWord(&line);
	if (maxString == NULL)
	    {
	    schema->minVal = -BIGDOUBLE;
	    schema->maxVal = BIGDOUBLE;
	    }
	else
	    {
	    schema->minVal = sqlDouble(minString);
	    schema->maxVal = sqlDouble(maxString);
	    }
	}
    else if (type == '$')
        {
	char *val;

	while ((val = nextQuotedWord(&line)) != NULL)
	    {
	    slNameAddHead(&schema->allowedVals, val);
	    }
	slReverse(&schema->allowedVals);
	if (schema->allowedVals == NULL)
	    schema->allowedVals = slNameNew("*");
	}
    else
	{
        errAbort("Unrecognized type character %s line %d of %s", 
	    typeString, lf->lineIx, lf->fileName);
	}

    if (strchr(schema->name, '['))
        {
	schema->objArrayPieces = makeObjArrayPieces(schema->name);
	}
    slAddHead(&list, schema);
    }
slReverse(&list);
return list;
}

struct hash *tagSchemaHash(struct tagSchema *list)
/* Return a hash of tagSchemas keyed by name */
{
struct hash *hash = hashNew(9);
struct tagSchema *schema;
for (schema = list; schema != NULL; schema = schema->next)
    hashAdd(hash, schema->name, schema);
return hash;
}

int tagSchemaDigitsUpToDot(char *s)
/* Return number of digits if is all digit up to next dot or end of string.
 * Otherwise return 0.  A specialized function but used by a couple of tag
 * storm modules. */
{
char c;
int digitCount = 0;
for (;;)
    {
    c = *s++;
    if (c == '.' || c == 0)
       return digitCount;
    if (!isdigit(c))
       return FALSE;
    ++digitCount;
    }
}

static int nonDotSize(char *s)
/* Return number of chars up to next dot or end of string */
{
char c;
int size = 0;
for (;;)
    {
    c = *s++;
    if (c == '.' || c == 0)
       return size;
    ++size;
    }
}

char *tagSchemaFigureArrayName(char *tagName, struct dyString *scratch)
/* Return tagName modified to indicate the array
 * status. For names with .# in them substitute a '[]' for
 * the number.   Example:
 *      person.12.name becomes person.[].name
 *      animal.13.children.4.name becomes animal.[].children.[].name
 *      person.12.cars.1 becomes person.[].cars.[]
 */
{
char dot = '.';
char *s = tagName;
dyStringClear(scratch);

for (;;)
    {
    /* Check for end of string */
    char firstChar = *s;
    if (firstChar == 0)
        break;

    /* If leading char is a dot and if so skip it. */
    boolean startsWithDot = (firstChar == dot);
    if (startsWithDot)
       {
       dyStringAppendC(scratch, dot);
       ++s;
       }

    int numSize = tagSchemaDigitsUpToDot(s);
    if (numSize > 0)
        {
	dyStringAppend(scratch, "[]");
	s += numSize;
	}
    else
        {
	int partSize = nonDotSize(s);
	dyStringAppendN(scratch, s, partSize);
	s += partSize;
	}
    }
return scratch->string;
}

int tagSchemaParseIndexes(char *input, int indexes[], int maxIndexCount)
/* This will parse out something that looks like:
 *     this.array.2.subfield.subarray.3.name
 * into
 *     2,3   
 * Where input is what we parse,   indexes is an array maxIndexCount long
 * where we put the numbers. */
{
char dot = '.';
char *s = input;
int indexCount = 0;
int maxMinusOne = maxIndexCount - 1;
for (;;)
    {
    /* Check for end of string */
    char firstChar = *s;
    if (firstChar == 0)
        break;

    /* If leading char is a dot and if so skip it. */
    boolean startsWithDot = (firstChar == dot);
    if (startsWithDot)
       ++s;

    /* Figure out if is all digits up to next dot and if so how many digits there are */
    int numSize = tagSchemaDigitsUpToDot(s);
    if (numSize > 0)
        {
	if (indexCount > maxMinusOne)
	    errAbort("Too many array indexes in %s, maxIndexCount is %d",
		input, maxIndexCount);
	indexes[indexCount] = atoi(s);
	indexCount += 1;
	s += numSize;
	}
    else
        {
	int partSize = nonDotSize(s);
	s += partSize;
	}
    }
return indexCount;
}

char *tagSchemaInsertIndexes(char *bracketed, int indexes[], int indexCount,
    struct dyString *scratch)
/* Convert something that looks like:
 *     this.array.[].subfield.subarray.[].name and 2,3
 * into
 *     this.array.2.subfield.subarray.3.name
 * The scratch string holds the return value. */
{
char *pos = bracketed;
int indexPos = 0;
dyStringClear(scratch);

/* Handle special case of leading "[]" */
if (startsWith("[]", pos))
     {
     dyStringPrintf(scratch, "%d", indexes[indexPos]);
     ++indexPos;
     pos += 2;
     }

char *aStart;
for (;;)
    {
    aStart = strchr(pos, '[');
    if (aStart == NULL)
        {
	dyStringAppend(scratch, pos);
	break;
	}
    else
        {
	if (indexPos >= indexCount)
	    errAbort("Expecting %d '[]' in %s, got more.", indexCount, bracketed);
	dyStringAppendN(scratch, pos, aStart-pos);
	dyStringPrintf(scratch, "%d", indexes[indexPos]);
	++indexPos;
	pos = aStart + 2;
	}
    }
return scratch->string;
}


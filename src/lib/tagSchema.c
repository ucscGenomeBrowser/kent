/* tagSchema - help deal with tagStorm schemas */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "obscure.h"
#include "tagSchema.h"


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
    slAddHead(&list, schema);
    }
slReverse(&list);
return list;
}


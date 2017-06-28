/* tagSchemaToJson - Convert a tagStormSchema (such as used by tagStormCheck) to a JSON 
 * schema such as decribed in http://json-schema.org/. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagSchema.h"
#include "jsonWrite.h"

struct hash *gDescriptions = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagSchemaToJson - Convert a tagStormSchema (such as used by tagStormCheck) to a JSON\n"
  "schema such as decribed in http://json-schema.org/\n"
  "usage:\n"
  "   tagSchemaToJsonSchema tagSchema.in jsonSchema.json\n"
  "options:\n"
  "   -descriptions=twoCol.txt - first col is tag, second description\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"descriptions", OPTION_STRING},
   {NULL, 0},
};

void addRegExWildEquivalent(struct dyString *dy, char *s)
/* Convert wildcard string to a regular expression */
{
char c;
dyStringAppendC(dy, '^');
while ((c = *s++) != 0)
    {
    if (c == '?')
       dyStringAppendC(dy, '.');
    else if (c == '*')
       dyStringAppend(dy, ".*");
    else
       dyStringAppendC(dy, c);
    }
dyStringAppendC(dy, '$');
}

struct dyString*wildListToRegEx(struct slName *wildList)
/* Try to convert list of wildcard string to a regular expression */
{
struct dyString *dy = dyStringNew(0);
struct slName *wild;
for (wild = wildList; wild != NULL; wild = wild->next)
    {
    if (dy->stringSize != 0)
        dyStringAppendC(dy, '|');
    addRegExWildEquivalent(dy, wild->name);
    }
return dy;
}

struct hash *hashDescriptions(char *fileName)
/* Read in descriptions file keyed by first word */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
   {
   char *tag = nextWord(&line);
   char *val = trimSpaces(line);
   if (isEmpty(val))
       errAbort("Empty description line %d of %s", lf->lineIx, lf->fileName);
   hashAdd(hash, tag, cloneString(val));
   }
lineFileClose(&lf);
return hash;
}

void writeOneSchema(struct jsonWrite *jw, char *label, struct tagSchema *schema)
/* Write out label for one schema item */
{
jsonWriteObjectStart(jw, label);
if (gDescriptions != NULL)
    {
    char *description = hashFindVal(gDescriptions, schema->name);
    if (description != NULL)
        {
	jsonWriteString(jw, "description", description);
	}
    }
if (schema->type == '#')
    {
    jsonWriteString(jw, "type",  "integer");
    if (schema->minVal != -BIGDOUBLE)
	jsonWriteNumber(jw, "minimum", schema->minVal);
    if (schema->maxVal != BIGDOUBLE)
	jsonWriteNumber(jw, "maximum", schema->maxVal);
    }
else if (schema->type == '%')
    {
    jsonWriteString(jw, "type",  "number");
    if (schema->minVal != -BIGDOUBLE)
	jsonWriteDouble(jw, "minimum", schema->minVal);
    if (schema->maxVal != BIGDOUBLE)
	jsonWriteDouble(jw, "maximum", schema->maxVal);
    }
else if (schema->type == '$')
    {
    jsonWriteString(jw, "type",  "string");
    if (schema->allowedVals != NULL)
	{
	struct dyString *dy = wildListToRegEx(schema->allowedVals);
	jsonWriteString(jw, "pattern", dy->string);
	dyStringFree(&dy);
	}
    }
else
    errAbort("Unrecognized type %c for %s", schema->type, schema->name);
jsonWriteObjectEnd(jw);
}

void tagSchemaToJson(char *inTagSchema, char *outJson)
/* tagSchemaToJsonSchema - Convert a tagStormSchema (such as used by tagStormCheck) to a JSON 
 * schema such as decribed in http://json-schema.org/. */
{
struct tagSchema *schemaList = tagSchemaFromFile(inTagSchema);
struct jsonWrite *jw = jsonWriteNew();
jw->sep = '\n';

jsonWriteObjectStart(jw, NULL);

/* Write out bits that don't actually depend on our schema */
jsonWriteString(jw, "$schema", "http://json-schema.org/draft-04/schema#");
jsonWriteString(jw, "title", inTagSchema);
char text[1024];
safef(text, sizeof(text), "schema generate by tagSchemaToJson from %s", inTagSchema);
jsonWriteString(jw, "description", text);
jsonWriteString(jw, "type", "object");

/* Figure out if we have to do fixed properties and or pattern properties */
boolean needFixed = FALSE, needPattern = FALSE;
struct tagSchema *schema;
for (schema = schemaList; schema != NULL; schema = schema->next)
    {
    if (anyWild(schema->name))
        needPattern = TRUE;
    else
        needFixed = TRUE;
    }

if (needFixed)
    {
    jsonWriteObjectStart(jw, "properties");
    for (schema = schemaList; schema != NULL; schema = schema->next)
	{
	if (!anyWild(schema->name))
	    {
	    writeOneSchema(jw, schema->name, schema);
	    }

	}
    jsonWriteObjectEnd(jw);
    }
if (needPattern)
    {
    struct tagSchema *schema;
    jsonWriteObjectStart(jw, "patternProperties");
    for (schema = schemaList; schema != NULL; schema = schema->next)
	{
	if (anyWild(schema->name))
	    {
	    struct dyString *dy = dyStringNew(0);
	    addRegExWildEquivalent(dy, schema->name);
	    writeOneSchema(jw, dy->string, schema);
	    dyStringFree(&dy);
	    }
	}
    jsonWriteObjectEnd(jw);
    }

jsonWriteBoolean(jw, "additionalProperties", FALSE);

/* Figure out if any required fields  */
boolean needsRequired = FALSE;
for (schema = schemaList; schema != NULL; schema = schema->next)
    {
    if (schema->required != 0)
        {
	needsRequired = TRUE;
	break;
	}
    }

/* If have any requirements write out in array */
if (needsRequired)
    {
    jsonWriteListStart(jw, "required");
    for (schema = schemaList; schema != NULL; schema = schema->next)
	{
	if (schema->required != 0)
	    jsonWriteString(jw, NULL, schema->name);
	}
    jsonWriteListEnd(jw);
    }

jsonWriteObjectEnd(jw);

FILE *f = mustOpen(outJson, "w");
fprintf(f, "%s\n", jw->dy->string);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
char *descriptions = optionVal("descriptions", NULL);
if (descriptions != NULL)
   gDescriptions = hashDescriptions(descriptions);
tagSchemaToJson(argv[1], argv[2]);
return 0;
}

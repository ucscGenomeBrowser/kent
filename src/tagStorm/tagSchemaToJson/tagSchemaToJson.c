/* tagSchemaToJson - Convert a tagStormSchema (such as used by tagStormCheck) to a JSON 
 * schema such as decribed in http://json-schema.org/. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagSchema.h"
#include "jsonWrite.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagSchemaToJson - Convert a tagStormSchema (such as used by tagStormCheck) to a JSON\n"
  "schema such as decribed in http://json-schema.org/\n"
  "usage:\n"
  "   tagSchemaToJsonSchema tagSchema.in jsonSchema.json\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct dyString*wildListToRegEx(struct slName *wildList)
/* Try to convert list of wildcard string to a regular expression */
{
struct dyString *dy = dyStringNew(0);
struct slName *wild;
for (wild = wildList; wild != NULL; wild = wild->next)
    {
    if (dy->stringSize != 0)
        dyStringAppendC(dy, '|');
    char *s = wild->name;
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
    }
return dy;
}

void tagSchemaToJson(char *inTagSchema, char *outJson)
/* tagSchemaToJsonSchema - Convert a tagStormSchema (such as used by tagStormCheck) to a JSON 
 * schema such as decribed in http://json-schema.org/. */
{
struct tagSchema *schemaList = tagSchemaFromFile(inTagSchema);
uglyf("Read %d from %s\n", slCount(schemaList), inTagSchema);
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

jsonWriteObjectStart(jw, "properties");
struct tagSchema *schema;
for (schema = schemaList; schema != NULL; schema = schema->next)
    {
    jsonWriteObjectStart(jw, schema->name);
    jsonWriteString(jw, "description", "please_describe_me");
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
jsonWriteObjectEnd(jw);


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
tagSchemaToJson(argv[1], argv[2]);
return 0;
}

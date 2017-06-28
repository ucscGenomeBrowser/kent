/* tagSchemaToJson - Convert a tagStormSchema (such as used by tagStormCheck) to a JSON 
 * schema such as decribed in http://json-schema.org/. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "tagSchema.h"
#include "jsonWrite.h"
#include "tagToJson.h"

struct hash *gDescriptions = NULL;
char *gInName = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagSchemaToJson - Convert a tagStormSchema (such as used by tagStormCheck) to a JSON\n"
  "schema such as decribed in http://json-schema.org/\n"
  "usage:\n"
  "   tagSchemaToJsonSchema tagSchema.in outDir\n"
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

void writeHighFields(struct jsonWrite *jw, struct ttjSubObj *obj, struct hash *schemaHash)
/* Write out type, properties, patternProperties, required, and so forth to jw  */
{
jsonWriteString(jw, "type", "object");
struct ttjSubObj *sub;

/* Figure out if we have to do fixed properties and or pattern properties */
boolean needFixed = FALSE, needPattern = FALSE;
for (sub = obj->children; sub != NULL; sub = sub->next)
    {
    if (anyWild(sub->name))
        needPattern = TRUE;
    else
        needFixed = TRUE;
    }

if (needFixed)
    {
    jsonWriteObjectStart(jw, "properties");
    for (sub = obj->children; sub != NULL; sub = sub->next)
	{
	if (sub->children != NULL)
	    {
	    jsonWriteObjectStart(jw, sub->name);
	    char refText[512];
	    safef(refText, sizeof(refText), "#/definitions/%s", sub->name);
	    jsonWriteString(jw, "$ref", refText);
	    jsonWriteObjectEnd(jw);
	    }
	else
	    {
	    if (!anyWild(sub->name))
		{
		struct tagSchema *schema = hashMustFindVal(schemaHash, sub->fullName);
		writeOneSchema(jw, sub->name, schema);
		}
	    }
	}
    jsonWriteObjectEnd(jw);
    }
if (needPattern)
    {
    jsonWriteObjectStart(jw, "patternProperties");
    for (sub = obj->children; sub != NULL; sub = sub->next)
	{
	if (sub->children == NULL)
	    {
	    if (anyWild(sub->name))
		{
		struct tagSchema *schema = hashMustFindVal(schemaHash, sub->fullName);
		struct dyString *dy = dyStringNew(0);
		addRegExWildEquivalent(dy, sub->name);
		writeOneSchema(jw, dy->string, schema);
		dyStringFree(&dy);
		}
	    }
	}
    jsonWriteObjectEnd(jw);
    }

jsonWriteBoolean(jw, "additionalProperties", FALSE);

/* Figure out if any required fields  */
boolean needsRequired = FALSE;
for (sub = obj->children; sub != NULL; sub = sub->next)
    {
    if (sub->children == NULL)
	{
	struct tagSchema *schema = hashMustFindVal(schemaHash, sub->fullName);
	if (schema->required != 0)
	    {
	    needsRequired = TRUE;
	    break;
	    }
	}
    }

/* If have any requirements write out in array */
if (needsRequired)
    {
    jsonWriteListStart(jw, "required");
    for (sub = obj->children; sub != NULL; sub = sub->next)
	{
	if (sub->children == NULL)
	    {
	    struct tagSchema *schema = hashMustFindVal(schemaHash, sub->fullName);
	    if (schema->required != 0)
		jsonWriteString(jw, NULL, schema->name);
	    }
	}
    jsonWriteListEnd(jw);
    }
}

void rWriteDefinitions(struct jsonWrite *jw, struct ttjSubObj *obj, struct hash *schemaHash)
/* Recursiveley (bottom first) write out definitions for things with subobjects */
{
struct ttjSubObj *sub;
for (sub = obj->children; sub != NULL; sub = sub->next)
    {
    if (sub->children != NULL)
        {
	rWriteDefinitions(jw, sub, schemaHash);
	jsonWriteObjectStart(jw, sub->name);
	writeHighFields(jw, sub, schemaHash);
	jsonWriteObjectEnd(jw);
	}
    }
}

void makeObjectSchema(char *fileName, struct ttjSubObj *obj, struct hash *schemaHash)
/* Make schema for object and all of it's subObject in fileName */
{
verbose(1, "making %s\n", fileName);
struct jsonWrite *jw = jsonWriteNew();
jw->sep = '\n';

/* Write out start bits */
jsonWriteObjectStart(jw, NULL);
/* Write out bits that don't actually depend on our schema */
jsonWriteString(jw, "$schema", "http://json-schema.org/draft-04/schema#");
jsonWriteString(jw, "title", obj->name);
char text[1024];
safef(text, sizeof(text), "%s schema generate by tagSchemaToJson from %s", obj->name, gInName);
jsonWriteString(jw, "description", text);

/* See if have any subobjects */
boolean hasSubobj = FALSE;
struct ttjSubObj *sub;
for (sub = obj->children; sub != NULL; sub = sub->next)
    if (sub->children != NULL)
        hasSubobj = TRUE;

if (hasSubobj)
     {
     jsonWriteObjectStart(jw, "definitions");
     rWriteDefinitions(jw, obj, schemaHash);
     jsonWriteObjectEnd(jw);
     }

/* Write out top level fields */
writeHighFields(jw, obj, schemaHash);


/* Close out schema */
jsonWriteObjectEnd(jw);

FILE *f = mustOpen(fileName, "w");
fprintf(f, "%s\n", jw->dy->string);
carefulClose(&f);
}

void tagSchemaToJson(char *inTagSchema, char *outDir)
/* tagSchemaToJson- Convert a tagStormSchema (such as used by tagStormCheck) to a JSON 
 * schema such as decribed in http://json-schema.org/. */
{
gInName = inTagSchema;

/* Load in schema, and to reuse library code make a simple list of all fields */
struct tagSchema *schema, *schemaList = tagSchemaFromFile(inTagSchema);
struct hash *schemaHash = hashNew(0);
struct slName *allFields = NULL;
for (schema = schemaList; schema != NULL; schema = schema->next)
    {
    slNameAddHead(&allFields, schema->name);
    hashAdd(schemaHash, schema->name, schema);
    }
slReverse(&allFields);

/* Do some figuring based on all fields available of what objects to make */
verbose(1, "Got %d fields in %s\n", slCount(allFields), inTagSchema);
struct slName *topLevelList = ttjUniqToDotList(allFields, NULL, 0);
verbose(1, "Got %d top level objects\n", slCount(topLevelList));

/* Make list of objects */
struct slName *topEl;
struct ttjSubObj *obj, *objList = NULL;
for (topEl = topLevelList; topEl != NULL; topEl = topEl->next)
    {
    verbose(1, "  %s\n", topEl->name);
    struct ttjSubObj *obj = ttjMakeSubObj(allFields, topEl->name, topEl->name);
    slAddHead(&objList, obj);
    }

/* Write out a separate schema file in outDir for each high level object */
makeDirsOnPath(outDir);
for (obj = objList; obj != NULL; obj = obj->next)
    {
    char outPath[PATH_LEN];
    safef(outPath, sizeof(outPath), "%s/%s_schema.json", outDir, obj->name);
    makeObjectSchema(outPath, obj, schemaHash);
    }
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

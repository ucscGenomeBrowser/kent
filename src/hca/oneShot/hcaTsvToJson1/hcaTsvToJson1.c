/* hcaTsvToJson1 - Convert HCA flavored tab-separated-values to HCA flavored JSON. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "fieldedTable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaTsvToJson1 - Convert HCA flavored tab-separated-values to HCA flavored JSON\n"
  "usage:\n"
  "   hcaTsvToJson1 in.tsv outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct subObj
/* A subobject - may have children or not */
    {
    struct subObj *next;      // Points to next at same level
    struct subObj *children;  // Points to lower level, may be NULL.
    char *name;		      // Field name 
    char *fullName;	      // Field name with parent fields too.
    int rowIx;		      // Index of field in row. 
    };

struct slName *uniqToDotList(char *fields[], int fieldCount, char *prefix, int prefixLen)
/* Return list of all unique prefixes in field, that is parts up to first dot */
/* Return list of all words in fields that start with optional prefix, up to next dot */
{
struct slName *list = NULL;
int i;
for (i=0; i<fieldCount; ++i)
     {
     char *field = fields[i];
     if (prefixLen > 0)
         {
	 if (!startsWith(prefix, field))
	     continue;
	 field += prefixLen;
	 }
     char *dotPos = strchr(field, '.');
     if (dotPos == NULL)
	{
	if (prefixLen == 0)
	    errAbort("Field %s has no '.'", field);
	else
	    dotPos = field + strlen(field);
	}
     int prefixSize = dotPos - field;
     char prefix[prefixSize+1];
     memcpy(prefix, field, prefixSize);
     prefix[prefixSize] = 0;
     slNameStore(&list, prefix);
     }
return list;
}

struct subObj *makeSubObj(char *fields[], int fieldCount, char *objName, char *prefix)
/* Make a subObj */
{
struct subObj *obj;
AllocVar(obj);
obj->name = cloneString(objName);
obj->fullName = cloneString(prefix);
obj->rowIx = stringArrayIx(obj->fullName, fields, fieldCount);
uglyf("Making subObj %s %s %d\n", obj->name, obj->fullName, obj->rowIx);

/* Make a string that is prefix plus a dot */
char prefixDot[512];
safef(prefixDot, sizeof(prefixDot), "%s.", prefix);
int prefixDotLen = strlen(prefixDot);

struct slName *subList = uniqToDotList(fields, fieldCount, prefixDot, prefixDotLen);
if (subList != NULL)
     {
     struct slName *subName;
     for (subName = subList; subName != NULL; subName = subName->next)
         {
	 char newPrefix[512];
	 char *name = subName->name;
	 safef(newPrefix, sizeof(newPrefix), "%s%s", prefixDot, name);
	 struct subObj *subObj = makeSubObj(fields, fieldCount, name, newPrefix); 
	 slAddHead(&obj->children, subObj);
	 }
     }

return obj;
}


void rWriteJson(FILE *f, struct fieldedTable *table, struct fieldedRow *row, 
    struct subObj *obj)
{
fprintf(f, "{");
struct subObj *field;
for (field = obj->children; field != NULL; field = field->next)
    {
    fprintf(f, "\"%s\":", field->name);
    if (field->children != NULL)
         rWriteJson(f, table, row, field);
    else
         fprintf(f, "\"%s\"", row->row[field->rowIx]);
    if (field->next != NULL)
         fprintf(f, ",");
    }
fprintf(f, "}");
}

void writeTopJson(char *fileName, struct fieldedTable *table, struct fieldedRow *row, 
    struct subObj *top)
{
uglyf("Writing %s\n", fileName);
FILE *f = mustOpen(fileName, "w");
rWriteJson(f, table, row, top);
fprintf(f, "\n");
carefulClose(&f);
}

void makeJsonObjects(char *dir, struct fieldedTable *table, struct fieldedRow *row, 
    struct subObj *topLevelList)
{
char jsonFileName[PATH_LEN];
struct subObj *topEl;
for (topEl = topLevelList; topEl != NULL; topEl = topEl->next)
    {
    safef(jsonFileName, sizeof(jsonFileName), "%s/%s.json", dir, topEl->name);
    writeTopJson(jsonFileName, table, row, topEl);
    }
}

void hcaTsvToJson1(char *inTab, char *outDir)
/* hcaTsvToJson1 - Convert HCA flavored tab-separated-values to HCA flavored JSON. */
{
static char *requiredFields[] = {"project.id", "file.name", "sample.donor.species", "assay.single_cell.method"};
struct fieldedTable *table = fieldedTableFromTabFile(inTab, inTab, requiredFields, ArraySize(requiredFields));
uglyf("%s has %d fields and %d rows\n", inTab, table->fieldCount, table->rowCount);

struct slName *topLevelList = uniqToDotList(table->fields, table->fieldCount, NULL, 0);
uglyf("%d in topLevelList\n", slCount(topLevelList));

struct slName *topEl;
struct subObj *objList = NULL;
for (topEl = topLevelList; topEl != NULL; topEl = topEl->next)
    {
    uglyf("topEl %s\n", topEl->name);
    struct subObj *obj = makeSubObj(table->fields, table->fieldCount, topEl->name, topEl->name);
    slAddHead(&objList, obj);
    }

makeDirsOnPath(outDir);
int bundle = 0;
struct fieldedRow *row;
for (row = table->rowList; row != NULL; row = row->next)
    {
    ++bundle;
    char bundleDir[PATH_LEN];
    safef(bundleDir, sizeof(bundleDir), "%s/bundle%d", outDir, bundle);
    makeDir(bundleDir);
    makeJsonObjects(bundleDir, table, row, objList);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hcaTsvToJson1(argv[1], argv[2]);
return 0;
}

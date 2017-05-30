/* tabInfo - Get basic info on a tab-separated-file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "tagToSql.h"
#include "obscure.h"

/* Global vars. */
boolean anySchema;

/* Command line variables. */
boolean clCounts;
int clVals = 0;	
boolean clSchema = FALSE;
boolean clLooseSchema = FALSE;
boolean clTightSchema = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tabInfo - Get basic info on a tab-separated-file\n"
  "usage:\n"
  "   tabInfo input.tsv\n"
  "options:\n"
  "   -counts - if set output names, use counts, and value counts of each tag\n"
  "   -vals=N - display tags and the top N values for them\n"
  "   -schema - put a schema that will fit this tag storm in output.txt\n"
  "   -looseSchema - put a less fussy schema instead\n"
  "   -tightSchema - put a more fussy schema instead\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"counts", OPTION_BOOLEAN},
   {"vals", OPTION_INT},
   {"schema", OPTION_BOOLEAN},
   {"tightSchema", OPTION_BOOLEAN},
   {"looseSchema", OPTION_BOOLEAN},
   {NULL, 0},
};

struct tagInfo
/* Keeps track of number of uses and unique values of a tag */
    {
    struct tagInfo *next;   /* Next in list */
    char *tagName;	    /* Name of tag */
    int useCount;	    /* Number of times tag is used */
    struct hash *tagVals;   /* Hash of tag values, integer valued */
    };

void tagInfoAdd(struct tagInfo *tagInfo, char *tagVal)
/* Add information about tag to tagInfo */
{
if (!isEmpty(tagVal))
    tagInfo->useCount += 1;
hashIncInt(tagInfo->tagVals, tagVal);
}

struct tagInfo *tagInfoNew(char *tagName)
/* Create a new tagInfo structure */
{
struct tagInfo *tagInfo;
AllocVar(tagInfo);
tagInfo->tagName = cloneString(tagName);
tagInfo->tagVals = hashNew(0);
return tagInfo;
}

void tabInfo(char *fileName)
/* tabInfo - Get basic info on a tab-separated-file. */
{
/* Read table from file and unpack some common fields into local vars */
struct fieldedTable *table = fieldedTableFromTabFile(fileName, fileName, NULL, 0);
int fieldCount = table->fieldCount;
char **fields = table->fields;

/* Do we do something fancy? */
if (clCounts || clVals > 0 || anySchema)
    {
    /* Make up array of tagInfos and tagTypeInfos */
    struct tagInfo *tagArray[fieldCount];
    struct tagTypeInfo *typeArray[fieldCount];
    int fieldIx;
    for (fieldIx=0; fieldIx<fieldCount; ++fieldIx)
	{
	char *field = fields[fieldIx];
	tagArray[fieldIx] = tagInfoNew(field);
	if (anySchema)
	    typeArray[fieldIx] = tagTypeInfoNew(field);
	}

    /* Loop through table collecting info */
    struct fieldedRow *fr;
    for (fr = table->rowList; fr != NULL; fr = fr->next)
	{
	char **row = fr->row;
	for (fieldIx=0; fieldIx<fieldCount; ++fieldIx)
	    {
	    char *val = row[fieldIx];
	    tagInfoAdd(tagArray[fieldIx], val);
	    if (anySchema)
		tagTypeInfoAdd(typeArray[fieldIx], val);
	    }
	}

    /* Output information on each field */
    for (fieldIx=0; fieldIx<fieldCount; ++fieldIx)
	{
	struct tagInfo *tagInfo = tagArray[fieldIx];
	struct hash *valHash = tagInfo->tagVals;
	if (clVals > 0)
	    {
	    struct hashEl *valEl, *valList = hashElListHash(valHash);
	    printf("%s has %d uses with %d vals\n", tagInfo->tagName, tagInfo->useCount,
		slCount(valList));
	    slSort(&valList, hashElCmpIntValDesc);
	    int soFar = 0, j;
	    for (j=0, valEl = valList; j < clVals && valEl != NULL; ++j, valEl = valEl->next)
	        {
		int valCount = ptToInt(valEl->val);
		soFar += valCount;
		printf("   %d\t%s\n", valCount, valEl->name);
		}
	    int otherCount = tagInfo->useCount - soFar;
	    if (otherCount > 0)
	        printf("   %d\t(in %d others)\n", otherCount, slCount(valEl));
	    slFreeList(&valList);
	    }
	else if (anySchema)
	    {
	    tagTypeInfoPrintSchemaLine(typeArray[fieldIx], tagInfo->useCount, valHash, 
		clLooseSchema, clTightSchema, stdout);
	    }
	else
	    {
	    printf("%d\t%d\t%s\n", tagInfo->useCount, valHash->elCount, tagInfo->tagName);
	    }
	}
    }
else
    {
    printf("columns\t%d\n", fieldCount);
    printf("rows\t%d\n", table->rowCount);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clCounts = optionExists("counts");
clVals = optionInt("vals", clVals);
clSchema = optionExists("schema");
clLooseSchema = optionExists("looseSchema");
clTightSchema = optionExists("tightSchema");
anySchema = (clSchema || clLooseSchema || clTightSchema);
tabInfo(argv[1]);
return 0;
}

/* tagStormInfo - Get basic information on a tag storm. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"
#include "tagSchema.h"
#include "tagStorm.h"
#include "tagToSql.h"
#include "csv.h"

/* Global vars. */
boolean anySchema;

/* Command line variables. */
boolean clCounts;
int clVals = 0;	
boolean clSchema = FALSE;
boolean clLooseSchema = FALSE;
boolean clTightSchema = FALSE;
char *clTag = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormInfo - Get basic information on a tag storm\n"
  "usage:\n"
  "   tagStormInfo input.tags\n"
  "options:\n"
  "   -counts - if set output names, use counts, and value counts of each tag\n"
  "   -vals=N - display tags and the top N values for them\n"
  "   -tag=tagName - restrict info to just the one tag, often used with vals option\n"
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
   {"tag", OPTION_STRING},
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

void tagInfoAdd(struct tagInfo *tagInfo, char *tagVal, struct dyString *scratch)
/* Add information about tag to tagInfo */
{
tagInfo->useCount += 1;
char *pos = tagVal;
char *val;
while ((val = csvParseNext(&pos, scratch)) != NULL)
    hashIncInt(tagInfo->tagVals, val);
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

void rFillInStats(struct tagStanza *list, int expansion, struct hash *tagHash, 
    struct dyString *scratch, long *retStanzaCount, long *retTagCount, long *retExpandedCount, 
    int depth, int *retMaxDepth)
/* Recursively traverse stanza tree filling in values */
{
struct tagStanza *stanza;
if (++depth > *retMaxDepth)
    *retMaxDepth = depth;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    *retStanzaCount += 1;
    struct slPair *pair;
    int stanzaSize = 0;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	char *tagName = tagSchemaFigureArrayName(pair->name, scratch);
	if (clTag == NULL || sameString(tagName, clTag))
	    {
	    stanzaSize += 1;
	    *retExpandedCount += 1 + expansion;
	    struct tagInfo *tagInfo = hashFindVal(tagHash, tagName);
	    if (tagInfo == NULL)
		 {
		 tagInfo = tagInfoNew(tagName);
		 hashAdd(tagHash, tagName, tagInfo);
		 }
	    tagInfoAdd(tagInfo, pair->val, scratch);
	    }
	}
    *retTagCount += stanzaSize;
    if (stanza->children != NULL)
	rFillInStats(stanza->children, expansion + stanzaSize, tagHash, scratch,
	    retStanzaCount, retTagCount, retExpandedCount, depth, retMaxDepth);
    }
}

void tagStormInfo(char *inputTags)
/* tagStormInfo - Get basic information on a tag storm. */
{
struct tagStorm *tags = tagStormFromFile(inputTags);
struct hash *tagHash = hashNew(0);
long stanzaCount = 0, tagCount = 0, expandedTagCount = 0;
int maxDepth = 0;
struct dyString *scratch = dyStringNew(0);
rFillInStats(tags->forest, 0, tagHash, scratch, &stanzaCount, &tagCount, &expandedTagCount, 0, &maxDepth);

/* Do we do something fancy? */
if (clCounts || clVals > 0 || anySchema)
    {
    /* Set up type inference if doing schema */
    struct tagTypeInfo *ttiList = NULL;
    struct hash *ttiHash = NULL;
    if (anySchema)
        tagStormInferTypeInfo(tags, &ttiList, &ttiHash);
        
    struct hashEl *el, *list = hashElListHash(tagHash);
    slSort(&list, hashElCmp);
    for (el = list; el != NULL; el = el->next)
        {
	struct tagInfo *tagInfo = el->val;
	struct hash *valHash = tagInfo->tagVals;
	if (clVals > 0)
	    {
	    struct hashEl *valEl, *valList = hashElListHash(valHash);
	    printf("%s has %d uses with %d vals\n", tagInfo->tagName, tagInfo->useCount,
		slCount(valList));
	    slSort(&valList, hashElCmpIntValDesc);
	    int soFar = 0, i;
	    for (i=0, valEl = valList; i < clVals && valEl != NULL; ++i, valEl = valEl->next)
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
	    struct tagTypeInfo *tti = hashMustFindVal(ttiHash, tagInfo->tagName);
	    tagTypeInfoPrintSchemaLine(tti, tagInfo->useCount, valHash, 
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
    printf("stanzas\t%ld\n", stanzaCount);
    printf("depth\t%d\n", maxDepth);
    printf("tags\t%ld\n", tagCount);
    printf("storm\t%ld\n", expandedTagCount);
    printf("types\t%d\n", tagHash->elCount);
    printf("fields\t");
    struct hashEl *el, *list = hashElListHash(tagHash);
    slSort(&list, hashElCmp);
    for (el = list; el != NULL; el = el->next)
	{
	printf("%s", el->name);
	if (el->next != NULL)
	   printf(",");
	}
    printf("\n");
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
clTag = optionVal("tag", clTag);
anySchema = (clSchema || clLooseSchema || clTightSchema);
tagStormInfo(argv[1]);
return 0;
}

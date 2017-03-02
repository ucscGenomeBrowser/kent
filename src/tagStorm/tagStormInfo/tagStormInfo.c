/* tagStormInfo - Get basic information on a tag storm. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"
#include "tagStorm.h"
#include "tagToSql.h"

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
  "tagStormInfo - Get basic information on a tag storm\n"
  "usage:\n"
  "   tagStormInfo input.tags\n"
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

void rFillInStats(struct tagStanza *list, int expansion, struct hash *tagHash,
    long *retStanzaCount, long *retTagCount, long *retExpandedCount, 
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
	char *tagName = pair->name;
	stanzaSize += 1;
	*retExpandedCount += 1 + expansion;
	struct tagInfo *tagInfo = hashFindVal(tagHash, tagName);
	if (tagInfo == NULL)
	     {
	     tagInfo = tagInfoNew(tagName);
	     hashAdd(tagHash, tagName, tagInfo);
	     }
	tagInfoAdd(tagInfo, pair->val);
	}
    *retTagCount += stanzaSize;
    if (stanza->children != NULL)
	rFillInStats(stanza->children, expansion + stanzaSize, tagHash,
	    retStanzaCount, retTagCount, retExpandedCount, depth, retMaxDepth);
    }
}

double roundedMax(double val)
/* Return number rounded up to nearest power of ten */
{
if (val <= 0.0)
    return 0;
double roundedVal = 1;
for (;;)
    {
    if (val <= roundedVal)
	break;
    roundedVal *= 10;
    }
return roundedVal;
}

double roundedMin(double val)
/* Return number that is 0, 1, or nearest negative power of 10 */
{
if (val < 0)
    return -roundedMax(-val);
if (val < 1.0)
    return 0.0;
else
    return 1.0;
}

void tagStormInfo(char *inputTags)
/* tagStormInfo - Get basic information on a tag storm. */
{
struct tagStorm *tags = tagStormFromFile(inputTags);
struct hash *tagHash = hashNew(0);
long stanzaCount = 0, tagCount = 0, expandedTagCount = 0;
int maxDepth = 0;
rFillInStats(tags->forest, 0, tagHash, &stanzaCount, &tagCount, &expandedTagCount, 0, &maxDepth);

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
	    struct hashEl *valEl, *valList = hashElListHash(valHash);
	    printf("%s ", tagInfo->tagName);
	    if (tti->isNum)
	        {
		double minVal = tti->minVal, maxVal = tti->maxVal;
		if (tti->isInt)
		     putchar('#');
		else 
		     putchar('%');
		if (!clLooseSchema)
		    {
		    if (!clTightSchema)
			{
			minVal = roundedMin(minVal);
			maxVal = roundedMax(maxVal);
			}
		    if (tti->isInt)
			printf(" %lld %lld", (long long)floor(minVal), (long long)ceil(maxVal));
		    else
			printf(" %g %g", minVal, maxVal);
		    }
		}
	    else  /* Not numerical */
	        {
		/* Decide by a heuristic whether to make it an enum or not */
		putchar('$');
		if (!clLooseSchema)
		    {
		    int useCount = tagInfo->useCount;
		    int distinctCount = valHash->elCount;
		    double repeatRatio = (double)useCount/distinctCount;
		    int useFloor = 8, distinctCeiling = 10, distinctFloor = 1;
		    double repeatFloor = 4.0;

		    if (clTightSchema)
			{
			useFloor = 0;
			repeatFloor = 0.0;
			distinctFloor = 0;
			}
		    if (useCount >= useFloor && distinctCount <= distinctCeiling 
			&& distinctCount > distinctFloor && repeatRatio >= repeatFloor)
			{
			slSort(&valList, hashElCmp);
			for (valEl = valList; valEl != NULL; valEl = valEl->next)
			    {
			    char *val = valEl->name;
			    if (hasWhiteSpace(val))
				{
				char *quotedVal = makeQuotedString(val, '"');
				printf(" %s", quotedVal);
				freeMem(quotedVal);
				}
			    else
				printf(" %s", val);
			    }
			}
		    else
			{
			printf(" *");
			}
		    }
		}
	    printf("\n");
	    slFreeList(&valList);
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
anySchema = (clSchema || clLooseSchema || clTightSchema);
tagStormInfo(argv[1]);
return 0;
}

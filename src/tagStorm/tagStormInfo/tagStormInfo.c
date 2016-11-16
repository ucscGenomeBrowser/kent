/* tagStormInfo - Get basic information on a tag storm. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"
#include "tagStorm.h"

boolean doCounts;
boolean gValCount = 5;	// Maximum number of values to output

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
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"counts", OPTION_BOOLEAN},
   {"vals", OPTION_INT},
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
    long *retStanzaCount, long *retTagCount, long *retExpandedCount)
/* Recursively traverse stanza tree filling in values */
{
struct tagStanza *stanza;
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
    rFillInStats(stanza->children, expansion + stanzaSize, tagHash,
	retStanzaCount, retTagCount, retExpandedCount);
    }
}

void tagStormInfo(char *inputTags)
/* tagStormInfo - Get basic information on a tag storm. */
{
struct tagStorm *tags = tagStormFromFile(inputTags);
struct hash *tagHash = hashNew(0);
long stanzaCount = 0, tagCount = 0, expandedTagCount = 0;
rFillInStats(tags->forest, 0, tagHash, &stanzaCount, &tagCount, &expandedTagCount);
if (doCounts || gValCount > 0)
    {
    struct hashEl *el, *list = hashElListHash(tagHash);
    slSort(&list, hashElCmp);
    for (el = list; el != NULL; el = el->next)
        {
	struct tagInfo *tagInfo = el->val;
	struct hash *valHash = tagInfo->tagVals;
	if (gValCount > 0)
	    {
	    struct hashEl *valEl, *valList = hashElListHash(valHash);
	    printf("%s has %d uses with %d vals\n", tagInfo->tagName, tagInfo->useCount,
		slCount(valList));
	    slSort(&valList, hashElCmpIntValDesc);
	    int soFar = 0, i;
	    for (i=0, valEl = valList; i < gValCount && valEl != NULL; ++i, valEl = valEl->next)
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
	else
	    {
	    printf("%d\t%d\t%s\n", tagInfo->useCount, valHash->elCount, tagInfo->tagName);
	    }
	}
    }
else
    {
    printf("stanzas\t%ld\n", stanzaCount);
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
doCounts = optionExists("counts");
gValCount = optionInt("vals", gValCount);
tagStormInfo(argv[1]);
return 0;
}

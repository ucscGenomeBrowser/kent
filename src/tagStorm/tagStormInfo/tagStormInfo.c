/* tagStormInfo - Get basic information on a tag storm. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"
#include "tagStorm.h"

boolean doNames, doCounts;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormInfo - Get basic information on a tag storm\n"
  "usage:\n"
  "   tagStormInfo input.tags\n"
  "options:\n"
  "   -counts - if set output names and use counts of each tag\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"counts", OPTION_BOOLEAN},
   {NULL, 0},
};

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
	stanzaSize += 1;
	*retExpandedCount += 1 + expansion;
	hashIncInt(tagHash, pair->name);
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
if (doCounts)
    {
    struct hashEl *el, *list = hashElListHash(tagHash);
    slSort(&list, hashElCmp);
    for (el = list; el != NULL; el = el->next)
        {
	int i = ptToInt(el->val);
	printf("%s\t%d\n", el->name, i);
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
tagStormInfo(argv[1]);
return 0;
}

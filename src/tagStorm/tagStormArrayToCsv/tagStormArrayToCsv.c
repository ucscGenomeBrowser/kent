/* tagStormArrayToCsv - Convert multiple indexed tag approach to comma-separated-values.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "localmem.h"
#include "tagStorm.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormArrayToCsv - Convert multiple indexed tag approach to comma-separated-values.\n"
  "usage:\n"
  "   tagStormArrayToCsv in.tags out.tags\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void tagStanzaMergeArrays(struct tagStorm *storm, struct tagStanza *stanza)
/* Merge arrays in this stanza */
{
struct dyString *scratch = dyStringNew(0);

/* Build up hash of all arrays with integer values being number of time seen */
struct hash *arrayHash = hashNew(0);
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    char *name = pair->name;
    char *openBrace = strrchr(name, '[');
    if (openBrace != NULL)
	{
	dyStringClear(scratch);
	dyStringAppendN(scratch, name, openBrace-name);
	hashIncInt(arrayHash, scratch->string);
	}
    }

if (arrayHash->elCount > 0)
    {
    verbose(2, "Got %d arrays in stanza\n", arrayHash->elCount);
    // Turn valHash into a comma separated list of values for each array type
    struct hash *valHash = hashNew(0);
    struct dyString *valList = NULL;
    for (pair = stanza->tagList; pair != NULL; pair = pair->next)
        {
	char *name = pair->name;
	char *openBrace = strrchr(name, '[');
	if (openBrace != NULL)
	    {
	    dyStringClear(scratch);
	    dyStringAppendN(scratch, name, openBrace-name);
	    int maxCount = hashIntVal(arrayHash, scratch->string);
	    int index = atoi(openBrace+1);
	    if (index < 0 || index >= maxCount)
	        errAbort("Index out of range on %s", name);
	    struct dyString *dyVal = hashFindVal(valHash, scratch->string);
	    if (dyVal == NULL)
	        {
		dyVal = dyStringNew(0);
		dyStringAppend(dyVal, (char*)pair->val);
		slAddHead(&valList, dyVal);
		hashAdd(valHash, scratch->string, dyVal);
		}
	    else
	        {
		dyStringPrintf(dyVal, ",%s", (char*)pair->val);
		}
	    }
	}

    /* Replace  array tags with just single tag with comma sep value */
    struct hash *uniqHash = hashNew(0);
    struct slPair *newTagList = NULL, *next;
    for (pair = stanza->tagList; pair != NULL; pair = next)
        {
	next = pair->next;
	char *name = pair->name;
	char *openBrace = strrchr(name, '[');
	if (openBrace != NULL)
	    {
	    dyStringClear(scratch);
	    dyStringAppendN(scratch, name, openBrace-name);
	    struct dyString *dyVal = hashMustFindVal(valHash, scratch->string);
	    if (hashLookup(uniqHash, scratch->string) == NULL)
	        {
		hashAdd(uniqHash, scratch->string, NULL);
		pair->name = lmCloneString(storm->lm, scratch->string);
		pair->val = lmCloneString(storm->lm, dyVal->string);
		slAddHead(&newTagList, pair);
		}
	    }
	else
	    slAddHead(&newTagList, pair);
	}
    slReverse(&newTagList);
    stanza->tagList = newTagList;


    /* Clean up stuff we have to do for stanzas with arrays */
    hashFree(&valHash);
    hashFree(&uniqHash);
    dyStringFreeList(&valList);
    }

/* Clean up stuff we have to do for all stanzas */
hashFree(&arrayHash);
dyStringFree(&scratch);
}

void tagStormRecursivelyMergeArrays(struct tagStorm *storm, struct tagStanza *list)
/* Merge arrays inside of a all stanzas */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
     {
     tagStanzaMergeArrays(storm, stanza);
     tagStormRecursivelyMergeArrays(storm, stanza->children);
     }
}

void tagStormArrayToCsv(char *inFile, char *outFile)
/* tagStormArrayToCsv - Convert multiple indexed tag approach to comma-separated-values.. */
{
struct tagStorm *storm = tagStormFromFile(inFile);
verbose(2, "Read %d from %s\n", slCount(storm->forest), inFile);
tagStormRecursivelyMergeArrays(storm, storm->forest);
tagStormWrite(storm, outFile, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tagStormArrayToCsv(argv[1], argv[2]);
return 0;
}

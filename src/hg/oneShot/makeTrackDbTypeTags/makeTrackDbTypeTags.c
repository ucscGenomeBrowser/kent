/* makeTrackDbTypeTags - Create a file that lists what sort of tags are acceptable in track 
 * stanzas of various types.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeTrackDbTypeTags - Create a file that lists what sort of tags are acceptable in track \n"
  "stanzas of various types.\n"
  "usage:\n"
  "   makeTrackDbTypeTags in.ra tagTypes.tab\n"
  "where in.ra is typically generated with tdbQuery 'select * from *'\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct tagTypeUsage
/* A list of types where a tag is used. */
    {
    struct tagTypeUsage *next;
    char *tag;	/* Tag name, not allocated here. */
    struct slName *typeList;	/* List of types we've seen tag in. */
    };


int tagTypeUsageCmpTag(const void *va, const void *vb)
/* Compare to sort based on tag. */
{
const struct tagTypeUsage *a = *((struct tagTypeUsage **)va);
const struct tagTypeUsage *b = *((struct tagTypeUsage **)vb);
return strcmp(a->tag, b->tag);
}

void makeTrackDbTypeTags(char *inFile, char *outFile)
/* makeTrackDbTypeTags - Create a file that lists what sort of tags are acceptable in track stanzas of various types.. */
{
/* Read input and convert it to a usageList of tags and types. */
struct tagTypeUsage *usageList = NULL, *usage;
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct slPair *stanza;
while ((stanza = raNextRecordAsSlPairList(lf)) != NULL)
    {
    char *trackName = slPairFindVal(stanza, "track");
    if (trackName != NULL)
        {
	char *fullType = slPairFindVal(stanza, "type");
	if (fullType == NULL)
	    fullType = trackName;
	char *type = firstWordInLine(fullType);
	struct slPair *pair;
	for (pair = stanza; pair != NULL; pair = pair->next)
	    {
	    usage = hashFindVal(hash, pair->name);
	    if (usage == NULL)
	        {
		AllocVar(usage);
		hashAddSaveName(hash, pair->name, usage, &usage->tag);
		slAddHead(&usageList, usage);
		}
	    slNameStore(&usage->typeList, type);
	    }
	}
    slPairFreeValsAndList(&stanza);
    }
slSort(&usageList, tagTypeUsageCmpTag);
lineFileClose(&lf);

verbose(1, "Got %d different tags inside of track stanzas\n", hash->elCount);

/* Output usage. */
FILE *f = mustOpen(outFile, "w");
for (usage = usageList; usage != NULL; usage = usage->next)
    {
    fprintf(f, "%s", usage->tag);
    struct slName *type;
    int typeCount = slCount(usage->typeList);
    if (typeCount <= 3)
        {
	for (type = usage->typeList; type != NULL; type = type->next)
	    fprintf(f, " %s", type->name);
	}
    else
        fprintf(f, " *");
    fprintf(f, "\n");
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
makeTrackDbTypeTags(argv[1], argv[2]);
return 0;
}

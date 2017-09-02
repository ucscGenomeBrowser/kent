/* hcav2v3ModifyContributors - Change format of contributers array in curated.tags for HCA v2v3 transition.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "csv.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcav2v3ModifyContributors - Change format of contributers array in curated.tags for HCA v2v3 transition.\n"
  "usage:\n"
  "   hcav2v3ModifyContributors in.tags out.tags\n"
  "in.tags and out.tags can be the same\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void modifyContributors(struct tagStorm *tags, struct tagStanza *stanza, void *context)
/* Modify contributors tag, unwinding an array. */
{
char *targetTag = "project.contributor";
char *val = tagFindLocalVal(stanza, targetTag);
if (val != NULL)
    {
    uglyf("Got you %s\n", val);
    struct dyString *scratchIn = dyStringNew(0);
    struct dyString *scratchOut = dyStringNew(0);
    char *oneVal;
    int ix = 0;
    char *pos = val;
    while ((oneVal = csvParseNext(&pos, scratchIn)) != NULL)
        {
	char name[256];
	safef(name, sizeof(name), "project.contributors.%d.name", ++ix);
	char *escapedVal = csvEscapeToDyString(scratchOut, oneVal);
	tagStanzaAppend(tags, stanza, name, escapedVal);
	}
    dyStringFree(&scratchIn);
    dyStringFree(&scratchOut);
    tagStanzaDeleteTag(stanza, targetTag);
    }
}

void hcav2v3ModifyContributors(char *inName, char *outName)
/* hcav2v3ModifyContributors - Change format of contributers array in curated.tags for HCA v2v3 transition.. */
{
struct tagStorm *tags = tagStormFromFile(inName);
tagStormTraverse(tags, tags->forest, NULL, modifyContributors);
tagStormWrite(tags, outName, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hcav2v3ModifyContributors(argv[1], argv[2]);
return 0;
}

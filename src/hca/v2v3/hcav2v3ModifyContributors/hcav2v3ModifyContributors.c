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

void unpackArray(struct tagStorm *tags, struct tagStanza *stanza, char *inTag, char *outPrefix, char *outSuffix)
{
char *val = tagFindLocalVal(stanza, inTag);
if (val != NULL)
    {
    struct dyString *scratchIn = dyStringNew(0);
    struct dyString *scratchOut = dyStringNew(0);
    char *oneVal;
    int ix = 0;
    char *pos = val;
    while ((oneVal = csvParseNext(&pos, scratchIn)) != NULL)
        {
	char name[256];
	safef(name, sizeof(name), "%s.%d.%s", outPrefix, ++ix, outSuffix);
	char *escapedVal = csvEscapeToDyString(scratchOut, oneVal);
	tagStanzaAppend(tags, stanza, name, escapedVal);
	}
    dyStringFree(&scratchIn);
    dyStringFree(&scratchOut);
    tagStanzaDeleteTag(stanza, inTag);
    }
}


void modifyContributors(struct tagStorm *tags, struct tagStanza *stanza, void *context)
/* Modify contributors tag, unwinding an array. */
{
unpackArray(tags, stanza, "project.contributor", "project.contributors", "name");
unpackArray(tags, stanza, "project.experimental_design.1.text", "project.experimental_design", "text");
unpackArray(tags, stanza, "sample.supplementary_protocols.1.type", "sample.supplementary_protocols", "type");
unpackArray(tags, stanza, "sample.supplementary_protocols.1.description", "sample.supplementary_protocols", "description");
}

char *tagOrder[] = { "project.title", "project.core.id",};

void hcav2v3ModifyContributors(char *inName, char *outName)
/* hcav2v3ModifyContributors - Change format of contributers array in curated.tags for HCA v2v3 transition.. */
{
struct tagStorm *tags = tagStormFromFile(inName);
tagStormTraverse(tags, tags->forest, NULL, modifyContributors);
tagStormOrderSort(tags, tagOrder, ArraySize(tagOrder) );
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

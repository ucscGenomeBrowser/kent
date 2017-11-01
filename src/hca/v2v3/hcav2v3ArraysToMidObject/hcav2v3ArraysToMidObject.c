/* hcav2v3ArraysToMidObject - Change format of contributers array and other arrays that used to 
 * be simple arrays to a field of an array of objects in curated.tags for HCA v2v3 transition. */
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
  "hcav2v3ArraysToMidObject - Change format of contributers array and other arrays that used to\n"
  "be simple arrays to a field of an array of objects in curated.tags for HCA v2v3 transition.\n"
  "hcav2v3ArraysToMidObject - Change format of contributers array in curated.tags for HCA v2v3 transition.\n"
  "usage:\n"
  "   hcav2v3ArraysToMidObject in.tags out.tags\n"
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


void arrayToMidObject(struct tagStorm *tags, struct tagStanza *stanza, void *context)
/* Modify a tag, unwinding a simple array into a field in an array of objects. */
{
unpackArray(tags, stanza, "project.contributor", "project.contributors", "name");
unpackArray(tags, stanza, "project.experimental_design.1.text", "project.experimental_design", "text");
unpackArray(tags, stanza, "sample.supplementary_protocols.1.type", "sample.supplementary_protocols", "type");
unpackArray(tags, stanza, "sample.supplementary_protocols.1.description", "sample.supplementary_protocols", "description");
unpackArray(tags, stanza, "project.publications.1.pmid", "project.publications", "pmid");
unpackArray(tags, stanza, "project.publications.1.author_list",  "project.publications", "author_list");
unpackArray(tags, stanza, "sample.donor.disease.1.text",  "sample.donor.disease", "text");
unpackArray(tags, stanza, "sample.donor.ancestry.1.text",  "sample.donor.ancestry", "text");
unpackArray(tags, stanza, "sample.donor.medication.1.text",  "sample.donor.medication", "text");
unpackArray(tags, stanza, "sample.donor.species.1.text",  "sample.donor.species", "text");
unpackArray(tags, stanza, "sample.donor.species.1.ontology",  "sample.donor.species", "ontology");
unpackArray(tags, stanza, "sample.donor.strain.1.text",  "sample.donor.strain", "text");
unpackArray(tags, stanza, "sample.supplementary_protocols.1.type",  "sample.supplementary_protocols", "type");
unpackArray(tags, stanza, "sample.supplementary_protocols.1.description",  "sample.supplementary_protocols", "description");
}

char *tagOrder[] = { "project.title", "project.core.id",};

void hcav2v3ArraysToMidObject(char *inName, char *outName)
/* hcav2v3ModifyContributors - Change format of contributers array and other arrays that used to 
 * be simple arrays to a field of an array of objects in curated.tags for HCA v2v3 transition. */
{
struct tagStorm *tags = tagStormFromFile(inName);
tagStormTraverse(tags, tags->forest, NULL, arrayToMidObject);
tagStormOrderSort(tags, tagOrder, ArraySize(tagOrder) );
tagStormWrite(tags, outName, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hcav2v3ArraysToMidObject(argv[1], argv[2]);
return 0;
}

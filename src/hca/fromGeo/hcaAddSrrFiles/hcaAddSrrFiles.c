/* hcaAddSrrFiles - Add srr files to a HCA tagStorm as a files[] array in assay.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "csv.h"

/* Global vars */
int gSrrCount = 0, gFileCount = 0;  // Keep track of number srr tags in and files out */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaAddSrrFiles - Add srr files to a HCA tagStorm as a files[] array in assay.\n"
  "usage:\n"
  "   hcaAddSrrFiles in.tags out.tags\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


void  addFilesToSraRun(struct tagStorm *storm, struct tagStanza *stanza)
/* If stanza has an srr_run tag add a files tag to it */
{
char *srrCsv = tagFindLocalVal(stanza, "assay.seq.sra_run");
if (srrCsv != NULL)
    {
    char *pairing = tagMustFindVal(stanza, "assay.seq.paired_ends");
    int endCount = (sameString(pairing, "no") ? 1 : 2);
    struct dyString *scratch = dyStringNew(0);
    char *srr, *pos = srrCsv;
    struct dyString *filesDy = dyStringNew(0);
    while ((srr = csvParseNext(&pos, scratch)) != NULL)
        {
	++gSrrCount;
	int i;
	for (i=0; i<endCount; ++i)
	    {
	    ++gFileCount;
	    if (filesDy->stringSize > 0)
		dyStringAppendC(filesDy, ',');
	    dyStringPrintf(filesDy, "%s_%d.fastq.gz", srr, i+1);
	    }
	}
    tagStanzaAppend(storm, stanza, "assay.seq.files", filesDy->string);
    dyStringFree(&scratch);
    dyStringFree(&filesDy);
    }
}

void rAddFilesToSraRun(struct tagStorm *storm, struct tagStanza *list)
/* Recursively do addFilesToSraRun to all stanzas in storm */
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    addFilesToSraRun(storm, stanza);
    rAddFilesToSraRun(storm, stanza->children);
    }
}



void hcaAddSrrFiles(char *inTags, char *outTags)
/* hcaAddSrrFiles - Add srr files to a HCA tagStorm as a files[] array in assay.. */
{
struct tagStorm *storm = tagStormFromFile(inTags);

verbose(1, "Added barcode sizes and offsets\n");

rAddFilesToSraRun(storm, storm->forest);
verbose(1, "Added %d files to %d runs\n", gFileCount, gSrrCount);

tagStormWrite(storm, outTags, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hcaAddSrrFiles(argv[1], argv[2]);
return 0;
}

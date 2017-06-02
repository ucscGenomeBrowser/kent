/* hcaAddSrrFiles - Add srr files to a HCA tagStorm as a files[] array in assay.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "csv.h"

/* Global vars */
int gSrrCount = 0, gFileCount = 0;  // Keep track of number srr tags in and files out */
int gEnds = 1;			    // Default to single ended

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
char *srrCsv = tagFindLocalVal(stanza, "assay.sra_run");
if (srrCsv != NULL)
    {
    struct dyString *scratch = dyStringNew(0);
    char *srr, *pos = srrCsv;
    struct dyString *filesDy = dyStringNew(0);
    while ((srr = csvParseNext(&pos, scratch)) != NULL)
        {
	++gSrrCount;
	int i;
	for (i=0; i<gEnds; ++i)
	    {
	    ++gFileCount;
	    if (filesDy->stringSize > 0)
		dyStringAppendC(filesDy, ',');
	    dyStringPrintf(filesDy, "%s_%d.fastq.gz", srr, i+1);
	    }
	}
    tagStanzaAppend(storm, stanza, "assay.files", filesDy->string);
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
struct tagStanza *rootStanza = storm->forest;
if (storm->forest == NULL)
    errAbort("Expecting single top level stanza, got %d", slCount(rootStanza));

char *method = tagMustFindVal(rootStanza, "assay.single_cell.method");
char *pairing = tagMustFindVal(rootStanza, "assay.seq.paired_ends");
char *molecule = tagMustFindVal(rootStanza, "assay.seq.molecule");
verbose(2, "hcaAddSrrFiles method=%s, pairing=%s, molecule=%s\n", method, pairing, molecule);

/* Set up default values for things that control output based on method */
char *umiBarcodeSize = "0", *cellBarcodeSize = "0";
char umiBarcodeOffset[8], cellBarcodeOffset[8];

/* Figure out more or less what to do based on method and pairing */
if (sameString(method, "drop-seq"))
    {
    uglyf("drop-seq\n");
    gEnds = 2;
    safef(umiBarcodeOffset, sizeof(umiBarcodeOffset), "12");
    safef(cellBarcodeOffset, sizeof(cellBarcodeOffset), "0");
    umiBarcodeSize = "8";
    cellBarcodeSize = "12";
    }
else if (sameString(method, "10x"))
    {
    uglyf("10x\n");
    gEnds = 2;
    safef(umiBarcodeOffset, sizeof(umiBarcodeOffset), "16");
    safef(cellBarcodeOffset, sizeof(cellBarcodeOffset), "0");
    umiBarcodeSize = "10";
    cellBarcodeSize = "16";
    }
else
    {
    if (sameString(pairing, "yes"))
        gEnds = 2;
    }

/* Add barcode tags */
tagStanzaUpdateTag(storm, rootStanza, "assay.seq.umi_barcode_size", umiBarcodeSize);
if (differentString("0", umiBarcodeSize))
    tagStanzaUpdateTag(storm, rootStanza, "assay.seq.umi_barcode_offset", umiBarcodeOffset);
tagStanzaUpdateTag(storm, rootStanza, "assay.single_cell.cell_barcode_size", cellBarcodeSize);
if (differentString("0", cellBarcodeSize))
    tagStanzaUpdateTag(storm, rootStanza, "assay.single_cell.cell_barcode_offset", 
	cellBarcodeOffset);

/* Add files[] array wherever find an assay.sra_run */
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

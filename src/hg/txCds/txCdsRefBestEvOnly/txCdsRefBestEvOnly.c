/* txCdsRefBestEvOnly - Go through a cdsEvidence file, and extract only the bits that refer to the native orf for a RefSeqReviewed transcript.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdsEvidence.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsRefBestEvOnly - Go through a cdsEvidence file, and extract only the bits that refer to the native orf for a RefSeqReviewed transcript.\n"
  "usage:\n"
  "   txCdsRefBestEvOnly in.tce out.tce\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void txCdsRefBestEvOnly(char *inFile, char *outFile)
/* txCdsRefBestEvOnly - Go through a cdsEvidence file, and extract only the bits that refer to the native orf for a RefSeqReviewed transcript.. */
{
struct cdsEvidence *cds, *cdsList = cdsEvidenceLoadAll(inFile);
struct hash *nativeEvHash = hashNew(18);
FILE *f = mustOpen(outFile, "w");

/* Make one pass through list adding the native refseq reviewed records to hash. */
for (cds = cdsList; cds != NULL; cds = cds->next)
    {
    if (sameString(cds->source, "RefSeqReviewed"))
        {
	char *acc = strrchr(cds->name, '.');
	assert(acc != NULL);
	acc += 1;
	if (sameString(acc, cds->accession))
	    hashAdd(nativeEvHash, cds->name, cds);
	}
    }

/* Make another pass through outputting all lines that correspond to
 * reviewd refseq's ORF. */
for (cds = cdsList; cds != NULL; cds = cds->next)
    {
    struct cdsEvidence *native = hashFindVal(nativeEvHash, cds->name);
    if (native != NULL)
        {
	if (cds->start == native->start && cds->end == native->end)
	    cdsEvidenceTabOut(cds, f);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
txCdsRefBestEvOnly(argv[1], argv[2]);
return 0;
}

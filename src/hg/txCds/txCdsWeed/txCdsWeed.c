/* txCdsWeed - Remove bad CDSs including NMD candidates. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "txInfo.h"
#include "cdsEvidence.h"
#include "cdsOrtho.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsWeed - Remove bad CDSs including NMD candidates\n"
  "usage:\n"
  "   txCdsWeed in.tce in.info out.tce out.info\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void txCdsWeed(char *inTce, char *inInfo, char *outTce, char *outInfo)
/* txCdsWeed - Remove bad CDSs including NMD candidates. */
{
/* Read in txInfo into a hash keyed by transcript name */
struct hash *infoHash = hashNew(17);
struct txInfo *info, *infoList = txInfoLoadAll(inInfo);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);
verbose(2, "Read info on %d transcripts from %s\n", infoHash->elCount, 
	inInfo);

/* Read in input CDS */
struct cdsEvidence *cds, *cdsList = cdsEvidenceLoadAll(inTce);

/* Write ones that pass filter to output file. */
FILE *f = mustOpen(outTce, "w");
int weedCount = 0;
for (cds = cdsList; cds != NULL; cds = cds->next)
    {
    struct txInfo *info = hashMustFindVal(infoHash, cds->name);
    if (info->isRefSeq || 
    	(!info->nonsenseMediatedDecay && !info->cdsSingleInIntron && !info->cdsSingleInUtr3))
	{
	cdsEvidenceTabOut(cds, f);
	}
    else
	{
	++weedCount;
	info->orfSize = 0;
	}
    }
carefulClose(&f);
verbose(1, "Weeded %d of %d cds\n", weedCount, slCount(cdsList));

/* Write updated info file. */
f = mustOpen(outInfo, "w");
for (info = infoList; info != NULL; info = info->next)
    txInfoTabOut(info, f);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
txCdsWeed(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

/* txCdsWeed - Remove bad CDSs including NMD candidates. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "txInfo.h"
#include "cdsEvidence.h"

static char const rcsid[] = "$Id: txCdsWeed.c,v 1.1 2007/02/28 00:25:55 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsWeed - Remove bad CDSs including NMD candidates\n"
  "usage:\n"
  "   txCdsWeed in.tce in.info out.tce\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void txCdsWeed(char *inTce, char *inInfo, char *outTce)
/* txCdsWeed - Remove bad CDSs including NMD candidates. */
{
/* Read in txInfo into a hash keyed by transcript name */
struct hash *infoHash = hashNew(16);
struct txInfo *info, *infoList = txInfoLoadAll(inInfo);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);
verbose(2, "Read info on %d transcripts from %s\n", infoHash->elCount, 
	inInfo);

/* Read in input CDS */
struct cdsEvidence *cds, *cdsList = cdsEvidenceLoadAll(inTce);

/* Write ones that pass filter to output file. */
FILE *f = mustOpen(outTce, "w");
for (cds = cdsList; cds != NULL; cds = cds->next)
    {
    struct txInfo *info = hashMustFindVal(infoHash, cds->name);
    if (!info->nonsenseMediatedDecay && !info->cdsSingleInIntron
        && !info->cdsSingleInUtr3)
	{
	cdsEvidenceTabOut(cds, f);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
txCdsWeed(argv[1], argv[2], argv[3]);
return 0;
}

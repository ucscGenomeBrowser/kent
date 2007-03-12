/* txCdsWeed - Remove bad CDSs including NMD candidates. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "txInfo.h"
#include "cdsEvidence.h"
#include "cdsOrtho.h"

static char const rcsid[] = "$Id: txCdsWeed.c,v 1.4 2007/03/12 05:37:38 kent Exp $";

double minOrthoSize = 0.60;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsWeed - Remove bad CDSs including NMD candidates\n"
  "usage:\n"
  "   txCdsWeed in.tce in.info in.ortho out.tce out.info\n"
  "options:\n"
  "   -minOrthoSize=0.N - Minimum sizer ratio in ortho organism compared to us\n"
  "                 Default %g\n"
  "Note - this will need some tuning for mouse.\n"
  , minOrthoSize
  );
}

static struct optionSpec options[] = {
   {"minOrthoSize", OPTION_DOUBLE},
   {NULL, 0},
};

boolean passesOrtho(struct cdsEvidence *cds, struct hash *orthoHash)
/* Return true if it passes our weak orthology test - CDS must be
 * at least 60% size in two species.  One, chimp, is practically a freebie. */
{
struct hashEl *hel = hashLookup(orthoHash, cds->name);
if (hel == NULL)
    errAbort("Couldn't find orthology info for %s", cds->name);
int minCdsSize = minOrthoSize * (cds->end - cds->start);
int goodCount = 0;
for ( ; hel != NULL; hel = hashLookupNext(hel) )
    {
    struct cdsOrtho *ortho = hel->val;
    if (ortho->start == cds->start && ortho->end == cds->end)
        {
	if (ortho->orthoSize >= minCdsSize)
	    {
	    ++goodCount;
	    }
	}
    }
if (goodCount < 2)
    return FALSE;
else
    return TRUE;
}

void txCdsWeed(char *inTce, char *inInfo, char *inOrtho, char *outTce, char *outInfo)
/* txCdsWeed - Remove bad CDSs including NMD candidates. */
{
/* Read in txInfo into a hash keyed by transcript name */
struct hash *infoHash = hashNew(17);
struct txInfo *info, *infoList = txInfoLoadAll(inInfo);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);
verbose(2, "Read info on %d transcripts from %s\n", infoHash->elCount, 
	inInfo);

/* Read in ortho info into a hash as well. */
struct hash *orthoHash = hashNew(19);
struct cdsOrtho *ortho, *orthoList = cdsOrthoLoadAll(inOrtho);
for (ortho = orthoList; ortho != NULL; ortho = ortho->next)
    hashAdd(orthoHash, ortho->name, ortho);

/* Read in input CDS */
struct cdsEvidence *cds, *cdsList = cdsEvidenceLoadAll(inTce);

/* Write ones that pass filter to output file. */
FILE *f = mustOpen(outTce, "w");
for (cds = cdsList; cds != NULL; cds = cds->next)
    {
    struct txInfo *info = hashMustFindVal(infoHash, cds->name);
    if (info->isRefSeq || 
    	(!info->nonsenseMediatedDecay && !info->cdsSingleInIntron && !info->cdsSingleInUtr3
	&& passesOrtho(cds, orthoHash)))
	{
	cdsEvidenceTabOut(cds, f);
	}
    else
	info->orfSize = 0;
    }
carefulClose(&f);

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
if (argc != 6)
    usage();
minOrthoSize = optionDouble("minOrthoSize", minOrthoSize);
txCdsWeed(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}

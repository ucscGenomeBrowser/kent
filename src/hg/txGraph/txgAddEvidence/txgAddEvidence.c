/* txgAddEvidence - Add evidence from a bed file to existing transcript graph.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "txGraph.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgAddEvidence - Add evidence from a bed file to existing transcript graph.\n"
  "usage:\n"
  "   txgAddEvidence in.txg in.bed sourceType out.txg\n"
  "where sourceType is a decriptive word like 'refSeq' 'est' etc.\n"
  "options:\n"
  "   -intron - in.bed file  is full of introns, not exons\n"
  );
}

static struct optionSpec options[] = {
   {"intron", OPTION_BOOLEAN},
   {NULL, 0},
};

void txgAddEvidence(char *txgIn, char *bedIn, char *sourceType, char *txgOut)
/* txgAddEvidence - Add evidence from a bed file to existing transcript graph.. */
{
struct txGraph *graphList = txGraphLoadAll(txgIn);
verbose(2, "%d elements in graphList\n", slCount(graphList));
struct bed *bedList = bedLoadAll(bedIn);
verbose(2, "%d elements in bedList\n", slCount(bedList));
FILE *f = mustOpen(txgOut, "w");

verbose(1, "Adding %s evidence from %s to %s. Result is in %s.\n",
	sourceType, bedIn, txgIn, txgOut);

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
txgAddEvidence(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

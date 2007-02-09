/* txgAddEvidence - Add evidence from a bed file to existing transcript graph.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "txGraph.h"
#include "binRange.h"

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

struct minChromSize
/* Associate chromosome and size. */
    {
    char *chrom;	/* Chromosome name, Not alloced here */
    int minSize;		
    };

struct hash *txgChromMinSizeHash(struct txGraph *txgList)
/* Hash full of lower bounds on chromosome sizes, taken
 * from tEnd's in txgList. */
{
struct txGraph *txg;
struct hash *sizeHash = hashNew(16);
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    struct minChromSize *chrom = hashFindVal(sizeHash, txg->tName);
    if (chrom == NULL)
        {
	lmAllocVar(sizeHash->lm, chrom);
	chrom->chrom = txg->tName;
	chrom->minSize = txg->tEnd;
	hashAdd(sizeHash, txg->tName, chrom);
	}
    else
        {
	chrom->minSize = max(chrom->minSize, txg->tEnd);
	}
    }
return sizeHash;
}

struct hash *txgIntoKeeperHash(struct txGraph *txgList)
/* Create a hash full of bin keepers (one for each chromosome or contig.
 * The binKeepers are full of txGraphs. */
{
struct hash *sizeHash = txgChromMinSizeHash(txgList);
struct hash *bkHash = hashNew(16);
struct txGraph *txg;
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    struct binKeeper *bk = hashFindVal(bkHash, txg->tName);
    if (bk == NULL)
        {
	struct minChromSize *chrom = hashMustFindVal(sizeHash, txg->tName);
	bk = binKeeperNew(0, chrom->minSize);
	hashAdd(bkHash, txg->tName, bk);
	}
    }
return bkHash;
}

void txgAddEvidence(char *txgIn, char *bedIn, char *sourceType, char *txgOut)
/* txgAddEvidence - Add evidence from a bed file to existing transcript graph.. */
{
struct txGraph *txgList = txGraphLoadAll(txgIn);
verbose(2, "%d elements in txgList\n", slCount(txgList));
struct bed *bedList = bedLoadAll(bedIn);
verbose(2, "%d elements in bedList\n", slCount(bedList));
FILE *f = mustOpen(txgOut, "w");

verbose(1, "Adding %s evidence from %s to %s. Result is in %s.\n",
	sourceType, bedIn, txgIn, txgOut);
struct hash *bkHash = txgIntoKeeperHash(txgList);
verbose(2, "Loaded into keeper hash of %d elements\n", bkHash->elCount);

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

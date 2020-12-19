/* txgToGeneBed - Convert txg to a simple bed with one block and the most common gene name.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "txGraph.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txgToGeneBed - Convert txg to a simple bed with one block and the most common gene name.\n"
  "usage:\n"
  "   txgToGeneBed in.txg txToGene.tsv geneToName.tsv out.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

char *pickGene(struct txGraph *txg, struct hash *txToGeneHash)
/* Pick out most commonly used name */
{
if (txg->sourceCount <= 2)  // If just one or two take first one
    return hashMustFindVal(txToGeneHash, txg->sources[0].accession);
else
    {
    int bestCount = 0;
    char *bestGene = NULL;
    struct hash *geneHash = hashNew(0);
    int i;
    for (i=0; i<txg->sourceCount; ++i)
        {
	char *gene = hashMustFindVal(txToGeneHash, txg->sources[i].accession);
	int count = hashIncInt(geneHash, gene);
	if (count > bestCount)
	    {
	    bestCount = count;
	    bestGene = gene;
	    }
	}
    hashFree(&geneHash);
    return bestGene;
    }
}

void txgToGeneBed(char *inTxg, char *txToGeneFile, char *geneToNameFile, char *outBed)
/* txgToGeneBed - Convert txg to a simple bed with one block and the most common gene name.. */
{
struct hash *txToGeneHash = hashTwoColumnFile(txToGeneFile);
struct hash *geneToNameHash = hashTwoColumnFile(geneToNameFile);
struct txGraph *txgList = txGraphLoadAll(inTxg);
FILE *f = mustOpen(outBed, "w");
struct txGraph *txg;
for (txg = txgList; txg != NULL; txg = txg->next)
    {
    char *gene = pickGene(txg, txToGeneHash);
    char *name = hashFindVal(geneToNameHash, gene);
    if (name == NULL)
        name = gene;
    fprintf(f, "%s\t%d\t%d\t%s\t0\t%s\t%s\n", 
	txg->tName, txg->tStart, txg->tEnd, gene, txg->strand, name);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
txgToGeneBed(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

/* lrgToPsl - Convert Locus Reference Genomic (LRG) bed to PSL. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "hdb.h"
#include "lrg.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lrgToPsl - Convert Locus Reference Genomic (LRG) bed to PSL\n"
  "usage:\n"
  "   lrgToPsl lrg.bed chrom.sizes lrg.psl\n"
  "LRG's BED12+ representation includes a compact indel representation;\n"
  "use the info (plus chrom.sizes) to make PSL.\n"
  "(intended for mapping from LRG coordinates to assembly coords).\n"
//  "options:\n"
//  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

char *lrgFile = argv[1];
char *chromSizes = argv[2];
char *pslFile = argv[3];

struct hash *chromHash = hChromSizeHashFromFile(chromSizes);
struct lrg *lrg, *lrgList = lrgLoadAllByTab(lrgFile);
FILE *f = mustOpen(pslFile, "w");
for (lrg = lrgList;  lrg != NULL;  lrg = lrg->next)
    {
    int chromSize = hashIntValDefault(chromHash, lrg->chrom, 0);
    if (chromSize == 0)
	errAbort("Can't find size of '%s' in chrom.sizes file %s.", lrg->chrom, chromSizes);
    struct psl *psl = lrgToPsl(lrg, chromSize);
    pslTabOut(psl, f);
    }
return 0;
}

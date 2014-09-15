/* genePredToBigGenePred - converts genePred or genePredExt to bigGenePred. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genePred.h"
#include "bigGenePred.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredToBigGenePred - converts genePred or genePredExt to bigGenePred input\n"
  "usage:\n"
  "   genePredToBigGenePred file.gp file.bgpInput\n"
  "NOTE: to build bigBed:\n"
  "   bedToBigBed -type=bed12+8 -tab -as=bigGenePred.as file.bgpInput chrom.sizes output.bb\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

#define MAX_BLOCKS 10000
unsigned blockSizes[MAX_BLOCKS];
unsigned blockStarts[MAX_BLOCKS];

void outBigGenePred(FILE *fp, struct genePred *gp)
{
struct bigGenePred bgp;

if (gp->exonCount > MAX_BLOCKS)
    errAbort("genePred has more than %d exons, make MAX_BLOCKS bigger in source", MAX_BLOCKS);

if (gp->exonFrames == NULL)
    genePredAddExonFrames(gp);

bgp.chrom = gp->chrom;
bgp.chromStart = gp->txStart;
bgp.chromEnd = gp->txEnd;
bgp.name = gp->name;
bgp.score = 1000;
bgp.strand[0] = gp->strand[0];
bgp.strand[1] = gp->strand[1];
bgp.thickStart = gp->cdsStart;
bgp.thickEnd = gp->cdsEnd;
bgp.itemRgb = 0; // BLACK
bgp.blockCount = gp->exonCount;
bgp.blockSizes = (unsigned *)blockSizes;
bgp.chromStarts = (unsigned *)blockStarts;
int ii;
for(ii=0; ii < bgp.blockCount; ii++)
    {
    blockStarts[ii] = gp->exonStarts[ii] - bgp.chromStart;
    blockSizes[ii] = gp->exonEnds[ii] - gp->exonStarts[ii];
    }
    
bgp.name2 = gp->name2;
bgp.cdsStartStat = gp->cdsStartStat;
bgp.cdsEndStat = gp->cdsEndStat;
bgp.exonFrames = gp->exonFrames;
bgp.type = NULL;
bgp.geneName = gp->name;
bgp.geneName2 = gp->name2;
bgp.geneType = NULL;

bigGenePredOutput(&bgp, fp, '\t', '\n');
}

void genePredToBigGenePred(char *genePredFile, char *bigGeneOutput)
/* genePredToBigGenePred - converts genePred or genePredExt to bigGenePred. */
{
boolean hasFrames = TRUE;
struct genePred *gp = hasFrames ? genePredExtLoadAll(genePredFile) : genePredLoadAll(genePredFile);

FILE *fp = fopen(bigGeneOutput, "w");

for(; gp ; gp = gp->next)
    {
    outBigGenePred(fp, gp);
    }

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
genePredToBigGenePred(argv[1], argv[2]);
return 0;
}

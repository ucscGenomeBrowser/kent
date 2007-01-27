/* clusterRna - Make clusters of mRNA and ESTs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "binRange.h"
#include "options.h"
#include "nibTwo.h"
#include "psl.h"
#include "ggMrnaAli.h"
#include "geneGraph.h"

static char const rcsid[] = "$Id: clusterPsl.c,v 1.1 2007/01/27 04:15:17 kent Exp $";

void addCluster(struct ggMrnaAli *maList, struct binKeeper *bins, struct dnaSeq *chrom);
/* Do basic clustering. */

void writeCluster(struct ggMrnaCluster *cluster, FILE *f);
/* Write out a cluster to file. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterPsl - Make clusters of mRNA aligments\n"
  "usage:\n"
  "   clusterPsl input dnaSource  output\n"
  "where input is a psl file (not necessarily sorted)\n"
  "      dnaSource is a .2bit file or a directory of .nib files\n"
  "      output is a text file full of cluster info\n"
  "options:\n"
  "   -verbose=2 Make output more verbose.\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


struct ggMrnaCluster *clusterPslsOnChrom(struct psl *pslList, struct dnaSeq *chrom)
/* Make clusters of overlapping alignments on a single chromosome. */
{
struct ggMrnaCluster *clusterList = NULL;

/* Convert alignments from psl to ggMrnaAli. */
struct psl *psl;
struct ggMrnaAli *maList = NULL, *ma;
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    assert(psl->tSize == chrom->size);
    ma = pslToGgMrnaAli(psl, psl->tName, 0, psl->tSize, chrom);
    ggMrnaAliMergeBlocks(ma, 5);
    slAddHead(&maList, ma);
    }
slReverse(&maList);

/* Set up conversion from maList to binKeeper full of ggMrnaClusters. */
struct binKeeper *bins = binKeeperNew(0, chrom->size);
addCluster(maList, bins, chrom);

/* Convert from binKeeper of clusters to simple list of clusters. */
struct binElement *binList, *bin;
binList = binKeeperFindAll(bins);
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct ggMrnaCluster *cluster = bin->val;
    slAddHead(&clusterList, cluster);
    }
slReverse(&clusterList);

/* Clean up and go home. */
binKeeperFree(&bins);
return clusterList;
}


void clusterPsl(char *pslName, char *seqDir, char *outName)
/* Transform a file full of psl's into a file full of rnaClusters */
{
FILE *out = mustOpen(outName, "w");

/* Set up DNA seqence accesser */
struct nibTwoCache *dnaCache = nibTwoCacheNew(seqDir);
/* Load in list and sort by chromosomes. */
struct psl *pslList = pslLoadAll(pslName);
slSort(&pslList, pslCmpTarget);

/* Go through input one chromosome at a time. */
struct psl *chromStart, *chromEnd = NULL;
for (chromStart = pslList; chromStart != NULL; chromStart = chromEnd)
    {
    /* Find chromosome end. */
    char *chromName = chromStart->tName;
    struct psl *psl, *dummy, **endAddress = &dummy;
    for (psl = chromStart; psl != NULL; psl = psl->next)
        {
	if (!sameString(psl->tName, chromName))
	    break;
	endAddress = &psl->next;
	}
    chromEnd = psl;

    /* Terminate list before next chromosome */
    *endAddress = NULL;

    /* Get chromosome sequence */
    uglyf("chrom %s\n", chromStart->tName); 
    struct dnaSeq *chrom = nibTwoCacheSeq(dnaCache, chromStart->tName);

    /* Create clusters. */
    struct ggMrnaCluster *clusterList = clusterPslsOnChrom(chromStart, chrom);

    /* Write clusters to file. */
    struct ggMrnaCluster *cluster;
    for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
         writeCluster(cluster, out);

    /* Free chromosome sequence. */
    dnaSeqFree(&chrom);

    /* Restore list. */
    *endAddress = chromEnd;
    }
carefulClose(&out);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
clusterPsl(argv[1], argv[2], argv[3]);
return 0;
}

/* txGeneCanonical - Pick a canonical version of each gene - that is the form to use when just interested in a single splicing varient.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "binRange.h"
#include "rangeTree.h"
#include "options.h"
#include "bed.h"
#include "txGraph.h"
#include "txCluster.h"

static char const rcsid[] = "$Id: txGeneCanonical.c,v 1.1 2007/03/03 17:21:51 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneCanonical - Pick a canonical version of each gene - that is the form\n"
  "to use when just interested in a single splicing varient. Produces final\n"
  "transcript clusters as well.\n"
  "usage:\n"
  "   txGeneCanonical coding.cluster noncoding.txg genes.bed canonical.tab isoforms.tab txCluster.tab\n"
  "where:\n"
  "   coding.cluster is clusters of all the coding genes, from txCdsCluster\n"
  "   noncoding.txg is a txGraph of noncoding genes from txBedToGraph run on the\n"
  "       noncoding output of txGeneSeparateNoncoding\n"
  "   genes.bed contains all the transcripts\n"
  "   canonical.tab contains the 'best' trancript from each cluster\n"
  "   isoforms.tab associates transcripts and clusters\n"
  "   txCluster.tab contains a little info on each cluster\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void txGeneCanonical(char *codingCluster, char *noncodingGraph, char *genesBed,
	char *outCanonical, char *outIsoforms, char *outClusters)
/* txGeneCanonical - Pick a canonical version of each gene - that is the form to use 
 * when just interested in a single splicing varient. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
txGeneCanonical(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}

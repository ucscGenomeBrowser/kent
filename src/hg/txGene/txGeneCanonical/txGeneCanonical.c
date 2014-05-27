/* txGeneCanonical - Pick a canonical version of each gene - that is the form
 * to use when just interested in a single splicing varient. Produces final
 * transcript clusters as well. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "binRange.h"
#include "options.h"
#include "rangeTree.h"
#include "obscure.h"
#include "bed.h"
#include "txGraph.h"
#include "txCluster.h"
#include "txInfo.h"
#include "minChromSize.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneCanonical - Pick a canonical version of each gene - that is the form\n"
  "to use when just interested in a single splicing varient. Produces final\n"
  "transcript clusters as well.\n"
  "usage:\n"
  "   txGeneCanonical coding.cluster genes.info noncoding.txg genes.bed canonical.tab isoforms.tab txCluster.tab\n"
  "where:\n"
  "   coding.cluster is clusters of all the coding genes, from txCdsCluster\n"
  "   noncoding.txg is a txGraph of noncoding genes from txBedToGraph run on the\n"
  "       noncoding output of txGeneSeparateNoncoding\n"
  "   genes.bed contains all the transcripts\n"
  "   nearCoding.bed contains the nearCoding output of txGeneSeparateNoncoding\n"
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

struct gene
/* Our own gene structure.  Contains a list of transcripts
 * mostly. */
    {
    struct gene *next;	/* Next in list */
    char *chrom;	/* Chromosome name */
    int start;		/* Chromosome start */
    int end;		/* Chromosome end */
    char strand;	/* Strand: + or - */
    boolean isCoding;	/* True if coding. */
    struct bed *txList; /* List of transcripts */
    struct rbTree *exonTree;  /* Contains regions covered by exons. */
    struct bed *niceTx; /* A nice transcript to use as a representative. */
    };

struct gene *geneNew()
/* Create empty gene. */
{
struct gene *gene;
AllocVar(gene);
gene->exonTree = rangeTreeNew();
return gene;
}

void geneAddBed(struct gene *gene, struct bed *bed)
/* Add a bed into gene. */
{
slAddHead(&gene->txList, bed);
bedIntoRangeTree(bed, gene->exonTree);
}

int scoreNoncodingBed(struct bed *bed)
/* Score noncoding bed weighing number of exons and total size. */
{
return bed->blockCount*100 + bedTotalBlockSize(bed);
}

struct gene *geneFromGraph(struct txGraph *graph, struct hash *bedHash)
/* Create noncoding gene from gene graph */
{
struct gene *gene = geneNew();
gene->chrom = cloneString(graph->tName);
gene->start = graph->tStart;
gene->end = graph->tEnd;
gene->strand = graph->strand[0];
int i;
int bestScore = 0;
for (i=0; i<graph->sourceCount; ++i)
    {
    struct bed *bed = hashMustFindVal(bedHash, graph->sources[i].accession);
    geneAddBed(gene, bed);
    int score = scoreNoncodingBed(bed);
    if (score > bestScore)
        {
	bestScore = score;
	gene->niceTx = bed;
	}
    }
return gene;
}

struct gene *geneFromCluster(struct txCluster *cluster, struct hash *bedHash, 
	struct hash *infoHash)
/* Create a coding gene from coding cluster. */
{
struct gene *gene = geneNew();
gene->chrom = cloneString(cluster->chrom);
gene->start = cluster->chromStart;
gene->end = cluster->chromEnd;
gene->strand = cluster->strand[0];
gene->isCoding = TRUE;
int i;
double bestScore = -BIGNUM;
for (i=0; i<cluster->txCount; ++i)
    {
    /* Find and add bed. */
    struct bed *bed = hashMustFindVal(bedHash, cluster->txArray[i]);
    geneAddBed(gene, bed);

    /* Recalc min/max of gene since cluster has CDS, not transcription bounds. */
    gene->start = min(gene->start, bed->chromStart);
    gene->end = max(gene->end, bed->chromEnd);

    /* Figure out nicest gene in cluster to use as example. */
    struct txInfo *info = hashMustFindVal(infoHash, bed->name);
    double score = txInfoCodingScore(info, TRUE);
    if (score > bestScore)
         {
	 bestScore = score;
	 gene->niceTx = bed;
	 }
    }
return gene;
}

int geneCmp(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct gene *a = *((struct gene **)va);
const struct gene *b = *((struct gene **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

struct gene *mostOverlappingGene(struct hash *keeperHash, struct bed *bed)
/* Find most overlapping gene in hash of keepers of genes. */
{
struct binKeeper *bk = hashMustFindVal(keeperHash, bed->chrom);
struct binElement *bin, *binList = binKeeperFind(bk, bed->chromStart, bed->chromEnd);
int bestOverlap = 0;
struct gene *bestGene = NULL;
for (bin = binList; bin != NULL; bin = bin->next)
    {
    struct gene *gene = bin->val;
    if (gene->strand == bed->strand[0])
	{
	int overlap = bedRangeTreeOverlap(bed, gene->exonTree);
	if (overlap > bestOverlap)
	    {
	    bestOverlap = overlap;
	    bestGene = gene;
	    }
	}
    }
slFreeList(&binList);
return bestGene;
}

void txGeneCanonical(char *codingCluster, char *infoFile, 
	char *noncodingGraph, char *genesBed, char *nearCoding, 
	char *outCanonical, char *outIsoforms, char *outClusters)
/* txGeneCanonical - Pick a canonical version of each gene - that is the form
 * to use when just interested in a single splicing varient. Produces final
 * transcript clusters as well. */
{
/* Read in input into lists in memory. */
struct txCluster *coding, *codingList = txClusterLoadAll(codingCluster);
struct txGraph *graph, *graphList = txGraphLoadAll(noncodingGraph);
struct bed *bed, *nextBed, *bedList = bedLoadNAll(genesBed, 12);
struct txInfo *info, *infoList = txInfoLoadAll(infoFile);
struct bed *nearList = bedLoadNAll(nearCoding, 12);

/* Make hash of all beds. */
struct hash *bedHash = hashNew(18);
for (bed = bedList; bed != NULL; bed = bed->next)
    hashAdd(bedHash, bed->name, bed);

/* Make has of all info. */
struct hash *infoHash = hashNew(18);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);

/* Make a binKeeper structure that we'll populate with coding genes. */
struct hash *sizeHash = minChromSizeFromBeds(bedList);
struct hash *keeperHash = minChromSizeKeeperHash(sizeHash);

/* Make list of coding genes and toss them into binKeeper.
 * This will eat up bed list, but bedHash is ok. */
struct gene *gene, *geneList = NULL;
for (coding = codingList; coding != NULL; coding = coding->next)
    {
    gene = geneFromCluster(coding, bedHash, infoHash);
    slAddHead(&geneList, gene);
    struct binKeeper *bk = hashMustFindVal(keeperHash, gene->chrom);
    binKeeperAdd(bk, gene->start, gene->end, gene);
    }

/* Go through near-coding genes and add them to the coding gene
 * they most overlap. */
for (bed = nearList; bed != NULL; bed = nextBed)
    {
    nextBed = bed->next;
    gene = mostOverlappingGene(keeperHash, bed);
    if (gene == NULL)
        errAbort("%s is near coding, but doesn't overlap any coding!?", bed->name);
    geneAddBed(gene, bed);
    }

/* Add non-coding genes. */
for (graph = graphList; graph != NULL; graph = graph->next)
    {
    gene = geneFromGraph(graph, bedHash);
    slAddHead(&geneList, gene);
    }

/* Sort so it all looks nicer. */
slSort(&geneList, geneCmp);

/* Open up output files. */
FILE *fCan = mustOpen(outCanonical, "w");
FILE *fIso = mustOpen(outIsoforms, "w");
FILE *fClus = mustOpen(outClusters, "w");

/* Loop through, making up gene name, and writing output. */
int geneId = 0;
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    /* Make up name. */
    char name[16];
    safef(name, sizeof(name), "g%05d", ++geneId);

    /* Reverse transcript list just to make it look better. */
    slReverse(&gene->txList);

    /* Write out canonical file output */
    bed = hashMustFindVal(bedHash, gene->niceTx->name);
    fprintf(fCan, "%s\t%d\t%d\t%d\t%s\t%s\n",
    	bed->chrom, bed->chromStart, bed->chromEnd, geneId,
	gene->niceTx->name, gene->niceTx->name);

    /* Write out isoforms output. */
    for (bed = gene->txList; bed != NULL; bed = bed->next)
        fprintf(fIso, "%d\t%s\n", geneId, bed->name);

    /* Write out cluster output, starting with bed 6 standard fields. */
    fprintf(fClus, "%s\t%d\t%d\t%s\t%d\t%c\t",
    	gene->chrom, gene->start, gene->end, name, 0, gene->strand);

    /* Write out thick-start/thick end. */
    if (gene->isCoding)
        {
	int thickStart = gene->end, thickEnd  = gene->start;
	for (bed = gene->txList; bed != NULL; bed = bed->next)
	    {
	    if (bed->thickStart < bed->thickEnd)
	        {
		thickStart = min(thickStart, bed->thickStart);
		thickEnd = max(thickEnd, bed->thickEnd);
		}
	    }
	fprintf(fClus, "%d\t%d\t", thickStart, thickEnd);
	}
    else
        {
	fprintf(fClus, "%d\t%d\t", gene->start, gene->start);
	}

    /* We got no rgb value, just write out zero. */
    fprintf(fClus, "0\t");

    /* Get exons from exonTree. */
    struct range *exon, *exonList = rangeTreeList(gene->exonTree);
    fprintf(fClus, "%d\t", slCount(exonList));
    for (exon = exonList; exon != NULL; exon = exon->next)
	fprintf(fClus, "%d,", exon->start - gene->start);
    fprintf(fClus, "\t");
    for (exon = exonList; exon != NULL; exon = exon->next)
	fprintf(fClus, "%d,", exon->end - exon->start);
    fprintf(fClus, "\t");

    /* Write out associated transcripts. */
    fprintf(fClus, "%d\t", slCount(gene->txList));
    for (bed = gene->txList; bed != NULL; bed = bed->next)
        fprintf(fClus, "%s,", bed->name);
    fprintf(fClus, "\t");

    /* Write out nice value */
    fprintf(fClus, "%s\t", gene->niceTx->name);

    /* Write out coding/noncoding value. */
    fprintf(fClus, "%d\n", gene->isCoding);
    }

/* Close up files. */
carefulClose(&fCan);
carefulClose(&fIso);
carefulClose(&fClus);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 9)
    usage();
txGeneCanonical(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
return 0;
}

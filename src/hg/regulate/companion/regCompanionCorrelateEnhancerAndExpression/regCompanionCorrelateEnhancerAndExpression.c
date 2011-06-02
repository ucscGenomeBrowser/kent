/* regCompanionCorrelateEnhancerAndExpression - Correlate files with info on enhancer activity 
 * and gene txn levels. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "chromBins.h"
#include "basicBed.h"
#include "correlate.h"
#include "rangeTree.h"
#include "obscure.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

double minR = 0.7071;
double minExp = 10;
double minAct = 15;
int maxDist = 50000;	/* Max distance we allow between enhancer and promoter. */
char *permuteTest = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionCorrelateEnhancerAndExpression - Correlate files with info on enhancer activity\n"
  "and gene transcription levels.\n"
  "usage:\n"
  "   regCompanionCorrelateEnhancerAndExpression enh.bed enh.levels gene.bed gene.levels out.tab\n"
  "The out.tab has the following columns\n"
  "   <enhId> <geneId> <promoter to enhancer distance> <max expn> <max act><level correlation>\n"
  "The out.bed is a non-stranded, multi-block bed file with the thick block being the\n"
  "enhancer and the thin blocks being the promoters correlated with it\n"
  "options:\n"
  "   -minR=0.N (default %g) Minimum threshold of correlation to output in bed file\n"
  "   -minExp=N (default %g) Minimum expression level in some cell to output in bed file\n"
  "   -minAct=N (default %g) Minimum activation level in some cell to output in bed file\n"
  "   -maxDist=N (default %d) Maximum distance between promoter and enhancer\n"
  "   -outPairBed=outPair.bed - output bed file with a record for each enhancer->promoter\n"
  "   -outEnhBed=outEnh.bed - output bed file with a record for each enhancer, possibly including\n"
  "              multiple promoters per enhancer\n"
  "   -outProBed=outPro.bed - output bed file with a record for each promoter, possibly including\n"
  "              multiple enhancers per promoter\n"
  "   -outOverBed=outOver.bed -output bed file with enhancers that are over thresholds\n"
  "   -permuteTest=permute.tab perform a permutation test 100x and output # of pairs and\n"
  "                number of pairs with  R >= minR\n"
  , minR, minExp, minAct, maxDist
  );
}

static struct optionSpec options[] = {
   {"minR", OPTION_DOUBLE},
   {"minExp", OPTION_DOUBLE},
   {"minAct", OPTION_DOUBLE},
   {"maxDist", OPTION_INT},
   {"outPairBed", OPTION_STRING},
   {"outEnhBed", OPTION_STRING},
   {"outProBed", OPTION_STRING},
   {"outOverBed", OPTION_STRING},
   {"permuteTest", OPTION_STRING},
   {NULL, 0},
};

int cellCount = 7;	/* Number of cell lines in our data. */
int proPad = 50;	/* Define promoter as this much on either side of txStart. */

struct hash *readLevelsIntoHash(char *fileName)
/* Read file and return hash keyed by the first column (gene name) and
 * with values an array of cellCount doubles witht he gene levels. */
{
struct hash *hash = hashNew(16);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[cellCount+1];
while (lineFileRow(lf, row))
    {
    char *id = row[0];
    double *vals;
    AllocArray(vals, cellCount);
    int i;
    for (i=0; i<cellCount; ++i)
        vals[i] = lineFileNeedDouble(lf, row, i+1);
    hashAdd(hash, id, vals);
    }
lineFileClose(&lf);
return hash;
}

double aveArray(double *array, int size)
/* Return average of array of given size. */
{
double sum = 0;
int i;
for (i=0; i<size; ++i)
    sum += array[i];
return sum/size;
}

double maxArray(double *array, int size)
/* Return average of array of given size. */
{
double maxVal = array[0];
int i;
for (i=1; i<size; ++i)
    {
    double val = array[i];
    if (val > maxVal)
        maxVal = val;
    }
return maxVal;
}

struct bed *rangeListToBed(struct range *rangeList, char *name, char *chrom, char strand, int score)
/* Convert a list of ranges to a bed12. */
{
if (rangeList == NULL)
    return NULL;

/* Figure out number of blocks and overall bounds. */
int blockCount = 0;
int totalSize = 0;
struct range *range = NULL, *lastRange = NULL;
for (range = rangeList; range != NULL; range = range->next)
    {
    ++blockCount;
    totalSize += range->end - range->start;
    lastRange = range;
    }

int chromStart = rangeList->start;
int chromEnd = lastRange->end;

/* Allocate bed and fill in most of it. */
struct bed *bed;
AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = chromStart;
bed->chromEnd = chromEnd;
bed->name = cloneString(name);
bed->score = score;
bed->strand[0] = strand;
bed->blockCount = blockCount;

/* Fill in sizes and starts arrays */
int *sizes = AllocArray(bed->blockSizes, blockCount);
int *starts = AllocArray(bed->chromStarts, blockCount);
int i = 0;
for (range = rangeList; range != NULL; range = range->next)
    {
    if (range->end > range->start)
	{
	sizes[i] = range->end - range->start;
	starts[i] = range->start - chromStart;
	++i;
	}
    }

return bed;
}

void printLinkingBed(FILE *f, struct bed *enh, struct bed *gene, double r)
/* Print out a two-block bed that links a thin enhancer to a fat promoter at
 * score proportional to the correlation, r. */
{
int promoPos = (gene->strand[0] == '-' ? gene->chromEnd : gene->chromStart);
int promoStart = promoPos - proPad, promoEnd = promoPos + proPad;
struct bed link;
ZeroVar(&link);
char nameBuf[256];
char *geneNoVersion = cloneStringZ(gene->name, 8);
safef(nameBuf, sizeof(nameBuf), "e%s->%8s", enh->name+3, geneNoVersion);
freez(&geneNoVersion);
link.chrom = gene->chrom;
link.name = nameBuf;
int chromStarts[2], blockSizes[2];
link.chromStarts = chromStarts;
link.blockSizes = blockSizes;
link.blockCount = 2;
link.score = 1000 * r;
link.thickStart = promoStart;
link.thickEnd = promoEnd;
chromStarts[0] = 0;
if (enh->chromStart < promoPos)
    {
    link.strand[0] = '+';
    link.chromStart = enh->chromStart;
    link.chromEnd = promoEnd;
    chromStarts[1] = promoStart - link.chromStart;
    blockSizes[0] = enh->chromEnd - enh->chromStart;
    blockSizes[1] = promoEnd - promoStart;
    }
else
    {
    link.strand[0] = '-';
    link.chromStart = promoStart;
    link.chromEnd = enh->chromEnd;
    chromStarts[1] = enh->chromStart - link.chromStart;
    blockSizes[0] = promoEnd - promoStart;
    blockSizes[1] = enh->chromEnd - enh->chromStart;
    }
bedTabOutN(&link, 12, f);
}

struct enhForGene
/* For a given gene, a list of enhancers. */
    {
    struct enhForGene *next;
    struct bed *gene;		/*  gene record. */
    struct slRef *enhList;	/* List of references to enhancer beds. */
    };

struct hash *permuteHash(struct hash *inHash)
/* Return a version of inHash with the same keys, but the values shuffled. */
{
struct hash *outHash = hashNew(18);
struct hashEl *inEl, *inList = hashElListHash(inHash);
struct slRef *refList = NULL, *ref;
for (inEl = inList; inEl != NULL; inEl = inEl->next)
    refAdd(&refList, inEl->val);
shuffleList(&refList, 1);
for (inEl = inList, ref=refList; inEl != NULL; inEl=inEl->next, ref=ref->next)
    hashAdd(outHash,inEl->name, ref->val);
slFreeList(&refList);
return outHash;
}

void doPermuteTest(char *fileName, struct bed *enhList, struct chromBins *geneBins, 
	struct hash *geneLevelHash, struct hash *enhLevelHash, int repCount, double goodLevel)
/* Permute gene/expn levels repeatedly and report results. */
{
FILE *f = mustOpen(fileName, "w");
int i;
for (i=0; i<repCount; ++i)
    {
    struct hash *permutedHash = permuteHash(geneLevelHash);
    int goodCount = 0, allCount = 0;
    struct bed *enh;
    for (enh = enhList; enh != NULL; enh = enh->next)
	{
	/* Get region to query */
	char *chrom = enh->chrom;
	int center = (enh->chromStart + enh->chromEnd)/2;
	int start = center - maxDist;
	if (start < 0) start = 0;
	int end = center + maxDist;

	double *enhVals = hashMustFindVal(enhLevelHash, enh->name);
	struct binElement *el, *elList = chromBinsFind(geneBins, chrom, start, end);
	for (el = elList; el != NULL; el = el->next)
	    {
	    struct bed *gene = el->val;
	    double *geneVals = hashMustFindVal(permutedHash, gene->name);
	    double r = correlateArrays(enhVals, geneVals, cellCount);
	    if (r >= goodLevel)
	       ++goodCount;
	    ++allCount;
	    }
        }
    fprintf(f, "%d\t%d\n", allCount, goodCount);
    hashFree(&permutedHash);
    }
carefulClose(&f);
}

void regCompanionCorrelateEnhancerAndExpression(char *enhBed, char *enhLevels, 
	char *geneBed, char *geneLevels, char *outTab)
/* regCompanionCorrelateEnhancerAndExpression - Correlate files with info on enhancer activity 
 * and gene txn levels. */
{
/* Figure out what additional outputs may be needed. */
char *outProBed = optionVal("outProBed", NULL);
char *outEnhBed = optionVal("outEnhBed", NULL);
char *outPairBed = optionVal("outPairBed", NULL);
char *outOverBed = optionVal("outOverBed", NULL);

/* Read genes into a chromBin object to be able to find quickly. Just store the promoter */
struct bed *gene, *geneList = bedLoadAll(geneBed);
struct chromBins *geneBins = chromBinsNew(NULL);
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    int start = (gene->strand[0] == '-' ? gene->chromEnd-1 : gene->chromStart);
    chromBinsAdd(geneBins, gene->chrom, start, start+1, gene);
    }

/* Read levels into hash */
struct hash *geneLevelHash = readLevelsIntoHash(geneLevels);
struct hash *enhLevelHash = readLevelsIntoHash(enhLevels);

/* Load enhancer bed. */
struct bed *enh, *enhList = bedLoadAll(enhBed);

/* Open up outputs. */
FILE *fTab = mustOpen(outTab, "w");
FILE *fPair = (outPairBed == NULL ? NULL : mustOpen(outPairBed, "w"));
FILE *fEnh = (outEnhBed == NULL ? NULL : mustOpen(outEnhBed, "w"));
FILE *fPro = (outProBed == NULL ? NULL : mustOpen(outProBed, "w"));
FILE *fOver = (outOverBed == NULL ? NULL : mustOpen(outOverBed, "w"));

/* Set up additional stuff for promoter-centered output. */
struct hash *proHash = hashNew(16);
struct enhForGene *enhGeneList = NULL, *enhGene;

/* Stream through input enhancers, finding genes within 50kb, and doing correlations. */
for (enh = enhList; enh != NULL; enh = enh->next)
    {
    boolean outputOver = FALSE;	/* Keep track if have output this one in fOver. */

    /* Get region to query */
    char *chrom = enh->chrom;
    int center = (enh->chromStart + enh->chromEnd)/2;
    int start = center - maxDist;
    if (start < 0) start = 0;
    int end = center + maxDist;

    double *enhVals = hashMustFindVal(enhLevelHash, enh->name);
    double maxAct = maxArray(enhVals, cellCount);
    struct binElement *el, *elList = chromBinsFind(geneBins, chrom, start, end);
    struct range *correlatingPromoterList = NULL, *range;
    for (el = elList; el != NULL; el = el->next)
        {
	struct bed *gene = el->val;
	double *geneVals = hashMustFindVal(geneLevelHash, gene->name);
	double r = correlateArrays(enhVals, geneVals, cellCount);
	double maxExp = maxArray(geneVals, cellCount);
	int distance;
	if (r >= minR && maxExp >= minExp && maxAct >= minAct)
	    {
	    if (fOver)
		{
		if (!outputOver)
		    {
		    fprintf(fOver, "%s\t%d\t%d\t%s\t%d\t%s\n", enh->chrom, 
		    	enh->chromStart, enh->chromEnd, enh->name, round(r*1000), gene->strand);
		    outputOver = TRUE;
		    }
		}
	    if (fPair)
		printLinkingBed(fPair, enh, gene, r);
	    if (fEnh)
		{
		int promoPos = (gene->strand[0] == '-' ? gene->chromEnd : gene->chromStart);
		AllocVar(range);
		range->start = promoPos - proPad;
		range->end = promoPos + proPad;
		slAddHead(&correlatingPromoterList, range);
		}
	    if (fPro)
	        {
		enhGene = hashFindVal(proHash, gene->name);
		if (enhGene == NULL)
		    {
		    AllocVar(enhGene);
		    enhGene->gene = gene;
		    slAddHead(&enhGeneList, enhGene);
		    hashAdd(proHash, gene->name, enhGene);
		    }
		refAdd(&enhGene->enhList, enh);
		}
	    }
	if (gene->strand[0] == '-')
	    distance = gene->chromEnd - center;
	else
	    distance = center - gene->chromStart;
	fprintf(fTab, "%s\t%s\t%d\t%g\t%g\t%g\n", 
		enh->name, gene->name, distance, maxExp, maxAct, r);
	}

    if (correlatingPromoterList != NULL)
        {
	/* Cool, we got a decent connection.  Make entry in bed file. */

	/* First we load it into a rangeTree.  This is just to do any merging if 
	 * needed between promoters and or enhancer areas, and also to sort. */
	struct rbTree *rt = rangeTreeNew();
	for (range = correlatingPromoterList; range != NULL; range = range->next)
	    rangeTreeAdd(rt, range->start, range->end);
	rangeTreeAdd(rt, enh->chromStart, enh->chromEnd);

	/* Then get it back out of range tree and convert to bed. */
	struct range *rangeList = rangeTreeList(rt);
	struct bed *enhBed = rangeListToBed(rangeList, enh->name, chrom, '.', 0);
	enhBed->thickStart = enh->chromStart;
	enhBed->thickEnd = enh->chromEnd;

	/* Save it. */
	bedTabOutN(enhBed, 12, fEnh);

	/* Clean up a little. */
	slFreeList(&correlatingPromoterList);
	rangeTreeFree(&rt);
	}
    }

/* Finally put out the promoter centered file. The enhGeneList will be empty if no output. */
slReverse(&enhGeneList);
for (enhGene = enhGeneList; enhGene != NULL; enhGene = enhGene->next)
    {
    /* Make range tree with the promoter and all the enhancers. */
    struct bed *gene = enhGene->gene;
    int proCenter = (gene->strand[0] == '-' ? gene->chromEnd : gene->chromStart);
    struct rbTree *rt = rangeTreeNew();
    rangeTreeAdd(rt, proCenter-proPad, proCenter+proPad);
    struct slRef *ref;
    for (ref = enhGene->enhList; ref != NULL; ref = ref->next)
        {
	struct bed *enh = ref->val;
	rangeTreeAdd(rt, enh->chromStart, enh->chromEnd);
	}

    /* Convert to bed and set thick start/end to the promoter. */
    struct range *rangeList = rangeTreeList(rt);
    struct bed *bed = rangeListToBed(rangeList, gene->name, gene->chrom, '.', 0);
    bed->thickStart = proCenter - proPad;
    bed->thickEnd = proCenter + proPad;

    /* Save it. */
    bedTabOutN(bed, 12, fPro);

    /* Clean up. */
    rangeTreeFree(&rt);
    }

if (permuteTest != NULL)
    {
    verbose(1, "Permuting...\n");
    doPermuteTest(permuteTest, enhList, geneBins, geneLevelHash, enhLevelHash, 1000, minR);
    }

carefulClose(&fTab);
carefulClose(&fPair);
carefulClose(&fEnh);
carefulClose(&fPro);
carefulClose(&fOver);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
minR = optionDouble("minR", minR);
minExp = optionDouble("minExp", minExp);
minAct = optionDouble("minAct", minAct);
maxDist = optionDouble("maxDist", maxDist);
permuteTest = optionVal("permuteTest", NULL);
regCompanionCorrelateEnhancerAndExpression(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}

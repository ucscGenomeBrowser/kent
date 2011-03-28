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

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

double minR = 0.7071;
double minExp = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionCorrelateEnhancerAndExpression - Correlate files with info on enhancer activity\n"
  "and gene transcription levels.\n"
  "usage:\n"
  "   regCompanionCorrelateEnhancerAndExpression enh.bed enh.levels gene.bed gene.levels out.tab out.bed\n"
  "The out.tab has the following columns\n"
  "   <enhId> <geneId> <distance from promoter to enhancer> <average expn> <level correlation>\n"
  "The out.bed is a non-stranded, multi-block bed file with the thick block being the\n"
  "enhancer and the thin blocks being the promoters correlated with it\n"
  "options:\n"
  "   -minR=0.N (default %g) Minimum threshold of correlation to output in bed file\n"
  "   -minExp=N (default %g) Minimum expression level in some cell to output in bed file\n"
  , minR, minExp
  );
}

static struct optionSpec options[] = {
   {"minR", OPTION_DOUBLE},
   {"minExp", OPTION_DOUBLE},
   {NULL, 0},
};

int cellCount = 7;
int maxDist = 50000;

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

void regCompanionCorrelateEnhancerAndExpression(char *enhBed, char *enhLevels, 
	char *geneBed, char *geneLevels, char *outTab, char *outBed)
/* regCompanionCorrelateEnhancerAndExpression - Correlate files with info on enhancer activity 
 * and gene txn levels. */
{
/* Read genes into a chromBin object to be able to find quickly. Just store the promoter */
struct bed *gene, *geneList = bedLoadAll(geneBed);
struct chromBins *chromBins = chromBinsNew(NULL);
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    int start = (gene->strand[0] == '-' ? gene->chromEnd-1 : gene->chromStart);
    chromBinsAdd(chromBins, gene->chrom, start, start+1, gene);
    }

/* Read levels into hash */
struct hash *geneLevelHash = readLevelsIntoHash(geneLevels);
struct hash *enhLevelHash = readLevelsIntoHash(enhLevels);

/* Stream through enhBed, finding genes within 50kb, and doing correlations. */
struct bed *enh, *enhList = bedLoadAll(enhBed);
FILE *fTab = mustOpen(outTab, "w");
FILE *fBed = mustOpen(outBed, "w");
for (enh = enhList; enh != NULL; enh = enh->next)
    {
    /* Get region to query */
    char *chrom = enh->chrom;
    int center = (enh->chromStart + enh->chromEnd)/2;
    int start = center - maxDist;
    if (start < 0) start = 0;
    int end = center + maxDist;

    double *enhVals = hashMustFindVal(enhLevelHash, enh->name);
    struct binElement *el, *elList = chromBinsFind(chromBins, chrom, start, end);
    struct range *correlatingPromoterList = NULL, *range;
    for (el = elList; el != NULL; el = el->next)
        {
	struct bed *gene = el->val;
	double *geneVals = hashMustFindVal(geneLevelHash, gene->name);
	double r = correlateArrays(enhVals, geneVals, cellCount);
	double aveExp = aveArray(geneVals, cellCount);
	double maxExp = maxArray(geneVals, cellCount);
	int distance;
	if (r >= minR && maxExp >= minExp)
	    {
	    int promoPos = (gene->strand[0] == '-' ? gene->chromEnd : gene->chromStart);
	    AllocVar(range);
	    range->start = promoPos - 100;
	    range->end = promoPos + 100;
	    slAddHead(&correlatingPromoterList, range);
	    }
	if (gene->strand[0] == '-')
	    distance = gene->chromEnd - center;
	else
	    distance = center - gene->chromStart;
	fprintf(fTab, "%s\t%s\t%d\t%g\t%g\n", enh->name, gene->name, distance, aveExp, r);
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
	bedTabOutN(enhBed, 12, fBed);

	/* Clean up a little. */
	slFreeList(&correlatingPromoterList);
	}
    }
carefulClose(&fTab);
carefulClose(&fBed);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
minR = optionDouble("minR", minR);
minExp = optionDouble("minExp", minExp);
regCompanionCorrelateEnhancerAndExpression(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}

/* regCompanionChia - Analyse chia pet data against promoters and enhancers.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "basicBed.h"
#include "bigWig.h"
#include "obscure.h"
#include "localmem.h"
#include "verbose.h"
#include "bits.h"
#include "genomeRangeTree.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionChia - Analyse chia pet data against promoters and enhancers.\n"
  "usage:\n"
  "   regCompanionChia input.bed output.tab\n"
  "where input.bed has to be all two-block items.  There's alas 7 other input files\n"
  "which you'll have to recompile the C source to change.  The output is also complex\n"
  "a table with 23 fields:\n"
  "  1 - chromosome\n"
  "  2 - chromosome start - 0 based\n"
  "  3 - chromosome end - 1 based\n"
  "  4 - name - unique id for this pair\n"
  "  5 - score - 0-1000 browser style score\n"
  "  6 - always the word \"block1\"\n"
  "  7 - size of block 1\n"
  "  8 - average DNAse level in block 1\n"
  "  9 - average H3K4Me3 level in block 1\n"
  " 10 - average H3K27Ac level in block 1\n"
  " 11 - average H3K4Me1 level in block 1\n"
  " 12 - average CTCF level in block 1\n"
  " 13 - average coverage by +-500 base from txStart promoters in block 1\n"
  " 14 - average coverage by enhancer in block 1\n"
  " 15 - always the word \"block2\"\n"
  " 16 - size of block 2\n"
  " 17 - average DNAse level in block 2\n"
  " 18 - average H3K4Me3 level in block 2\n"
  " 19 - average H3K27Ac level in block 2\n"
  " 20 - average H3K4Me1 level in block 2\n"
  " 21 - average CTCF level in block 2\n"
  " 22 - average coverage by +-500 base from txStart promoters in block 2\n"
  " 23 - average coverage by enhancer in block 2\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

enum inType {itBlockedBed, itBigWig, itPromoterBed, itUnstrandedBed};

struct inInfo
/* Help classify input types. */
    {
    char *name;			/* Name of input */
    enum inType type;		/* Type of input */
    char *fileName;			/* Path of file with input */
    struct lineFile *lf;	/* Open lineFile if a bed */
    struct bbiFile *bbi;	/* Open bbiFile if a bigWig */
    double *out[2];	/* Average of signal for each of two blocks. */
    };

struct inInfo inArray[] =
    {
    {"pairs", itBlockedBed, NULL},
    // {"CHIApet", itBlockedBed, "/hive/groups/encode/dcc/analysis/ftp/pipeline/hg19/wgEncodeGisChiaPet/wgEncodeGisChiaPetGm12878D000005628.bed.gz"},
    {"DNAse", itBigWig, "/hive/users/kent/regulate/companion/dnase/normalized/Gm12878.bw"},
    {"H3K4Me3", itBigWig, "/hive/users/kent/regulate/companion/histones/normalized/wgEncodeBroadHistoneGm12878H3k4me3NormSig.bw"},
    {"H3K27Ac", itBigWig, "/hive/users/kent/regulate/companion/histones/normalized/wgEncodeBroadHistoneGm12878H3k27acNormSig.bw"},
    {"H3K4Me1", itBigWig, "/hive/users/kent/regulate/companion/histones/normalized/wgEncodeBroadHistoneGm12878H3k4me1NormSig.bw"},
    {"CTCF", itBigWig, "/hive/users/kent/regulate/companion/histones/normalized/wgEncodeBroadHistoneGm12878CtcfNormSig.bw"},
    {"UCSCgenes", itPromoterBed, "/hive/data/genomes/hg19/bed/ucsc.12/ucscGenes.bed"},
    {"UCSCenh", itUnstrandedBed, "/hive/users/kent/regulate/companion/enhPicks/stringent/enh7.bed"},
    };

#ifdef SOON
#endif /* SOON */

void checkInputOpenFiles(struct inInfo *array, int count)
/* Make sure all of the input is there and of right format before going forward. Since
 * this is going to take a while we want to fail fast. */
{
int i;
for (i=0; i<count; ++i)
    {
    struct inInfo *in = &array[i];
    switch (in->type)
        {
	case itBigWig:
	    {
	    /* Just open and close, it will abort if any problem. */
	    in->bbi = bigWigFileOpen(in->fileName);
	    break;
	    }
	case itPromoterBed:
	case itUnstrandedBed:
	case itBlockedBed:
	    {
	    struct lineFile *lf = in->lf = lineFileOpen(in->fileName, TRUE);
	    char *line;
	    lineFileNeedNext(lf, &line, NULL);
	    char *dupe = cloneString(line);
	    char *row[256];
	    int wordCount = chopLine(dupe, row);
	    struct bed *bed = NULL;
	    switch (in->type)
	        {
		case itPromoterBed:
		    lineFileExpectAtLeast(lf, 6, wordCount);
		    bed = bedLoadN(row, 6);
		    char strand = bed->strand[0];
		    if (strand != '+' && strand != '-')
		        errAbort("%s must be stranded, got %s in that field", lf->fileName, row[6]);
		    break;
		case itUnstrandedBed:
		    lineFileExpectAtLeast(lf, 4, wordCount);
		    bed = bedLoadN(row, 4);
		    break;
		case itBlockedBed:
		    lineFileExpectAtLeast(lf, 4, wordCount);
		    bed = bedLoadN(row, 12);
		    break;
		default:
		    internalErr();
		    break;
		}
	    bedFree(&bed);
	    freez(&dupe);
	    lineFileReuse(lf);
	    break;
	    }
	default:
	    internalErr();
	    break;
	}
    }
}

struct bed *bedLoadTwoBlocks(struct lineFile *lf)
/* Load a bed12, and make sure each item is 1 or 2 blocks.  Return list of
 * the two block ones. */
{
struct bed *bedList = NULL, *bed;
char *row[12];
int totalCount = 0, pairCount = 0;
while (lineFileRow(lf, row))
    {
    bed = bedLoad12(row);
    if (bed->blockCount != 1 && bed->blockCount != 2)
       errAbort("Line %d of %s got blockCount of %d,  expecting 1 or 2", 
       	lf->lineIx, lf->fileName, bed->blockCount);
    ++totalCount;
    if (bed->blockCount == 2)
        {
	slAddHead(&bedList, bed);
	++pairCount;
	}
    }
slReverse(&bedList);
verbose(1, "Got %d items including %d pairs in %s\n", totalCount, pairCount, lf->fileName);
return bedList;
}

struct genomeRangeTree *grtFromOpenBed(struct lineFile *lf, int size, boolean doPromoter)
/* Read an open bed file into a genomeRangeTree and return it. */
{
struct genomeRangeTree *grt = genomeRangeTreeNew();
char *row[size];
while (lineFileRow(lf, row))
    {
    struct bed *bed = bedLoadN(row, size);
    if (doPromoter)
        {
	if (bed->strand[0] == '+')
	    genomeRangeTreeAdd(grt, bed->chrom, bed->chromStart - 500, bed->chromEnd + 500);
	else
	    genomeRangeTreeAdd(grt, bed->chrom, bed->chromEnd - 500, bed->chromEnd + 500);
	}
    else
	genomeRangeTreeAdd(grt,  bed->chrom, bed->chromStart, bed->chromEnd);
    }
return grt;
}

void outputOverlappingGrt(struct bed *chiaList, struct genomeRangeTree *grt,
	double *out1, double *out2)
{
int chiaIx = 0;
struct bed *chia;
for (chia = chiaList; chia != NULL; chia = chia->next)
    {
    int blockStart = chia->chromStart;
    int blockSize = chia->blockSizes[0];
    int blockEnd = blockStart + blockSize;
    int overlap = genomeRangeTreeOverlapSize(grt, chia->chrom, blockStart, blockEnd);
    out1[chiaIx] = (double)overlap/blockSize;

    blockStart = chia->chromStart + chia->chromStarts[1];
    blockSize = chia->blockSizes[1];
    blockEnd = blockStart + blockSize;
    overlap = genomeRangeTreeOverlapSize(grt, chia->chrom, blockStart, blockEnd);
    out2[chiaIx] = (double)overlap/blockSize;

    ++chiaIx;
    }
}

void doItPromoterBed(struct inInfo *in, struct bed *chiaList, double *out1, double *out2)
/* Process overlaps with promoters defined by a gene bed file. */
{
struct genomeRangeTree *grt = grtFromOpenBed(in->lf, 12, TRUE);
outputOverlappingGrt(chiaList, grt, out1, out2);
uglyf("done %s\n", in->fileName);
}

void doItUnstrandedBed(struct inInfo *in, struct bed *chiaList, double *out1, double *out2)
/* Process overlaps with unstranded bed into output */
{
struct genomeRangeTree *grt = grtFromOpenBed(in->lf, 3, FALSE);
outputOverlappingGrt(chiaList, grt, out1, out2);
}

double averageInRegion(struct bigWigValsOnChrom *chromVals, int start, int size)
/* Return average value in region where there is data. */
{
int n = bitCountRange(chromVals->covBuf, start, size);
if (n == 0)
    return 0.0;
else
    {
    double *val = chromVals->valBuf + start;
    double sum = 0;
    int i = size;
    while (--i >= 0)
        sum += *val++;
    return sum/n;
    }
}

void doItBigWig(struct inInfo *in, struct bed *chiaList, struct bigWigValsOnChrom *chromVals,
	double *out1, double *out2)
{
struct bed *chromStart, *chromEnd, *chia;
int chiaIx = 0;
for (chromStart = chiaList; chromStart != NULL; chromStart = chromEnd)
    {
    chromEnd = bedListNextDifferentChrom(chromStart);
    if (bigWigValsOnChromFetchData(chromVals, chromStart->chrom, in->bbi))
        {
	for (chia = chromStart; chia != chromEnd; chia = chia->next)
	    {
	    int blockStart = chia->chromStart;
	    int blockSize = chia->blockSizes[0];
	    out1[chiaIx] = averageInRegion(chromVals, blockStart, blockSize);

	    blockStart = chia->chromStart + chia->chromStarts[1];
	    blockSize = chia->blockSizes[1];
	    out2[chiaIx] = averageInRegion(chromVals, blockStart, blockSize);

	    ++chiaIx;
	    }
	}
    else
	{
	/* No data on this chrom, just output zero everywhere. */
	for (chia = chromStart; chia != chromEnd; chia = chia->next)
	    {
	    out1[chiaIx] = out2[chiaIx] = 0;
	    ++chiaIx;
	    }
	}
    verboseDot();
    }
}


void regCompanionChia(char *inBedPairs, char *output)
/* regCompanionChia - Analyse chia pet data against promoters and enhancers.. */
{
inArray[0].fileName = inBedPairs;
checkInputOpenFiles(inArray, ArraySize(inArray));
verbose(1, "Opened all %d inputs successfully\n", (int)ArraySize(inArray));
struct bed *chiaList = bedLoadTwoBlocks(inArray[0].lf);
slSort(&chiaList, bedCmp);
int chiaCount = slCount(chiaList);
struct bigWigValsOnChrom *chromVals = bigWigValsOnChromNew();

int inIx;
for (inIx=1; inIx < ArraySize(inArray); ++inIx)
    {
    /* Allocate output arrays. */
    struct inInfo *in = &inArray[inIx];
    double *out1 = AllocArray(in->out[0], chiaCount);
    double *out2 = AllocArray(in->out[1], chiaCount);

    /* Process input depending on type. */
    verbose(1, "Processing %s", in->fileName);
    switch(in->type)
        {
	case itPromoterBed:
	    doItPromoterBed(in, chiaList, out1, out2);
	    break;
	case itUnstrandedBed:
	    doItUnstrandedBed(in, chiaList, out1, out2);
	    break;
	case itBigWig:
	    doItBigWig(in, chiaList, chromVals, out1, out2);
	    break;
	default:
	    internalErr();
	    break;
	}
    verbose(1, "\n");
    }

/* do output */
FILE *f = mustOpen(output, "w");
struct bed *chia;
int chiaIx = 0;
for (chia = chiaList; chia != NULL; chia = chia->next, ++chiaIx)
    {
    // fprintf(f, "%s\t%d\t%d\tchia%d\t%d", chia->chrom, chia->chromStart, chia->chromEnd, chiaIx+1, chia->score);
    fprintf(f, "%s\t%d\t%d\t%s\t%d", chia->chrom, chia->chromStart, chia->chromEnd, chia->name, chia->score);
    int blockIx;
    for (blockIx=0; blockIx < 2; ++blockIx)
	{
	fprintf(f, "\tblock%d\t%d", blockIx+1, chia->blockSizes[blockIx]);
	for (inIx=1; inIx < ArraySize(inArray); ++inIx)
	    {
	    struct inInfo *in = &inArray[inIx];
	    double *out = in->out[blockIx];
	    fprintf(f, "\t%g", out[chiaIx]);
	    }
	}
    fprintf(f, "\n");
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
regCompanionChia(argv[1], argv[2]);
return 0;
}

/* regionPicker - Code to pick regions to annotate deeply.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bits.h"
#include "dnautil.h"
#include "hdb.h"
#include "featureBits.h"
#include "axt.h"

struct region
/* Part of the genome. */
    {
    struct region *next;	/* Next item in list. */
    char *name;			/* Region name. */
    char *chrom;		/* Chromosome. */
    int start, end;		/* 0 based half open range */
    };

/* Command line overridable variables. */
char *clRegion = "genome";
int bigWinSize = 500000;
int bigStepSize = 100000;
int smallWinSize = 125;
double threshold = 0.8;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regionPicker - Code to pick regions to annotate deeply.\n"
  "Stratifies genome based on mouse non-transcribed homology\n"
  "and spliced EST density.\n"
  "usage:\n"
  "   regionPicker database axtBestDir output\n"
  "options:\n"
  "   -region=chrN - restrict to a single chromosome\n"
  "   -region=file - File has chrN:start-end on each line\n"
  "   -bigWinSize=N -default %d\n"
  "   -bigStepSize=N - default %d\n"
  "   -smallWinSize=N - default %d\n"
  "   -theshold=0.N - minimum base identity in small window. default %2.1f\n",
       bigWinSize, bigStepSize, smallWinSize, threshold
  );
}

#define histSize 1000
#define geneScale 5  /* Spread gene density from 0-20% instead of 0-100% */

struct stats
/* Keep stats. */
    {
    struct chromStats *next;
    int consCounts[histSize];
    int geneCounts[histSize];
    int bothCounts[histSize][histSize];
    int totalConsCount;
    int totalGeneCount;
    int totalBothCount;
    };
    
void sumStats(struct stats *a, struct stats *b)
/* Add a to b and return result in a. */
{
int i,j;
for (i=0; i<histSize; ++i)
    {
    a->consCounts[i] += b->consCounts[i];
    a->geneCounts[i] += b->geneCounts[i];
    for (j=0; j<histSize; ++j)
	a->bothCounts[i][j] += b->bothCounts[i][j];
    }
a->totalConsCount += b->totalConsCount;
a->totalGeneCount += b->totalGeneCount;
a->totalBothCount += b->totalBothCount;
}

void addToStats(struct stats *stats, Bits *aliBits, Bits *matchBits, 
	Bits *geneBits, Bits *seqBits, struct region *r, FILE *f)
/* Step big window through region adding to stats. */
{
char *chrom = r->chrom;
int chromStart = r->start;
int chromEnd = r->end;
int bigStart, bigEnd, smallStart, smallEnd;  
int aliCount, matchCount, geneCount, seqCount;
int bigWeight;
double consRatio = 0, geneRatio = 0;

/* Do some sanity checking/error reporting */
if (chromEnd < chromStart)
     errAbort("Out of range %s:%d-%d (%s)", 
     	chrom, chromStart, chromEnd, r->name);

fprintf(f, "%s\n", r->name);
for (bigStart = chromStart; bigStart < chromEnd;  bigStart += bigStepSize)
    {
    int smallPassing = 0;  /* Count of small windows passing %ID threshold */
    int smallGotData = 0;  /* Count of small windows with alignment data */
    int consIx = -1; /* Index into conservation histogram */
    int geneIx = -1; /* Index into gene density histogram */

    /* Figure out boundaries of big window,  and based on
     * size how much to weigh it in histogram */
    bigEnd = bigStart + bigWinSize;
    if (bigEnd > chromEnd) bigEnd = chromEnd;
    bigWeight = round(10.0 * (bigEnd - bigStart) / bigWinSize);


    /* Figure out number of non-N bases, and skip this window
     * if they amount to less than half of it. */
    seqCount = bitCountRange(seqBits, bigStart, bigEnd - bigStart);
    if (seqCount < (bigEnd - bigStart)/2)
        continue;

    /* Step through small windows inside big one to calculate
     * what percentage of small windows are conserved over
     * theshold. */
    for (smallStart = bigStart; smallStart < bigEnd; smallStart += smallWinSize)
        {
	smallEnd = smallStart + smallWinSize;
	if (smallEnd > chromEnd) smallEnd = chromEnd;
	aliCount = bitCountRange(aliBits, smallStart, smallEnd - smallStart);
	matchCount = bitCountRange(matchBits, smallStart, smallEnd - smallStart);
	/* See if enough of the small window aligns to
	 * calculate percentage of bases aligning
	 * accurately, and if so add small window to
	 * data set. */
	if (aliCount >= 0.75 * smallWinSize)
	    {
	    double ratio = (double) matchCount/aliCount;
	    smallGotData += 1;
	    if (ratio >= threshold)
		smallPassing += 1;
	    }
	}
    /* If a reasonable number of small windows have
     * data,  add statistics to conservation histogram. */
    if (smallGotData >= 50)
        {
	consRatio = (double) smallPassing/smallGotData;
	consIx = consRatio * histSize;
	if (consIx > histSize) consIx = histSize;
	stats->totalConsCount += bigWeight;
	stats->consCounts[consIx] += bigWeight;
	}

    /* Calculate gene density and save. */
    geneCount = bitCountRange(geneBits, bigStart, bigEnd - bigStart);
    geneRatio = (double)geneCount / seqCount;
    geneIx = geneRatio *  geneScale * histSize;
    if (geneIx > histSize) geneIx = histSize;
    stats->totalGeneCount += bigWeight;
    stats->geneCounts[geneIx] += bigWeight;

    /* If valid gene density and conservation data then
     * add it to two-dimensional histogram */
    if (geneIx >= 0 && consIx >= 0)
        {
	fprintf(f, "    %s:%d-%d ", chrom, bigStart+1, bigEnd);
	fprintf(f, "gene %4.1f%% consNotTx %4.1f%%\n", 100*geneRatio, 100*consRatio);
	stats->totalBothCount += bigWeight;
	stats->bothCounts[consIx][geneIx] += bigWeight;
	}
    }
}

void maskFeatures(struct sqlConnection *conn, char *chrom, int chromSize, Bits *maskBits)
/* Mask out bits we're not interested in for conservation. */
{
fbOrTableBits(maskBits, "gap", chrom, chromSize, conn);
fbOrTableBits(maskBits, "refGene:exon:12", chrom, chromSize, conn);
fbOrTableBits(maskBits, "mrna:exon:12", chrom, chromSize, conn);
fbOrTableBits(maskBits, "ensGene:exon:12", chrom, chromSize, conn);
fbOrTableBits(maskBits, "twinscan:exon:12", chrom, chromSize, conn);
fbOrTableBits(maskBits, "xenoMrna:exon:12", chrom, chromSize, conn);
fbOrTableBits(maskBits, "intronEst:exon:12", chrom, chromSize, conn);
fbOrTableBits(maskBits, "anyMrnaCov", chrom, chromSize, conn);
fbOrTableBits(maskBits, "rmsk", chrom, chromSize, conn);
printf("%s: %d bits masked\n", chrom, bitCountRange(maskBits, 0, chromSize));
}

void axtSetBits(struct axt *axt, int chromSize, Bits *aliBits, Bits *matchBits)
/* Set bits where there are alignments and matches. */
{
char q, t, *qSym = axt->qSym, *tSym = axt->tSym;
int i, symCount = axt->symCount;
int tPos = axt->tStart;

assert(axt->tStrand == '+');
for (i=0; i<symCount; ++i)
    {
    assert(tPos < chromSize);
    q = qSym[i];
    t = tSym[i];
    if (q != '-' && t != '-')
        {
	bitSetOne(aliBits, tPos);
	if (ntChars[q] == ntChars[t])
	    bitSetOne(matchBits, tPos);
	}
    if (t != '-')
        ++tPos;
    }
}

void setAliBits(char *axtBestDir, char *chrom, int chromSize,
	Bits *aliBits, Bits *matchBits)
/* Set bits where there are alignments and matches. */
{
char axtFileName[512];
struct axt *axt;
struct lineFile *lf;

sprintf(axtFileName, "%s/%s.axt", axtBestDir, chrom);
if ((lf = lineFileMayOpen(axtFileName, TRUE)) == NULL)
    {
    warn("Couldn't open %s", axtFileName);
    return;
    }
while ((axt = axtRead(lf)) != NULL)
    {
    axtSetBits(axt, chromSize, aliBits, matchBits);
    axtFree(&axt);
    }
lineFileClose(&lf);
}

void statsOnSpan(struct sqlConnection *conn, struct region *r,
	char *axtBestDir, struct stats *stats, FILE *f)
/* Gather region info on one chromosome/region. */
{
char *chrom = r->chrom;
int chromSize = hChromSize(chrom);
Bits *maskBits = bitAlloc(chromSize);
Bits *aliBits = bitAlloc(chromSize);
Bits *matchBits = bitAlloc(chromSize);
Bits *geneBits = bitAlloc(chromSize);

/* Set up aliBits and matchBits for to be turned on
 * where bases align, and where bases align and match.
 * Zero both bitmaps in areas that are transcribed. */
setAliBits(axtBestDir, chrom, chromSize, aliBits, matchBits);
maskFeatures(conn, chrom, chromSize, maskBits);
bitNot(maskBits, chromSize);
bitAnd(aliBits, maskBits, chromSize);
bitAnd(matchBits, maskBits, chromSize);

/* Set up maskBits to have 0's on gaps in genome */
bitClear(maskBits, chromSize);
fbOrTableBits(maskBits, "gap", chrom, chromSize, conn);
bitNot(maskBits, chromSize);

/* Set up bitmap for Ensemble or mRNA. */
fbOrTableBits(geneBits, "ensGene", chrom, chromSize, conn);
fbOrTableBits(geneBits, "mrna", chrom, chromSize, conn);

/* Calculate various stats on windows. */
addToStats(stats, aliBits, matchBits, geneBits, maskBits, r, f);

/* Cleanup */
bitFree(&geneBits);
bitFree(&maskBits);
bitFree(&aliBits);
bitFree(&matchBits);
}

void reportStats(struct stats *stats, FILE *f)
/* Report statistics */
{
int i, j;
int total = stats->totalConsCount;

fprintf(f, "Noncoding conservation:\n");
for (i=0; i<histSize; ++i)
    {
    fprintf(f, "%4.1f%%: %3d ( %5.3f%%)\n", 
    	100.0 * i/histSize, 
	stats->consCounts[i], 100.0 * stats->consCounts[i]/total);
    }
fprintf(f, "\n");
fprintf(f, "Gene density:\n");
total = stats->totalGeneCount;
for (i=0; i<histSize; ++i)
    {
    fprintf(f, "%5.1f%%: %3d ( %5.3f%%)\n", 
    	(double)100.0 * i/histSize/geneScale, 
	stats->geneCounts[i], 100.0 * stats->geneCounts[i]/total);
    }
fprintf(f, "\n");
fprintf(f, "both density:\n");
total = stats->totalBothCount;
for (i=0; i<histSize; ++i)
    {
    fprintf(f, "%4.1f%%: x", (double)i/geneScale);
    for (j=0; j<histSize; ++j)
	fprintf(f, "%f ", (double)stats->bothCounts[j][i] / total);
    fprintf(f, "\n");
    }
}

void regionPicker(char *database, char *axtBestDir, char *output)
/* regionPicker - Code to pick regions to annotate deeply.. */
{
struct sqlConnection *conn = NULL;
struct slName *allChroms = NULL, *chrom = NULL;
struct region *regionList = NULL, *region;
FILE *f = mustOpen(output, "w");
struct stats *stats;

AllocVar(stats);
hSetDb(database);

/* Figure out which regions to process from command line.
 * By default will do whole genome. */
clRegion = optionVal("region", clRegion);
if (sameWord(clRegion, "genome"))
    {
    allChroms = hAllChromNames();
    for (chrom = allChroms; chrom != NULL; chrom = chrom->next)
        {
	if (!endsWith(chrom->name, "_random"))
	    {
	    AllocVar(region);
	    region->name = cloneString(chrom->name);
	    region->chrom = cloneString(chrom->name);
	    region->start = 0;
	    region->end = hChromSize(chrom->name);
	    slAddHead(&regionList, region);
	    }
	}
    }
else if (startsWith("chr", clRegion) && strchr(clRegion, ':') == NULL)
    {
    AllocVar(region);
    region->name = cloneString(clRegion);
    region->chrom = cloneString(clRegion);
    region->start = 0;
    region->end = hChromSize(clRegion);
    slAddHead(&regionList, region);
    }
else 
    {
    char *row[2];
    struct lineFile *lf = lineFileOpen(clRegion, TRUE);
    while (lineFileRow(lf, row))
	{
	AllocVar(region);
	region->name = cloneString(row[0]);
	if (!hgParseChromRange(row[1], &region->chrom, &region->start, &region->end))
	    errAbort("Bad range %s\n", row[1]);
	slAddHead(&regionList, region);
	}
    }
slReverse(&regionList);


/* Gather statistics one region at a time and then
 * print them. */
conn = hAllocConn();
for (region = regionList; region != NULL; region = region->next)
     {
     printf("Processing %s %s:%d-%d\n", region->name,
     	region->chrom, region->start, region->end);
     statsOnSpan(conn, region, axtBestDir, stats, f);
     }
fprintf(f, "\n");
reportStats(stats, f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
bigWinSize = optionInt("bigWinSize", bigWinSize);
bigStepSize = optionInt("bigStepSize", bigStepSize);
smallWinSize = optionInt("smallWinSize", smallWinSize);
threshold = optionFloat("threshold", threshold);
dnaUtilOpen();
if (argc != 4)
    usage();
regionPicker(argv[1], argv[2], argv[3]);
return 0;
}

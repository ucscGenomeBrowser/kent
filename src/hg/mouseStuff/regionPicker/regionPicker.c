/* regionPicker - Code to pick regions to annotate deeply.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "bits.h"
#include "dnautil.h"
#include "hdb.h"
#include "featureBits.h"
#include "axt.h"
#include "htmshell.h"

static char const rcsid[] = "$Id: regionPicker.c,v 1.11 2008/09/03 19:20:39 markd Exp $";

/* Command line overridable variables. */
char *clRegion = "genome";
int bigWinSize = 500000;
int bigStepSize = 100000;
int smallWinSize = 125;
double threshold = 0.8;
int picksPer = 5;
char *htmlOutput = NULL;
char *avoidFile = NULL;
int randSeed = 12345;
boolean printWin = FALSE;


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
  "   -html=output.html - where to write hyperlinks for region\n"
  "   -region=chrN - restrict to a single chromosome\n"
  "   -region=file - File has chrN:start-end on each line\n"
  "   -printWin - Print stats on each window\n"
  "   -avoid=file - File of regions to avoid\n"
  "   -randSeed=N - Seed for random number generator\n"
  "   -bigWinSize=N -default %d\n"
  "   -bigStepSize=N - default %d\n"
  "   -smallWinSize=N - default %d\n"
  "   -theshold=0.N - minimum base identity in small window. default %2.1f\n"
  "   -chromLimit=file - File that has limits for picks per chromosome\n"
  "   -picksPer=N - number of picks per strata, default %d\n",
       bigWinSize, bigStepSize, smallWinSize, threshold, picksPer
  );
}

struct region
/* Part of the genome. */
    {
    struct region *next;	/* Next item in list. */
    char *name;			/* Region name. */
    char *chrom;		/* Chromosome. */
    int start, end;		/* 0 based half open range */
    };

struct scoredWindow
/* A scored region. */
    {
    struct scoredWindow *next;
    char *chrom;	/* Not allocated here. */
    int start;		/* Start in genome. */
    double geneRatio;	/* Gene density. */
    double consRatio;   /* Conserved not transcribed density. */
    };

struct chromLimit
/* Structure to help us limit number of picks per 
 * chromosome. */
    {
    struct chromLimit *next;
    char *name;		/* Chromosome name - allocated in hash */
    int size;		/* Chromosome size. */
    int maxPicks;	/* Maximum number of picks. */
    int curPicks;	/* Current number of picks. */
    };

/* This is the ratio of genome on which we make cuts. */
#define strataCount 3
double genoCuts[strataCount] = {0.5, 0.8, 1.0};

#define histSize 1000
#define geneScale 5  /* Spread gene density from 0-20% instead of 0-100% */


struct region *loadRegionFile(char *database, char *fileName)
/* Load up list of regions from a file. */
{
struct region *regionList = NULL, *region;
char *row[2];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    AllocVar(region);
    region->name = cloneString(row[0]);
    if (!hgParseChromRange(database, row[1], &region->chrom, 
    		&region->start, &region->end))
	errAbort("Bad range %s\n", row[1]);
    slAddHead(&regionList, region);
    }
lineFileClose(&lf);
slReverse(&regionList);
return regionList;
}

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
	Bits *geneBits, Bits *seqBits, struct region *r, FILE *f,
	struct scoredWindow **pWinList)
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

if (printWin)
    fprintf(f, "%s\n", r->name);
for (bigStart = chromStart; bigStart < chromEnd;  bigStart += bigStepSize)
    {
    int smallPassing = 0;  /* Count of small windows passing %ID threshold */
    int smallGotData = 0;  /* Count of small windows with alignment data */
    int consIx = -1; /* Index into conservation histogram */
    int geneIx = -1; /* Index into gene density histogram */
    int thisBigSize;

    /* Figure out boundaries of big window,  and based on
     * size how much to weigh it in histogram */
    bigEnd = bigStart + bigWinSize;
    if (bigEnd > chromEnd) bigEnd = chromEnd;
    thisBigSize = bigEnd - bigStart;
    bigWeight = round(10.0 * thisBigSize / bigWinSize);


    /* Figure out number of non-N bases, and skip this window
     * if they amount to less than half of it. */
    seqCount = bitCountRange(seqBits, bigStart, thisBigSize);
    if (seqCount < thisBigSize/2)
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
	if (printWin)
	    {
	    fprintf(f, "    %s:%d-%d ", chrom, bigStart+1, bigEnd);
	    fprintf(f, "gene %4.1f%% consNotTx %4.1f%%\n", 
	    	100*geneRatio, 100*consRatio);
	    }
	stats->totalBothCount += bigWeight;
	stats->bothCounts[consIx][geneIx] += bigWeight;

	/* If no gaps add it to window list. */
	if (seqCount == bigWinSize)
	    {
	    struct scoredWindow *win;
	    AllocVar(win);
	    win->chrom = chrom;
	    win->start = bigStart;
	    win->geneRatio = geneRatio;
	    win->consRatio = consRatio;
	    slAddHead(pWinList, win);
	    }
	}
    }
}

void maskFeatures(char *database, struct sqlConnection *conn, char *chrom, int chromSize, Bits *maskBits)
/* Mask out bits we're not interested in for conservation. */
{
fbOrTableBits(database, maskBits, "gap", chrom, chromSize, conn);
fbOrTableBits(database, maskBits, "refGene:exon:12", chrom, chromSize, conn);
fbOrTableBits(database, maskBits, "mrna:exon:12", chrom, chromSize, conn);
fbOrTableBits(database, maskBits, "ensGene:exon:12", chrom, chromSize, conn);
fbOrTableBits(database, maskBits, "softberryGene:exon:12", chrom, chromSize, conn);
fbOrTableBits(database, maskBits, "twinscan:exon:12", chrom, chromSize, conn);
fbOrTableBits(database, maskBits, "xenoMrna:exon:12", chrom, chromSize, conn);
fbOrTableBits(database, maskBits, "intronEst:exon:12", chrom, chromSize, conn);
fbOrTableBits(database, maskBits, "anyMrnaCov", chrom, chromSize, conn);
fbOrTableBits(database, maskBits, "rmsk", chrom, chromSize, conn);
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
	if (ntChars[(int)q] == ntChars[(int)t])
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

void statsOnSpan(char *database, struct sqlConnection *conn, struct region *r,
	char *axtBestDir, struct stats *stats, FILE *f, 
	struct scoredWindow **pWinList)
/* Gather region info on one chromosome/region. */
{
char *chrom = r->chrom;
int chromSize = hChromSize(database, chrom);
Bits *maskBits = bitAlloc(chromSize);
Bits *aliBits = bitAlloc(chromSize);
Bits *matchBits = bitAlloc(chromSize);
Bits *geneBits = bitAlloc(chromSize);

/* Set up aliBits and matchBits for to be turned on
 * where bases align, and where bases align and match.
 * Zero both bitmaps in areas that are transcribed. */
setAliBits(axtBestDir, chrom, chromSize, aliBits, matchBits);
maskFeatures(database, conn, chrom, chromSize, maskBits);
bitNot(maskBits, chromSize);
bitAnd(aliBits, maskBits, chromSize);
bitAnd(matchBits, maskBits, chromSize);

/* Set up maskBits to have 0's on gaps in genome */
bitClear(maskBits, chromSize);
fbOrTableBits(database, maskBits, "gap", chrom, chromSize, conn);
bitNot(maskBits, chromSize);

/* Set up bitmap for Ensemble or mRNA. */
fbOrTableBits(database, geneBits, "ensGene", chrom, chromSize, conn);
fbOrTableBits(database, geneBits, "mrna", chrom, chromSize, conn);

/* Calculate various stats on windows. */
addToStats(stats, aliBits, matchBits, geneBits, maskBits, r, f, pWinList);

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

void calcCuts(int *hist, int sizeHist, int total, double scale,
	double *cuts, int cutCount)
/* Figure out how to go from percent of genome covered to
 * thresholds. */
{
int hIx = 0, cIx;
int acc = 0;

for (cIx = 0; cIx < cutCount;  ++cIx)
    {
    int threshold = round(genoCuts[cIx] * total);
    for (; hIx < sizeHist; ++hIx)
        {
	acc += hist[hIx];
	if (acc >= threshold)
	    break;
	}
    cuts[cIx] = hIx * scale / sizeHist;
    }
}

int cutIx(double *cuts, int cutCount, double val)
/* Return index of cut this falls into. */
{
int i, ix = cutCount-1;
for (i=0; i<cutCount; ++i)
    {
    if (val < cuts[i])
	{
	ix = i;
        break;
	}
    }
return ix;
}

boolean hitsRegions(char *chrom, int start, int end, struct region *regionList)
/* Return TRUE if position intersects any region on list. */
{
struct region *r;
for (r = regionList; r != NULL; r = r->next)
    {
    if (sameString(chrom, r->chrom) 
    	&& rangeIntersection(start, end, r->start, r->end) > 0)
	return TRUE;
    }
return FALSE;
}


struct chromCounts
/* Help count up things in a chromosome. */
    {
    struct chromCounts *next;
    char *name;	/* Chromosome name - allocated in hash. */
    int chromSize;	/* Size of chromosome */
    int count;		/* Count for this chromosome. */
    };

void countChromWindows(char *database, struct scoredWindow *winList, FILE *f)
/* Go through winList and count up how many hit each chromosome. */
{
struct chromCounts *ccList = NULL, *cc;
struct hash *hash = newHash(8);
struct scoredWindow *win;

for (win = winList; win != NULL; win = win->next)
    {
    char *chrom = win->chrom;
    cc = hashFindVal(hash, chrom);
    if (cc == NULL)
        {
	AllocVar(cc);
	slAddHead(&ccList, cc);
	hashAddSaveName(hash, chrom, cc, &cc->name);
	cc->chromSize = hChromSize(database, chrom);
	}
    cc->count += 1;
    }

fprintf(f, "Finished window count per chromosome:\n");
for (cc = ccList; cc != NULL; cc = cc->next)
    {
    fprintf(f, "%s\t%d\t%5.2f%%\n", cc->name, cc->count,
    	100.0 * cc->count * bigStepSize / cc->chromSize);
    }
fprintf(f, "\n");

slFreeList(&ccList);
hashFree(&hash);
}

boolean withinChromLimits(struct hash *chromLimitHash, char *chrom)
/* Return TRUE if this pick would not over-represent this
 * chromosome (and increment current pick count on that
 * chromosome). */
{
struct chromLimit *cl = hashFindVal(chromLimitHash, chrom);
if (cl->curPicks >= cl->maxPicks)
    return FALSE;
++cl->curPicks;
return TRUE;
}

void outputPicks(struct scoredWindow *winList,  char *database,
	struct hash *chromLimitHash, struct stats *stats, FILE *f)
/* Output picked regions. */
{
struct scoredWindow *strata[strataCount][strataCount];
double geneCuts[strataCount], consCuts[strataCount];
struct scoredWindow *win, *next;
int geneIx, consIx, pickIx;
FILE *html = NULL;
struct region *avoidList = NULL, *avoid;

/* Get list of regions to avoid */
if (avoidFile != NULL)
    avoidList = loadRegionFile(database, avoidFile);

if (htmlOutput != NULL)
    {
    html = mustOpen(htmlOutput, "w");
    htmStart(html, "Random Regions for 1% Project");
    }
fprintf(f, "Cuts at:\n");
calcCuts(stats->consCounts, histSize, 
	stats->totalConsCount, 1.0, consCuts, strataCount);
uglyf("cons %f %f %f\n", consCuts[0], consCuts[1], consCuts[2]);
fprintf(f, "cons %f %f %f\n", consCuts[0], consCuts[1], consCuts[2]);
calcCuts(stats->geneCounts, histSize, 
	stats->totalGeneCount, 1.0/geneScale, geneCuts, strataCount);
uglyf("gene %f %f %f\n", geneCuts[0], geneCuts[1], geneCuts[2]);
uglyf("gene %f %f %f\n", geneCuts[0], geneCuts[1], geneCuts[2]);
fprintf(f, "\n");


/* Move winList to strata. */
zeroBytes(strata, sizeof(strata));
for (win = winList; win != NULL; win = next)
    {
    /* Calculate appropriate strata and move. */
    next = win->next;
    consIx = cutIx(consCuts, strataCount, win->consRatio);
    geneIx = cutIx(geneCuts, strataCount, win->geneRatio);
    slAddHead(&strata[consIx][geneIx], win);
    }

/* Shuffle strata and output first picks in each. */
srand(randSeed);
for (consIx=strataCount-1; consIx>=0; --consIx)
    {
    for (geneIx=strataCount-1; geneIx>=0; --geneIx)
        {
	int cs=0, gs=0;
	if (geneIx>0)
	    gs = round(100*genoCuts[geneIx-1]);
	if (consIx>0)
	    cs = round(100*genoCuts[consIx-1]);
	fprintf(f, "consNonTx %d%%-%d%%, gene %d%%-%d%%\n", 
		cs, round(100*genoCuts[consIx]),
		gs, round(100*genoCuts[geneIx]));
	if (html)
	    {
	    fprintf(html, "<H2>consNonTx %d%% - %d%%, gene %d%% - %d%%</H3>\n", 
		cs, round(100*genoCuts[consIx]),
		gs, round(100*genoCuts[geneIx]));
	    }
	shuffleList(&strata[consIx][geneIx], 20);
	pickIx = 0;
	for (win = strata[consIx][geneIx]; win != NULL; win = win->next)
	    {
	    int end = win->start + bigWinSize;
	    if (!hitsRegions(win->chrom, win->start, end, avoidList))
		{
		if (withinChromLimits(chromLimitHash, win->chrom))
		    {
		    AllocVar(avoid);
		    avoid->chrom = cloneString(win->chrom);
		    avoid->start = win->start;
		    avoid->end = end;
		    slAddHead(&avoidList, avoid);
		    fprintf(f, "%s:%d-%d\t", win->chrom, win->start+1, end);
		    fprintf(f, "consNonTx %4.1f%%, gene %4.1f%%\n",
			    100*win->consRatio, 100*win->geneRatio);
		    if (html)
			{
			fprintf(html, "<A HREF=\"http://genome.ucsc.edu/cgi-bin/");
			fprintf(html, "hgTracks?db=%s&position=%s:%d-%d\">",
				database, win->chrom, win->start+1, end);
			fprintf(html, "%s:%d-%d</A>", win->chrom, win->start+1, end);
			fprintf(html, "\tconsNonTx %4.1f%%, gene %4.1f%%<BR>\n",
				100*win->consRatio, 100*win->geneRatio);
			}
		    if (++pickIx >= picksPer)
			break;
		    }
		}
	    }
	fprintf(f, "\n");
	}
    }
if (html)
    {
    htmEnd(html);
    carefulClose(&html);
    }
}


struct hash *getChromLimits(char *database)
/* Get hash full of chromosome limits. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(8);
struct chromLimit *clList = NULL, *cl;
double sum = 0;
char *limitFile = optionVal("chromLimit", NULL);

/* Read in chromosome info from database. */
sr = sqlGetResult(conn, "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(cl);
    hashAddSaveName(hash, row[0], cl, &cl->name);
    cl->size = atoi(row[1]);
    sum += cl->size;
    slAddHead(&clList, cl);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

/* Calculate max picks. */
for (cl = clList; cl != NULL; cl = cl->next)
    {
    cl->maxPicks = round(60.0*cl->size/sum);
    }

/* Override max picks based on chromLimits file if any. */
if (limitFile != NULL)
    {
    struct lineFile *lf = lineFileOpen(limitFile, TRUE);
    char *row[2];
    while (lineFileRow(lf, row))
        {
	cl = hashFindVal(hash, row[0]);
	cl->maxPicks = lineFileNeedNum(lf, row, 1);
	}
    lineFileClose(&lf);
    }

return hash;
}

void regionPicker(char *database, char *axtBestDir, char *output)
/* regionPicker - Code to pick regions to annotate deeply.. */
{
struct sqlConnection *conn = NULL;
struct slName *allChroms = NULL, *chrom = NULL;
struct region *regionList = NULL, *region;
FILE *f = mustOpen(output, "w");
struct stats *stats;
struct scoredWindow *winList = NULL;
struct hash *chromLimitHash = NULL;

AllocVar(stats);
chromLimitHash = getChromLimits(database);

/* Figure out which regions to process from command line.
 * By default will do whole genome. */
if (sameWord(clRegion, "genome"))
    {
    allChroms = hAllChromNames(database);
    for (chrom = allChroms; chrom != NULL; chrom = chrom->next)
        {
	if (!endsWith(chrom->name, "_random"))
	    {
	    AllocVar(region);
	    region->name = cloneString(chrom->name);
	    region->chrom = cloneString(chrom->name);
	    region->start = 0;
	    region->end = hChromSize(database, chrom->name);
	    slAddHead(&regionList, region);
	    }
	}
    slReverse(&regionList);
    }
else if (startsWith("chr", clRegion) && strchr(clRegion, ':') == NULL)
    {
    AllocVar(region);
    region->name = cloneString(clRegion);
    region->chrom = cloneString(clRegion);
    region->start = 0;
    region->end = hChromSize(database, clRegion);
    slAddHead(&regionList, region);
    }
else 
    {
    regionList = loadRegionFile(database, clRegion);
    }


/* Gather statistics one region at a time and then
 * print them. */
conn = hAllocConn(database);
for (region = regionList; region != NULL; region = region->next)
     {
     printf("Processing %s %s:%d-%d\n", region->name,
     	region->chrom, region->start, region->end);
     statsOnSpan(database, conn, region, axtBestDir, stats, f, &winList);
     }
fprintf(f, "\n");
reportStats(stats, f);
fprintf(f, "\n");

uglyf("Got %d windows with no gaps\n", slCount(winList));
countChromWindows(database, winList, f);
outputPicks(winList, database, chromLimitHash, stats, f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
clRegion = optionVal("region", clRegion);
bigWinSize = optionInt("bigWinSize", bigWinSize);
bigStepSize = optionInt("bigStepSize", bigStepSize);
smallWinSize = optionInt("smallWinSize", smallWinSize);
threshold = optionFloat("threshold", threshold);
picksPer = optionInt("picksPer", picksPer);
htmlOutput = optionVal("html", htmlOutput);
avoidFile = optionVal("avoid", avoidFile);
randSeed = optionInt("randSeed", randSeed);
printWin = optionExists("printWin");
dnaUtilOpen();
if (argc != 4)
    usage();
regionPicker(argv[1], argv[2], argv[3]);
return 0;
}

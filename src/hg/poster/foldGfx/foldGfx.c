/* foldGfx - make graphics for Science foldout for draft genome issue. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "portable.h"
#include "memgfx.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "hCommon.h"
#include "hdb.h"
#include "nib.h"


char *nibDir = "/projects/cc/hg/gs.3/oo.15/nib";
int totalPageDots = 15000;
int totalPageBases = 300000000;
double baseToDotScale = 15000.0/300000000.0;
double dotToBaseScale = 300000000.0/15000.0;

int squiggleHeight = 75;
int grayBandHeight = 50;

int baseToDot(int basePos)
/* Returns dot-position corresponding to base position. */
{
return round(baseToDotScale*basePos);
}

int dotToBase(int dotPos)
/* Returns base-position corresponding to dot. */
{
return round(dotToBaseScale * dotPos);
}

struct memGfx *getScaledMg(int bases, int height)
/* Allocate a memGfx big enough for bases. */
{
int width = baseToDot(bases);
struct memGfx *mg;
uglyf("Allocatting memory graphic of %d x %d (for %d bases)\n", width, height, bases);
mg = mgNew(width, height);
mgClearPixels(mg);
return mg;
}


double gcPercentMin = 0.310;
double gcPercentMax = 0.610;
double gcPercentRange = 1.0/0.300;

int gcScaleRange(double gcPercent, int range)
/* Return gcPercent scaled to range. */
{
int ret = round(range * (gcPercent-gcPercentMin)*gcPercentRange);
ret = range-1 - ret;
if (ret < 0) ret = 0;
if (ret >= range) ret = range-1;
return ret;
}

int realDnaCount(DNA *dna, int size)
/* Return count of non-N bases. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    {
    if (dna[i] != 'n')
        ++count;
    }
return count;
}

int gcDnaCount(DNA *dna, int size)
/* Return count of C or G bases. */
{
int count = 0;
int i;
int base;

for (i=0; i<size; ++i)
    {
    base = dna[i];
    if (base == 'c' || base == 'g')
        ++count;
    }
return count;
}

void gcSquiggle(char *chromName, char *destDir, char *type, bool thick, bool line)
/* Make gcSquiggle  pic for chromosome. */
{
char gifName[512];
int chromSize = hChromSize(chromName);
struct memGfx *mg = getScaledMg(chromSize, squiggleHeight);
int dotWidth = mg->width;
char nibName[512];
FILE *nibFile;
int nibChromSize;
struct dnaSeq *seq = NULL;
double lastGcPercent = (gcPercentMin+gcPercentMax)/2;
double gcPercent;
int startBase = 0, endBase = 0, baseWidth;
int lastDot = -1, dot;
int realBaseCount;
int gcBaseCount;
bool lastMissing = TRUE;
int squigHeight = squiggleHeight-thick;
int y1,y2;

sprintf(gifName, "%s/%sgc%s.gif", destDir, chromName, type);
sprintf(nibName, "%s/%s.nib", nibDir, chromName);
nibOpenVerify(nibName, &nibFile, &nibChromSize);
if (nibChromSize != chromSize)
    errAbort("Disagreement on size of %s between database and %s\n",
    	chromName, nibName);

for (dot = 0; dot <dotWidth; ++dot)
    {
    startBase = endBase;
    endBase = dotToBase(dot+1);
    if (endBase > nibChromSize)
       endBase = nibChromSize;
    baseWidth = endBase-startBase;
    seq = nibLdPart(nibName, nibFile, nibChromSize, startBase, baseWidth);
    realBaseCount = realDnaCount(seq->dna, seq->size);
    gcBaseCount = gcDnaCount(seq->dna, seq->size);
    if (realBaseCount < 20)
        {
	/* Add psuedocounts from last time if sample is small. */
	lastMissing = TRUE;
	}
    else
        {
	gcPercent = (double)gcBaseCount/(double)realBaseCount;
	y2 = gcScaleRange(gcPercent, squigHeight);
	if (line && !lastMissing)
	    {
	    y1 = gcScaleRange(lastGcPercent, squigHeight);
	    mgDrawLine(mg, dot-1, y1, dot, y2, MG_BLACK);
	    if (thick)
	        {
		mgDrawLine(mg, dot-1, y1+1, dot, y2+1, MG_BLACK);
		}
	    }
	else
	    {
	    mgPutDot(mg, dot, y2, MG_BLACK);
	    if (thick)
	        mgPutDot(mg, dot, y2+1, MG_BLACK);
	    }
	lastGcPercent = gcPercent;
	lastMissing = FALSE;
	}
    freeDnaSeq(&seq);
    }
fclose(nibFile);
mgSaveGif(mg, gifName);
mgFree(&mg);
}

void foldGfx(char *destDir)
/* foldGfx - make graphics for Science foldout for draft genome issue. */
{
char *testChrom = "chrY";

makeDir(destDir);
gcSquiggle(testChrom, destDir, "Plot", FALSE, TRUE);
gcSquiggle(testChrom, destDir, "Dot", FALSE, FALSE);
gcSquiggle(testChrom, destDir, "Plot2", TRUE, TRUE);
gcSquiggle(testChrom, destDir, "Dot2", TRUE, FALSE);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "foldGfx - make graphics for Science foldout for draft genome issue\n"
  "usage:\n"
  "   foldGfx destDir\n");
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
dnaUtilOpen();
foldGfx(argv[1]);
return 0;
}

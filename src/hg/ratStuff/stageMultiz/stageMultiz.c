/* stageMultiz - Stage input directory for Webb's multiple aligner. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "axt.h"
#include "maf.h"
#include "portable.h"

int winSize = 1010000;
int overlapSize = 10000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "stageMultiz - Stage input directory for Webb's multiple aligner\n"
  "usage:\n"
  "   stageMultiz humanChr.axt mouseRatAxtDir outputDir\n"
  "This will create outputDir, and a subdirectory of outputDir\n"
  "for each window of the human.   The mouseRatAxtDir needs to\n"
  "have a chrN.axt.ix for each chrN.axt.  Use axtIndex to generate\n"
  "these.\n"
  "options:\n"
  "   -winSize=N Human bases in each directory.  Default %d\n"
  "   -overlapSize=N Amount to overlap each window. Default %d\n",
  winSize, overlapSize
  );
}

int maxChromSize = 500*1024*1024;	/* Largest size a binRange can handle */

struct binKeeper *loadAxtsIntoRange(char *fileName)
/* Read in an axt file and shove it into a bin-keeper. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct binKeeper *bk = binKeeperNew(0, maxChromSize);
struct axt *axt;

while ((axt = axtRead(lf)) != NULL)
    binKeeperAdd(bk, axt->tStart, axt->tEnd, axt);
lineFileClose(&lf);
return bk;
}

void outputSlice(FILE *f, struct axt *axt, int tStart, int tEnd)
/* Output a portion of to file as maf. */
{
if (tStart < axt->tStart)
    tStart = axt->tStart;
if (tEnd > axt->tEnd)
    tEnd = axt->tEnd;
axtSubsetOnT(axt, tStart, tEnd, axtScoreSchemeDefault(), f);
}

void stageMultiz(char *humanAxtFile, char *ratMouseDir, char *outputDir)
/* stageMultiz - Stage input directory for Webb's multiple aligner. */
{
struct binKeeper *humanBk = loadAxtsIntoRange(humanAxtFile);
struct hash *mouseHash = newHash(0);
int hStart;
char humanChromName[256];

makeDir(outputDir);
splitPath(humanAxtFile, NULL, humanChromName, NULL);

for (hStart = 0; hStart<maxChromSize - winSize; hStart += winSize - overlapSize)
    {
    int hEnd = hStart + winSize;
    struct binElement *humanList = binKeeperFindSorted(humanBk, hStart, hEnd);
    struct binElement *humanEl;
    char dirName[512], hmName[512], mrName[512];
    FILE *f;

    if (humanList != NULL)
	{
	/* Make output directory and create output file names. */
	sprintf(dirName, "%s/%s.%d", outputDir, humanChromName, hStart);
	uglyf("Making %s\n", dirName);
	makeDir(dirName);
	sprintf(hmName, "%s/hm.axt", dirName);
	sprintf(mrName, "%s/mr.axt", dirName);

	/* Write human/mouse file. */
	f = mustOpen(hmName, "w");
	for (humanEl = humanList; humanEl != NULL; humanEl = humanEl->next)
	    {
	    struct axt *axt = humanEl->val;
	    outputSlice(f, axt, hStart, hEnd);
	    }
	carefulClose(&f);
	slFreeList(&humanList);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 4)
    usage();
winSize = optionInt("winSize", winSize);
overlapSize = optionInt("overlapSize", overlapSize);
stageMultiz(argv[1], argv[2], argv[3]);
return 0;
}

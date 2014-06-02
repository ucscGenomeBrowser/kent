/* fiveCToBed - Convert 5C data (matrix) to bed. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hdb.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "fiveCToBed - Convert 5C data (matrix) to bed\n"
  "usage:\n"
  "   fiveCToBed in.txt outRoot\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct bed *getVert(char **choppedArray, int wordCount)
/* convert first line into array of beds. */
{
int i;
struct bed *list = NULL;
for (i = 0; i < wordCount; i++)
    {
    struct bed *newBed;
    int s, e;
    char *chrom;
    AllocVar(newBed);
    if (hgParseChromRange(NULL, choppedArray[i], &chrom, &s, &e))
	{
	newBed->chrom = cloneString(chrom);
	newBed->chromStart = (unsigned)s;
	newBed->chromEnd = (unsigned)e;	
	slAddHead(&list, newBed);
	}
    else
	freeMem(newBed);
    }
slReverse(&list);
return list;
}

void swapChromThick(struct bed *someBed)
/* swap chromStart/chromEnd with thickStart/thickEnd */
{
unsigned swap = someBed->chromStart;
someBed->chromStart = someBed->thickStart;
someBed->thickStart = swap;
swap = someBed->chromEnd;
someBed->chromEnd = someBed->thickEnd;
someBed->thickEnd = swap;
}

void fiveCToBed(char *infile, char *outRoot)
/* fiveCToBed - Convert 5C data (matrix) to bed. */
/* very primitive coding. pathetic almost */
{
struct lineFile *lf = lineFileOpen(infile, TRUE);
char *words[4096];
int count = lineFileChopTab(lf, words);
/* deal with line 1... it'll be pretty long */
struct bed *vertList = getVert(words, count);
struct bed *vert;
char tssLociOutName[128];
char dhsLociOutName[128];
char tssInterOutName[128];
char dhsInterOutName[128];
FILE *tssLociOut;
FILE *dhsLociOut;
FILE *tssInterOut;
FILE *dhsInterOut;
safef(tssLociOutName, sizeof(tssLociOutName), "%sTssLoci.bed", outRoot);
safef(dhsLociOutName, sizeof(dhsLociOutName), "%sDhsLoci.bed", outRoot);
safef(tssInterOutName, sizeof(tssInterOutName), "%sTssInter.bed", outRoot);
safef(dhsInterOutName, sizeof(dhsInterOutName), "%sDhsInter.bed", outRoot);
tssLociOut = mustOpen(tssLociOutName, "w");
dhsLociOut = mustOpen(dhsLociOutName, "w");
tssInterOut = mustOpen(tssInterOutName, "w");
dhsInterOut = mustOpen(dhsInterOutName, "w");
/* the first line has enough info for one of the four beds outputted. */
for (vert = vertList; vert != NULL; vert = vert->next)
    bedTabOutN(vert, 3, dhsLociOut);
carefulClose(&dhsLociOut);
/* deal with remaining lines */
while (lineFileNextRowTab(lf, words, count))
    {
    struct bed rowBed;
    char *chrom;
    int s, e;
    char *range = cloneString(words[0]);
    if (hgParseChromRange(NULL, range, &chrom, &s, &e))
	{
	int i;
	rowBed.chrom = cloneString(chrom);
	rowBed.chromStart = (unsigned)s;
	rowBed.chromEnd = (unsigned)e;
	bedTabOutN(&rowBed, 3, tssLociOut);
	rowBed.name = cloneString(".");
	rowBed.score = 1000;
	rowBed.strand[0] = '+';
	rowBed.strand[1] = '\0';
	for (i = 1, vert = vertList; (vert != NULL) && (i < count); vert = vert->next, i++)
	    {
	    unsigned val = sqlUnsigned(words[i]);
	    if (val > 0)
		{
		rowBed.thickStart = vert->chromStart;
		rowBed.thickEnd = vert->chromEnd;
		rowBed.itemRgb = val;
		bedTabOutN(&rowBed, 9, tssInterOut);
		swapChromThick(&rowBed);
		bedTabOutN(&rowBed, 9, dhsInterOut);
		swapChromThick(&rowBed);
		}
	    }
	}
    freez(&range);
    }
carefulClose(&tssLociOut);
carefulClose(&tssInterOut);
carefulClose(&dhsInterOut);
lineFileClose(&lf);
bedFreeList(&vertList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
fiveCToBed(argv[1], argv[2]);
return 0;
}

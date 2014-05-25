/* gpStats - Figure out some stats on the golden path.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "gpStats - Figure out some stats on the golden path.\n"
  "usage:\n"
  "   gpStats windowSize maxGap file(s).agp\n");
}

unsigned long totalBases=0;
unsigned long totalRaftStarts = 0;
unsigned long totalFlotStarts = 0;

struct sizeList
/* A list of sizes. */
   {
   struct sizeList *next;	/* Next in list. */
   int size;
   };

struct sizeList *raftList = NULL, *flotillaList = NULL;

int cmpSizeList(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct sizeList *a = *((struct sizeList **)va);
const struct sizeList *b = *((struct sizeList **)vb);
return (a->size - b->size);
}

int findWeightedMedian(struct sizeList *slList)
/* Find wieghted median of sizeList - where
 * half of bases are contained in items of size or
 * more.  Assumes list is sorted. */
{
unsigned long total = 0;
unsigned long halfTotal;
unsigned long acc = 0;
struct sizeList *sl;

if (slList == NULL)
    return 0;
for (sl = slList; sl != NULL; sl = sl->next)
    total += sl->size;
halfTotal = total/2;
for (sl = slList; sl != NULL; sl = sl->next)
    {
    acc += sl->size;
    if (acc >= halfTotal)
        return sl->size;
    }
assert(FALSE);
return 0;
}

void outputFlotilla(int size, int winSize)
/* Note flotilla (linked rafts) of given size. */
{
int starts = size - winSize;
struct sizeList *sl;

AllocVar(sl);
sl->size = size;
slAddHead(&flotillaList, sl);
totalBases += size;
if (starts > 0)
    totalFlotStarts += starts;
}

void outputRaft(int size, int winSize)
/* Note raft of a given size. */
{
int starts = size - winSize;
struct sizeList *sl;

AllocVar(sl);
sl->size = size;
slAddHead(&raftList, sl);
if (starts > 0)
    totalRaftStarts += starts;
}

void oneStats(char *fileName, int winSize, int maxGap)
/* Accumulate stats on one file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *words[32];
boolean inRaft = FALSE; 
int raftSize = 0, flotillaSize = 0;
int gapSize;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    if (wordCount < 6)
        errAbort("Expecting at least 5 words line %d of %s\n", 
	     lf->lineIx, lf->fileName);
    if (words[4][0] == 'N' || words[4][0] == 'U')
        {
	if (inRaft)
	    {
	    gapSize = atoi(words[5]);
	    outputRaft(raftSize, winSize);
	    raftSize = 0;
	    if (wordCount < 8 || sameWord(words[7], "no") || gapSize > maxGap)
	        {
		outputFlotilla(flotillaSize, winSize);
		flotillaSize = 0;
		}
	    }
	inRaft = FALSE;
	}
    else
        {
	int start, end, size1;
	inRaft = TRUE;
	start = atoi(words[1]) - 1;
	end = atoi(words[2]);
	size1 = end - start;
	raftSize += size1;
	flotillaSize += size1;
	}
    }
if (inRaft)
    {
    outputFlotilla(flotillaSize, winSize);
    outputRaft(raftSize, winSize);
    }
}

void gpStats(int windowSize, int maxGap, int fileCount, char *files[])
/* gpStats - Figure out some stats on the golden path.. */
{
int fileIx;
char *fileName;


for (fileIx=0; fileIx < fileCount; ++fileIx)
    {
    fileName = files[fileIx];
    oneStats(fileName, windowSize, maxGap);
    }
printf("totalBases %lu\n", totalBases);
printf("%f percent ordered\n", (double)totalFlotStarts/(double)totalBases);
printf("%f precent contig\n", (double)totalRaftStarts/(double)totalBases);
slSort(&raftList, cmpSizeList);
slSort(&flotillaList, cmpSizeList);
printf("%d wieghted median raft size\n", 
    findWeightedMedian(raftList));
printf("%d wieghted median flotilla size\n", 
    findWeightedMedian(flotillaList));
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 4)
    usage();
if (!isdigit(argv[1][0]))
    usage();
if (!isdigit(argv[2][0]))
    usage();
gpStats(atoi(argv[1]), atoi(argv[2]), argc-3, argv+3);
return 0;
}

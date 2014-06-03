/* faGapSizes - report on gap size counts/statistics. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "fa.h"
#include "dnautil.h"
#include "options.h"


/* parameters */
int histWidth = 45;
char *niceSizeList = "10,50,100,500,1000,5000,10000,50000";

/* global stats state */
int gapCount = 0;
int totalN = 0;
int minSize = 0;
int maxSize = 0;
int *niceSizes = NULL;
int niceSizeCount = 0;
int *histCounts = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faGapSizes - report on gap size counts/statistics\n"
  "usage:\n"
  "   faGapSizes in.fa\n"
  "options:\n"
  "   -niceSizes=N,M,... - List of sizes to use as boundaries for histogram bins\n"
  );
}

void getNiceSizes(char *niceSizeList, int **retNiceSizes, int *retCount)
/* Parse out comma separated list. */
{
char *words[256];
int i, wordCount = chopCommas(niceSizeList, words);
int *niceSizes;
AllocArray(niceSizes, wordCount);
for (i=0; i<wordCount; ++i)
    niceSizes[i] = atoi(words[i]);
*retNiceSizes = niceSizes;
*retCount = wordCount;
}


void initStats(int niceSizeCount)
/* initialize stats and histogram */
{
gapCount = 0;
totalN = 0;
minSize = BIGNUM;
maxSize = 0;
AllocArray(histCounts, (2*niceSizeCount)+1);
}


void recordGap(int nCount)
/* Keep track of min/max/total and histogram. */
{
int i=0;

if (nCount < minSize)
    minSize = nCount;
if (nCount > maxSize)
    maxSize = nCount;
totalN += nCount;
gapCount++;

for (i=0;  i < niceSizeCount;  i++)
    {
    if (nCount < niceSizes[i])
	{
	histCounts[2*i]++;
	break;
	}
    else if (nCount == niceSizes[i])
	{
	histCounts[(2*i)+1]++;
	break;
	}
    }
if (nCount > niceSizes[niceSizeCount-1])
    histCounts[2*niceSizeCount]++;
}


void reportStats()
/* print out min/max/etc and a histogram of gap sizes. */
{
double scale=0.0;
int maxCount=0;
int i=0, j=0;

if (gapCount == 0)
    printf("\nNo N's/gaps found!\n\n");
else
    printf("\ngapCount=%d, totalN=%d, minGap=%d, maxGap=%d, avgGap=%.2f\n\n",
	   gapCount, totalN, minSize, maxSize, (double)(totalN / gapCount));

maxCount = 0;
for (i=0;  i < (2*niceSizeCount)+1;  i++)
    {
    if (histCounts[i] > maxCount)
	maxCount = histCounts[i];
    }
if (maxCount <= histWidth)
    scale = 1.0;
else
    scale = ((double)histWidth / (double)maxCount);

for (i=0;  i < niceSizeCount;  i++)
    {
    int bottom = (i == 0) ? 0 : niceSizes[i-1];
    printf("%6d < size < %-6d: %5d: ", bottom, niceSizes[i], histCounts[2*i]);
    for (j=0;  j < (int)(histCounts[2*i] * scale);  j++)
	{
	putchar('*');
	}
    printf("\n");
    printf("         size = %-6d: %5d: ", niceSizes[i], histCounts[(2*i)+1]);
    for (j=0;  j < (int)(histCounts[(2*i)+1] * scale);  j++)
	{
	putchar('*');
	}
    printf("\n");
    }
    printf("         size > %-6d: %5d: ", niceSizes[i-1], histCounts[2*i]);
    for (j=0;  j < (int)(histCounts[2*i] * scale);  j++)
	{
	putchar('*');
	}
    printf("\n");
}


void faGapSizes(char *fileName)
/* faGapSizes - report on gap size counts/statistics. */
{
struct dnaSeq *seqList = NULL;
struct dnaSeq *seq = NULL;
int nCount=0;
int i=0;

getNiceSizes(niceSizeList, &niceSizes, &niceSizeCount);

initStats(niceSizeCount);

seqList = faReadAllDna(fileName);
nCount = 0;
for (seq=seqList;  seq != NULL;  seq=seq->next)
    {
    for (i=0;  i < seq->size;  i++)
	{
	if (seq->dna[i] == 'n' || seq->dna[i] == 'N')
	    {
	    nCount++;
	    }
	else if (nCount > 0)
	    {
	    recordGap(nCount);
	    nCount = 0;
	    }
	}
    if (nCount > 0)
	{
	recordGap(nCount);
	nCount = 0;
	}
    }

reportStats();
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
niceSizeList = optionVal("niceSizes", cloneString(niceSizeList));

if (argc != 2)
    usage();
faGapSizes(argv[1]);
return 0;
}

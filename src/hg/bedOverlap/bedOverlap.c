/* Copyright (C) 2005 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "dnautil.h"
#include "bed.h"
#include "hdb.h"
#include "binRange.h"
//#include "cheapcgi.h"
#include "verbose.h"

boolean strictTab = FALSE;	/* Separate on tabs. */
boolean hasBin = FALSE;		/* Input bed file includes bin. */
boolean noBin = FALSE;		/* Suppress bin field. */
int verbosity = 1;
int call;                   /* depth of stack */
int outCall;                   /* calls to outList */
struct bedStub *outList;        /* global output list */
float overlapPercent = 0.02;    /* minimum overlap to be removed */

struct bedStub
/* A line in a bed file with chromosome, start, end position parsed out. */
    {
    struct bedStub *next;	/* Next in list. */
    char *chrom;                /* Chromosome . */
    unsigned chromStart;             /* Start position. */
    unsigned chromEnd;		/* End position. */
    char *name;	/* Name of item */
    int score;                  /* score */
    char strand[2];  /* + or -.  */
    unsigned thickStart; /* Start of where display should be thick (start codon for genes) */
    unsigned thickEnd;   /* End of where display should be thick (stop codon for genes) */
    unsigned itemRgb;    /* RGB 8 bits each */
    unsigned blockCount; /* Number of blocks. */
    int *blockSizes;     /* Comma separated list of block sizes.  */
    int *chromStarts;    /* Start positions inside chromosome.  Relative to chromStart*/
    char *line;                 /* Line. */
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedOverlap -  Remove Overlaps from bed files - choose highest scoring bed\n"
  "usage:\n"
  "   bedOverlap infile.bed output.bed\n"
  "options:\n"
  "   -hasBin         Input bed file starts with a bin field.\n"
  "   -noBin          Suppress bin field\n"
  "Note: infile.bed must have at least 12 columns and be sorted by chrom, chromStart"
  );
}

char **cloneRow(char **row, int size)
{
int i;
char **result;

AllocArray(result, size);
for (i = 0 ; i < size ; i++)
    {
    result[i] = cloneString(row[i]);
    }
return result;
}

int findBedSize(char *fileName)
/* Read first line of file and figure out how many words in it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line;
int wordCount;
lineFileNeedNext(lf, &line, NULL);
if (strictTab)
    wordCount = chopTabs(line, words);
else
    wordCount = chopLine(line, words);
if (wordCount == 0)
    errAbort("%s appears to be empty", fileName);
lineFileClose(&lf);
return wordCount;
}

void loadOneBed(char *fileName, int bedSize, struct bedStub **pList)
/* Load one bed file.  Make sure all lines have bedSize fields.
 * Put results in *pList. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line, *dupe;
int wordCount;
struct bedStub *bed;

verbose(1, "Reading %s\n", fileName);
while (lineFileNext(lf, &line, NULL))
    {
    if (hasBin)
	nextWord(&line);
    dupe = cloneString(line);
    if (strictTab)
	wordCount = chopTabs(line, words);
    else
	wordCount = chopLine(line, words);
    lineFileExpectWords(lf, bedSize, wordCount);
    bed = (struct bedStub *)bedLoad12(words);
    bed->line = dupe;
    if (bed->score > 0)
        slAddHead(pList, bed);
    else
        verbose(1, "Skipping record %s:%d-%d with score %d\n",bed->chrom,bed->chromStart,bed->chromEnd,bed->score);
    }
lineFileClose(&lf);
slReverse(pList);
}

int getNum(char *words[], int wordIx, int chromStart)
{
/* Make sure that words[wordIx] is an ascii integer, and return
 * binary representation of it. Conversion stops at first non-digit char. */
char *ascii = words[wordIx];
char c = ascii[0];
if (c != '-' && !isdigit(c))
    errAbort("Expecting number field %d chromStart %d , got %s", 
    	wordIx+1, chromStart, ascii);
return atoi(ascii);
}

void writeBedTab(char *fileName, struct bedStub *bedList, int bedSize)
/* Write out bed list to tab-separated file. */
{
struct bedStub *bed;
FILE *f = mustOpen(fileName, "w");
char *words[64];
int i, wordCount;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (!noBin)
        fprintf(f, "%u\t", hFindBin(bed->chromStart, bed->chromEnd));
    if (strictTab)
	wordCount = chopTabs(bed->line, words);
    else
	wordCount = chopLine(bed->line, words);
    for (i=0; i<wordCount; ++i)
        {
	fputs(words[i], f);
	if (i == wordCount-1)
	    fputc('\n', f);
	else
	    fputc('\t', f);
	}
    }
fclose(f);
}

int bedOverlapBlocks(struct bedStub *bed1, struct bedStub *bed2)
/* return number of bases overlapping both beds looking at blocks */
{
int count = 0 ;  /* count of overlapping bases */
int i, j;
if (differentString(bed1->chrom, bed2->chrom))
    return 0;
for (i = 0 ; i < bed1->blockCount ; i++)
    for (j = 0 ; j < bed2->blockCount; j++)
        {
        int start1 = bed1->chromStart + bed1->chromStarts[i];
        int start2 = bed2->chromStart + bed2->chromStarts[j];
        int end1 = start1 + bed1->blockSizes[i];
        int end2 = start2 + bed2->blockSizes[j];
        count += positiveRangeIntersection(start1, end1, start2, end2);
        }
return count;
}

void pareList(struct bedStub **bedList, struct bedStub *match)
/* remove elements from the list that overlap match */
{
struct bedStub *bed;

for (bed = *bedList; bed != NULL; bed = bed->next)
    {
    /*char *name = bed->chrom;
    int start = bed->chromStart;
    int end = bed->chromEnd;
    int score = bed->score;*/
    int overlap = bedOverlapBlocks(bed, match);
    if (overlap > 0)
        {
        verbose(4, "remove %s\n",bed->name);
        slRemoveEl(bedList, bed);
        }
    }
}

void removeOverlap(int bedSize , struct bedStub *bedList)
/* pick highest scoring record from each overlapping cluster 
 * then remove all overlapping records and call recusively 
 * return list of best scoring records in each cluster */
{
struct bedStub *bed, *bestMatch = NULL, *prevBed = NULL;
bool first = TRUE;
//char *prevChrom = cloneString("chr1");
int prevStart = 0, prevEnd = 0;
int bestScore = 0;

if (bedList == NULL)
    return;

verbose(4, "list now %d\n",slCount(bedList));
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    int start = bed->chromStart;
    int end = bed->chromEnd;
    int score = bed->score;

    if (first || start <= prevEnd ) 
        {
        if (first)
            {
            prevStart = start;
            prevEnd = end;
            first = FALSE;
            }
        if (score > bestScore)
            {
            bestMatch = bed;
            bestScore = score;
            }
        prevEnd = max(prevEnd, end);
        prevBed = bed;
        }
    else
        break;
    }
if (bestMatch != NULL)
    {
    slRemoveEl(&bedList, bestMatch);
    slAddHead(&outList, bestMatch);
    verbose(4, "add to outList %d count %d\n",slCount(outList), outCall++);
    if (bedList != NULL)
        {
        verbose(4, "before pare %d\n",slCount(bedList));
        pareList(&bedList, bestMatch);
        verbose(4, "after  pare %d\n",slCount(bedList));
        }
    }
removeOverlap(bedSize , bedList);
}

void bedOverlap(char *aFile, char *outFile)
/* load all beds and pick highest scoring record from each overlapping cluster */
{
struct bedStub *bedList = NULL;
int bedSize = findBedSize(aFile);

if (hasBin)
    bedSize--;
printf("Loading Predictions from %s of size %d\n",aFile, bedSize);
if (bedSize < 12)
    errAbort("%s must have a at least 12 cols\n", aFile);
loadOneBed(aFile, bedSize, &bedList);
verbose(1, "Loaded %d elements of size %d\n", slCount(bedList), bedSize);
removeOverlap(bedSize, bedList);
verbose(1, "Saving %d records to %s\n", slCount(outList), outFile);
if (outList != NULL)
    slReverse(&outList);
writeBedTab(outFile, outList, bedSize);
}
int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3) 
    usage();
verbosity = optionInt("verbose", verbosity);
verboseSetLogFile("stdout");
verboseSetLevel(verbosity);
hasBin = optionExists("hasBin");
overlapPercent = optionFloat("minOverlap", overlapPercent);
noBin = optionExists("noBin") || optionExists("nobin");
outList = NULL;
bedOverlap(argv[1], argv[2]);
return 0;
}

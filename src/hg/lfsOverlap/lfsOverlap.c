/* lfsOverlap - remove overlapping records from lfs file keeping highest scoring one in each case.  */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "localmem.h"
#include "lfs.h"
#include "hdb.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"hasBin", OPTION_BOOLEAN},
    {"minOverlap", OPTION_FLOAT},
    {"name", OPTION_BOOLEAN},
    {"notBlocks", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean strictTab = FALSE; /* Separate on tabs */
boolean hasBin = FALSE; /* Input bed file includes bin */
boolean noBin = TRUE; /* Suppress bin field */
boolean useName = FALSE; /*name of record must match bestMatch to be rejected */
boolean notBlocks = FALSE; /* use chromStart and chromEnd for overlap */
float minOverlapPercent = 0.02; /* minimum overlap to be removed */
int verbosity = 1;
int call;                   /* depth of stack */
int outCall;                   /* calls to outList */
struct lfsStub *outList;        /* global output list */

struct lfsStub
/* Contains lfs struct info with an extra variable for the complete line  */
/* from the lfs file. */
    {
    struct lfsStub *next;       /* Next in list. */
    short bin;                  /* bin number for browser speed */
    char *chrom;                /* Chromosome . */
    unsigned chromStart;        /* Start position. */
    unsigned chromEnd;          /* End position. */
    char *name; /* Name of item */
    unsigned score;                  /* score */
    char strand[2]; 		/* strand */
    char *pslTable; 		/* table of PSL records for lfs */
    unsigned lfCount; 		/* Number of linked features in series */
    unsigned *lfStarts;		/* Start positions for each linked feature */
    unsigned *lfSizes;		/* Sizes for each linked feature */
    char **lfNames; 		/* List of names for linked features */
    char *line;			/* Line */
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "lfsOverlap - remove overlapping records from lfs file and retain the best\n" 
  "scoring lfs record for each set of overlapping records.\n"
  "If scores are equal, the first record found is retained\n"
  "\n"
  "Usage:\n"
  "   lfsOverlap [options] inLfs outLfs\n"
  "Options:\n"
  "   -hasBin - file includes bin\n"
  "   -minOverlap=0.N - minimum overlap of blocks to reject a record\n"
  "   -name - name of record must match best match to be rejected\n"
  "   -notBlocks - calculate overlap based on chromStart and chromEnd\n"
  "Note: inLfs must have at 11 columns and be sorted by chrom, chromStart\n" 
  );
}

int findLfsSize(char *fileName)
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

void loadOneLfs(char *fileName, int lfsSize, struct lfsStub **pList)
/* Load one lfs file.  Make sure all lines have lfsSize fields.
 * Put results in *pList. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[64], *line, *dupe;
int wordCount;
struct lfsStub *lfs;

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
    lineFileExpectWords(lf, lfsSize, wordCount);
    lfs = (struct lfsStub *)lfsLoad(words);
    lfs->line = dupe;
    if (!hasBin)
       lfs->bin = 0;
    if (lfs->score > 0)
        {
        verbose(4, "adding to list: record %s:%d-%d\n", lfs->chrom,lfs->chromStart,lfs->chromEnd);
        slAddHead(pList, lfs);
        }
    else
        verbose(1, "Skipping record %s:%d-%d with score %d and name, %s\n",lfs->chrom,lfs->chromStart,lfs->chromEnd,lfs->score,lfs->name);
    }
lineFileClose(&lf);
slReverse(pList);
}

void writeLfsTab(char *fileName, struct lfsStub *lfsList, int lfsSize)
/* Write out lfs list to tab-separated file. */
{
struct lfsStub *lfs;
FILE *f = mustOpen(fileName, "w");
char *words[64];
int i, wordCount;

for (lfs = lfsList; lfs != NULL; lfs = lfs->next)
    {
    if (strictTab)
        wordCount = chopTabs(lfs->line, words);
    else
        wordCount = chopLine(lfs->line, words);
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

float lfsOverlaps(struct lfsStub *lfs1, struct lfsStub *lfs2)
/* Return overlapping bases from both lfs items looking at blocks as a */
/* percent of the total block size for the lfs with the largest total */
/* block size. */
/* If notBlocks option is selected look just at chromStart and chromEnd */
{
int count = 0 ;  /* count of overlapping bases */
int size = 0, size2 = 0; /* total size of blocks */
int i, j;
float overlapPercent;

if (differentString(lfs1->chrom, lfs2->chrom))
    return 0;
if (notBlocks)
    {
    count = positiveRangeIntersection(lfs1->chromStart, lfs1->chromEnd, lfs2->chromStart, lfs2->chromEnd);
    size = lfs1->chromEnd - lfs1->chromStart;
    size2 = lfs2->chromEnd - lfs2->chromStart;
    verbose(4, "start1:%d, end1: %d, start2: %d and end2: %d \n", lfs1->chromStart, lfs1->chromEnd, lfs2->chromStart, lfs2->chromEnd);
    }
else
    {
    for (i = 0 ; i < lfs1->lfCount ; i++)
        {
        size += lfs1->lfSizes[i];
        size2 += lfs2->lfSizes[i];
        for (j = 0 ; j < lfs2->lfCount; j++)
            {
            int start1 = lfs1->lfStarts[i];
            int start2 = lfs2->lfStarts[j];
            int end1 = start1 + lfs1->lfSizes[i];
            int end2 = start2 + lfs2->lfSizes[j];
            verbose(4, "start1: %d, start2: %d, end1: %d and end2: %d here\n", start1, start2, end1, end2);
            count += positiveRangeIntersection(start1, end1, start2, end2);
            }
        }
     }
if (size2 > size)
    size = size2;
overlapPercent = (float)count / (float)size;
verbose(4, "count is %d and lfs2 size: %d, overlap percent: %f\n", count, size, overlapPercent);
return overlapPercent;
}

void pareList(struct lfsStub **lfsList, struct lfsStub *match)
/* Remove elements from the list that overlap match */
{
struct lfsStub *lfs;
boolean removeRecord = TRUE; 
float overlapPercent;                                                                 
for (lfs = *lfsList; lfs != NULL; lfs = lfs->next)
    {
    removeRecord = TRUE;
    overlapPercent = lfsOverlaps(lfs, match);
    if ((useName) && (!sameString(lfs->name, match->name)))
        {
        removeRecord = FALSE;
        }
    if ((overlapPercent >= minOverlapPercent) && (removeRecord))
        {
        verbose(4, "remove %s\n",lfs->name);
        slRemoveEl(lfsList, lfs);
        }
    }
}

void removeOverlap(int lfsSize , struct lfsStub *lfsList)
/* pick highest scoring record from each overlapping cluster
 * then remove all overlapping records and call recursively
 * return list of best scoring records in each cluster */
{
struct lfsStub *lfs, *bestMatch = NULL;
// struct lfsStub *prevLfs = NULL;  unused
bool first = TRUE;
int prevEnd = 0;
// int prevStart = 0, prevEnd = 0;  unused
int bestScore = 0;
                                                                                
if (lfsList == NULL)
    return;
                                                                                
verbose(4, "list now %d\n",slCount(lfsList));
for (lfs = lfsList; lfs != NULL; lfs = lfs->next)
    {
    int start = lfs->chromStart;
    int end = lfs->chromEnd;
    int score = lfs->score;
    if (first || start <= prevEnd )
        {
        if (first)
            {
//            prevStart = start;  unused
            prevEnd = end;
            first = FALSE;
            }
        if (score > bestScore)
            {
            bestMatch = lfs;
            bestScore = score;
            }
        prevEnd = max(prevEnd, end);
//        prevLfs = lfs;  unused
        }
    else
        break;
    }
if (bestMatch != NULL)
    {
    slRemoveEl(&lfsList, bestMatch);
    slAddHead(&outList, bestMatch);
    verbose(4, "add to outList %d count %d\n",slCount(outList), outCall++);
    if (lfsList != NULL)
        {
        verbose(4, "before pare %d\n",slCount(lfsList));
        /* remove elements from the list that overlap the best match */
        pareList(&lfsList, bestMatch);
        verbose(4, "after  pare %d\n",slCount(lfsList));
       }
    }
removeOverlap(lfsSize, lfsList);
}

void lfsOverlap(char *aFile, char *outFile)
/* load all lfs and pick highest scoring record from each overlapping cluster */
{
struct lfsStub *lfsList = NULL;
int lfsSize = findLfsSize(aFile);

if (hasBin)
    lfsSize--;
printf("Loading records from %s of size %d\n",aFile, lfsSize);
if (lfsSize != 11)
    errAbort("%s must have 11 cols\n", aFile);
loadOneLfs(aFile, lfsSize, &lfsList);
verbose(1, "Loaded %d elements of size %d\n", slCount(lfsList), lfsSize);
removeOverlap(lfsSize, lfsList);
verbose(1, "Saving %d records to %s\n", slCount(outList), outFile);
if (outList != NULL)
    slReverse(&outList);
writeLfsTab(outFile, outList, lfsSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
verbosity = optionInt("verbose", verbosity);
//verboseSetLogFile("stdout");
verboseSetLevel(verbosity);
hasBin = optionExists("hasBin");
minOverlapPercent = optionFloat("minOverlap", minOverlapPercent);
useName = optionExists("name");
notBlocks = optionExists("notBlocks");
outList = NULL;
lfsOverlap(argv[1], argv[2]);
return 0;
}


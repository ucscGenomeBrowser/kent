#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "dnautil.h"
#include "bed.h"
#include "hdb.h"
#include "binRange.h"
#include "cheapcgi.h"

boolean strictTab = FALSE;	/* Separate on tabs. */
boolean hasBin = FALSE;		/* Input bed file includes bin. */
boolean noBin = FALSE;		/* Suppress bin field. */

struct bedStub
/* A line in a bed file with chromosome, start, end position parsed out. */
    {
    struct bedStub *next;	/* Next in list. */
    char *chrom;                /* Chromosome . */
    int chromStart;             /* Start position. */
    int chromEnd;		/* End position. */
    int score;                  /* score */
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
  "   -hasBin   Input bed file starts with a bin field.\n"
  "   -noBin   suppress bin field\n"
  "Note: infile.bed must be sorted by chrom, chromStart"
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
    AllocVar(bed);
    bed->chrom = cloneString(words[0]);
    bed->chromStart = lineFileNeedNum(lf, words, 1);
    bed->chromEnd = lineFileNeedNum(lf, words, 2);
    bed->score = lineFileNeedNum(lf, words, 4);
    bed->line = dupe;
    slAddHead(pList, bed);
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

void pareList(struct bedStub **bedList, struct bedStub *match)
/* remove elements from the list that overlap match */
{
struct bedStub *bed;

for (bed = *bedList; bed != NULL; bed = bed->next)
    {
    char *name = bed->chrom;
    int start = bed->chromStart;
    int end = bed->chromEnd;
    int score = bed->score;
    if (sameString(name, match->chrom) && rangeIntersection(start, end, match->chromStart, match->chromEnd) > 0)
        {
        slRemoveEl(bedList, bed);
        }
    }
if (*bedList != NULL)
    slReverse(*bedList);
}

void removeOverlap(int bedSize , struct bedStub *bedList, struct bedStub **outList)
/* pick highest scoring record from each overlapping cluster 
 * then remove all overlapping records and call recusively 
 * return list of best scoring records in each cluster */
{
struct bedStub *bed, *bestMatch = NULL;
int i;
bool first = TRUE;
char *prevChrom = cloneString("chr1");
int prevStart = 0, prevEnd = 0;
int bestScore = 0, bestCount = 0;

for (bed = bedList; bed != NULL; bed = bed->next)
    {
    char *name = bed->chrom;
    int start = bed->chromStart;
    int end = bed->chromEnd;
    int score = bed->score;

    if (first || (sameString(name,prevChrom) && start <= prevEnd))
        {
        if (first)
            {
            prevChrom = cloneString(name);
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
        }
    }
if (bestMatch != NULL)
    {
    slRemoveEl(&bedList, bestMatch);
    slAddHead(outList, bestMatch);
    pareList(&bedList, bestMatch);
    }
if (bedList != NULL)
    removeOverlap(bedSize , bedList, outList);
}

void bedOverlap(char *aFile, char *outFile)
/* load all beds and pick highest scoring record from each overlapping cluster */
{
struct bedStub *bedList = NULL, *bed, *outList = NULL;
int i, wordCount;
struct binElement *hitList = NULL, *hit;
struct binKeeper *bk;
int bedSize = findBedSize(aFile);

if (hasBin)
    bedSize--;
printf("Loading Predictions from %s of size %d\n",aFile, bedSize);
if (bedSize < 5)
    errAbort("%s must have a score\n", aFile);
loadOneBed(aFile, bedSize, &bedList);
verbose(1, "Loaded %d elements of size %d\n", slCount(bedList), bedSize);
removeOverlap(bedSize, bedList, &outList);
verbose(1, "Saving %d records to %s\n", slCount(outList), outFile);
if (outList != NULL)
    slReverse(&outList);
writeBedTab(outFile, outList, bedSize);
}
int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
hasBin = cgiBoolean("hasBin");
noBin = cgiBoolean("noBin") || cgiBoolean("nobin");
bedOverlap(argv[1], argv[2]);
return 0;
}

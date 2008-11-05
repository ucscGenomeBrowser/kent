/* splatMerge - Merge together splat files. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "splatAli.h"

static char const rcsid[] = "$Id: splatMerge.c,v 1.3 2008/11/05 08:01:27 kent Exp $";

boolean big = FALSE;
boolean dupeOk = FALSE;
static boolean worseToo = FALSE;
static int maxRepeat = 10;
static char *repeatOutput = NULL;
int minScore = 0;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "splatMerge - Merge together splat files.\n"
  "usage:\n"
  "   splatMerge in1.splat in2.splat ... inN.splat out.splat\n"
  "options:\n"
  "   -big - if big is set, then do merge from disk rather than in memory.\n"
  "          In this case the input files must be sorted on the read-name (sort -k 7)\n"
  "   -dupeOk - Allow the same read to align in the same place.  Normally this would\n"
  "             indicate an error in the input, but it's easy to do so it's checked\n"
  "   -worseToo - if set return alignments other than the best alignments\n"
  "   -maxRepeat=N - Maximum number of times for a read to be aligned.\n"
  "   -repeatOutput=file.fa - Output reads that align more than maxRepeat times here\n"
  "   -minScore=N - Minimum score (score is 2*match - 2*mismatch - 3*gap)\n"

  );
}

static FILE *repeatOutputFile = NULL;

static struct optionSpec options[] = {
   {"big", OPTION_BOOLEAN},
   {"dupeOk", OPTION_BOOLEAN},
   {"maxRepeat", OPTION_INT},
   {"repeatOutput", OPTION_STRING},
   {"worseToo", OPTION_BOOLEAN},
   {"minScore", OPTION_INT},
   {NULL, 0},
};

int splatAliCmpReadName(const void *va, const void *vb)
/* Compare two based on readName. Also separate secondarily on chrom position. */
{
const struct splatAli *a = *((struct splatAli **)va);
const struct splatAli *b = *((struct splatAli **)vb);
int diff = strcmp(a->readName, b->readName);
if (diff == 0)
    diff = a->chromStart - b->chromStart;
if (diff == 0)
    diff = a->chromEnd - b->chromEnd;
if (diff == 0)
    diff = a->strand - b->strand;
if (diff == 0)
    diff = strcmp(a->chrom, b->chrom);
return diff;
}


int splatAliScore(char *ali)
/* Score splat-encoded alignment. */
{
int score = 0;
char c;
while ((c = *ali++))
    {
    switch (c)
        {
	case 'a':
	case 'c':
	case 'g':
	case 't':
	    score -= 2;
	    break;
	case 'A':
	case 'C':
	case 'G':
	case 'T':
	    score += 2;
	    break;
	case 'n':
	case 'N':
	    break;
	case '^':
	    score -= 3;
	    ali += 1;
	    break;
	case '-':
	    score -= 3;
	    break;
	}
    }
return score;
}


void splatMergeBig(int inCount, char *inNames[], char *outName)
/* splatMergeBig - merge together previously sorted splat files using
 * a minimum of memory. */
{
errAbort("Not implemented.");
}

struct splatAli *findDifferentRead(struct splatAli *list)
/* Return first item in list different representing a different read
 * than the very first item. */
{
char *first =  list->readName;
struct splatAli *el;
for (el = list->next; el != NULL; el = el->next)
    {
    if (!sameString(first, el->readName))
         break;
    }
return el;
}

void findBestScoreInList(struct splatAli *start, struct splatAli *end, 
	int *retBestScore, int *retBestCount)
/* Scan through list and figure out best score and number of element in 
 * list with that score. */
{
int bestScore = 0, bestCount = 0;
struct splatAli *el;
for (el = start; el != end; el = el->next)
    {
    int score = splatAliScore(el->alignedBases);
    if (score >= bestScore)
        {
	if (score > bestScore)
	    {
	    bestScore = score;
	    bestCount = 1;
	    }
	else
	    bestCount += 1;
	}
    }
*retBestScore = bestScore;
*retBestCount = bestCount;
}

void checkDupes(struct splatAli *list)
/* Check there are no dupes in sorted list. */
{
if (list != NULL)
    {
    struct splatAli *el, *next;
    for (el = list; ; el = next)
	{
	next = el->next;
	if (next == NULL)
	    break;
	if (splatAliCmpReadName(&el, &next) == 0)
            {
	    errAbort("Duplicate alignment for %s, aborting.  Use -dupeOk to override.\n", 
	    	el->readName);
	    }
	}
    }
}

void outputBest(struct splatAli *start, struct splatAli *end, int bestScore, int bestCount,
	FILE *f)
/* Output the splat items between start and end that score at bestScore. */
{
struct splatAli *el;
if (bestScore >= minScore)
    {
    if (bestCount <= maxRepeat)
	{
	for (el = start; el != end; el = el->next)
	    {
	    int score = splatAliScore(el->alignedBases);
	    if (worseToo || score >= bestScore)
		{
		el->score = 1000/bestCount;
		splatAliTabOut(el, f);
		}
	    }
	}
    else
	{
	if (repeatOutputFile != NULL)
	    {
	    char *bases = cloneString(start->alignedBases);
	    stripChar(bases, '-');
	    stripChar(bases, '^');
	    fprintf(repeatOutputFile, ">%s\n%s\n", start->readName, bases);
	    freeMem(bases);
	    }
	}
    }
}

void splatMergeSmall(int inCount, char *inNames[], char *outName)
/* splatMerge - Merge together splat files in memory. */
{
/* Read in all files. */
struct splatAli *list = NULL, *el;
int i;
for (i=0; i<inCount; ++i)
    {
    struct lineFile *lf = lineFileOpen(inNames[i], TRUE);
    char *row[SPLATALI_NUM_COLS];
    while (lineFileRow(lf, row))
        {
	el = splatAliLoad(row);
	slAddHead(&list, el);
	}
    lineFileClose(&lf);
    }
uglyTime("loaded %d", slCount(list));

slSort(&list, splatAliCmpReadName);
uglyTime("sorted");
if (!dupeOk)
   checkDupes(list);

FILE *f = mustOpen(outName, "w");
struct splatAli *next;
for (el = list; el != NULL; el = next)
    {
    next = findDifferentRead(el);
    int bestScore, bestCount;
    findBestScoreInList(el, next, &bestScore, &bestCount);
    outputBest(el, next, bestScore, bestCount, f);
    }
carefulClose(&f);
}

void splatMerge(int inCount, char *inNames[], char *outName)
/* splatMerge - Merge together splat files.. */
{
if (repeatOutput != NULL)
    repeatOutputFile = mustOpen(repeatOutput, "w");
if (big)
    splatMergeBig(inCount, inNames, outName);
else
    splatMergeSmall(inCount, inNames, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 4)
    usage();
big = optionExists("big");
dupeOk = optionExists("dupeOk");
worseToo = optionExists("worseToo");
maxRepeat = optionInt("maxRepeat", maxRepeat);
repeatOutput = optionVal("repeatOutput", NULL);
minScore = optionInt("minScore", minScore);
splatMerge(argc-2, argv+1, argv[argc-1]);
return 0;
}

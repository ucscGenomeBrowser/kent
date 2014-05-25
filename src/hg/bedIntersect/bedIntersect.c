/* bedIntersect - Intersect two bed files. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"


static boolean aHitAny = FALSE;
static boolean bScore = FALSE;
static float minCoverage = 0.00001;
static boolean strictTab = FALSE;
static boolean allowStartEqualEnd = FALSE;

static struct optionSpec optionSpecs[] = {
    {"aHitAny", OPTION_BOOLEAN},
    {"bScore", OPTION_BOOLEAN},
    {"minCoverage", OPTION_FLOAT},
    {"tab", OPTION_BOOLEAN},
    {"allowStartEqualEnd", OPTION_BOOLEAN},
    {NULL, 0}
};


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedIntersect - Intersect two bed files\n"
  "usage:\n"
  "bed columns four(name) and five(score) are optional\n" 
  "   bedIntersect a.bed b.bed output.bed\n"
  "options:\n"
  "   -aHitAny        output all of a if any of it is hit by b\n"
  "   -minCoverage=0.N  min coverage of b to output match (or if -aHitAny, of a).\n"
  "                   Not applied to 0-length items.  Default %f\n"
  "   -bScore         output score from b.bed (must be at least 5 field bed)\n"
  "   -tab            chop input at tabs not spaces\n"
  "   -allowStartEqualEnd  Don't discard 0-length items of a or b\n"
  "                        (e.g. point insertions)\n",
   minCoverage
  );
}

struct bed5 
/* A five field bed. */
    {
    struct bed5 *next;
    char *chrom;	/* Allocated in hash. */
    int start;		/* Start (0 based) */
    int end;		/* End (non-inclusive) */
    char *name;	/* Name of item */
    int score; /* Score - 0-1000 */
    };

struct hash *readBed(char *fileName)
/* Read bed and return it as a hash keyed by chromName
 * with binKeeper values. */
{
char *row[5];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
int expectedCols = bScore ? 5 : 3;

while (lineFileNextRow(lf, row, expectedCols))
    {
    struct binKeeper *bk;
    struct bed5 *bed;
    struct hashEl *hel = hashLookup(hash, row[0]);
    if (hel == NULL)
       {
       bk = binKeeperNew(0, 1024*1024*1024);
       hel = hashAdd(hash, row[0], bk);
       }
    bk = hel->val;
    AllocVar(bed);
    bed->chrom = hel->name;
    bed->start = lineFileNeedNum(lf, row, 1);
    bed->end = lineFileNeedNum(lf, row, 2);
    if (bScore)
	bed->score = lineFileNeedNum(lf, row, 4);
    if (bed->start > bed->end)
        errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);
    if (bed->start == bed->end)
	{
	if (allowStartEqualEnd)
	    // Note we are tweaking binKeeper coords here, so use bed->start and bed->end.
	    binKeeperAdd(bk, max(0, bed->start-1), bed->end+1, bed);
	else
	    lineFileAbort(lf, "start==end (if this is legit, use -allowStartEqualEnd)");
	}
    else
	binKeeperAdd(bk, bed->start, bed->end, bed);
    }
lineFileClose(&lf);
return hash;
}

void outputBed(FILE *f, char **row, int wordCount, int aStart, int aEnd, struct bed5 *b)
/* Print a row of bed, possibly substituting in a different start, end and score. */
{
int s = aHitAny ? aStart : max(aStart, b->start);
int e = aHitAny ? aEnd : min(aEnd, b->end);
if (s == e && !allowStartEqualEnd)
    return;
int i;
fprintf(f, "%s\t%d\t%d", row[0], s, e);
for (i = 3 ; i<wordCount; i++)
    {
    if (bScore && i==4)
	fprintf(f, "\t%d", b->score);
    else
	fprintf(f, "\t%s", row[i]);
    }
fprintf(f, "\n");
}

float getCov(int aStart, int aEnd, struct bed5 *b)
/* Compute coverage of a's start and end vs. b's, taking aHitAny and
 * allowStartEqualEnd into account. */
{
float overlap = positiveRangeIntersection(aStart, aEnd, b->start, b->end);
float denominator = aHitAny ? (aEnd - aStart) : (b->end - b->start);
float cov = (denominator == 0) ? 0.0 : overlap/denominator;
if (allowStartEqualEnd && (aStart == aEnd || b->start == b->end))
    cov = 1.0;
return cov;
}

void bedIntersect(char *aFile, char *bFile, char *outFile)
/* bedIntersect - Intersect two bed files. */
{
struct lineFile *lf = lineFileOpen(aFile, TRUE);
struct hash *bHash = readBed(bFile);
FILE *f = mustOpen(outFile, "w");
char *row[40];
int wordCount;

while ((wordCount = (strictTab ? lineFileChopTab(lf, row) : lineFileChop(lf, row))) != 0)
    {
    char *chrom = row[0];
    int start = lineFileNeedNum(lf, row, 1);
    int end = lineFileNeedNum(lf, row, 2);
    if (start > end)
        errAbort("start after end line %d of %s", lf->lineIx, lf->fileName);
    if (start == end && !allowStartEqualEnd)
	lineFileAbort(lf, "start==end (if this is legit, use -allowStartEqualEnd)");
    struct binKeeper *bk = hashFindVal(bHash, chrom);
    if (bk != NULL)
	{
	struct binElement *hitList = NULL, *hit;
	if (allowStartEqualEnd && start == end)
	    hitList = binKeeperFind(bk, start-1, end+1);
	else
	    hitList = binKeeperFind(bk, start, end);
	if (aHitAny)
	    {
	    for (hit = hitList; hit != NULL; hit = hit->next)
		{
		float cov = getCov(start, end, hit->val);
		if (cov >= minCoverage)
		    {
		    outputBed(f, row, wordCount, start, end, hit->val);
		    break;
		    }
		else
		    {
		    struct bed5 *b = hit->val;
		    verbose(1, "filter out %s %d %d %d %d overlap %d %d %d %.3f\n",
			    chrom, start, end, b->start, b->end,
			    positiveRangeIntersection(start, end, b->start, b->end),
			    end-start, b->end-b->start, cov);
		    }
		}
	    }
	else
	    {
	    for (hit = hitList; hit != NULL; hit = hit->next)
	        {
		if (getCov(start, end, hit->val) >= minCoverage)
		    outputBed(f, row, wordCount, start, end, hit->val);
		}
	    }
	slFreeList(&hitList);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
aHitAny = optionExists("aHitAny");
bScore = optionExists("bScore");
minCoverage = optionFloat("minCoverage", minCoverage);
strictTab = optionExists("tab");
allowStartEqualEnd = optionExists("allowStartEqualEnd");
if(argc==4)
   bedIntersect(argv[1], argv[2], argv[3]);
if(argc!=4)
   usage();
return 0;
}

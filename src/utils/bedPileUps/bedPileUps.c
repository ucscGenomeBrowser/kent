/* bedPileUps - Find (exact) pile-ups (overlaps) if any in bed input */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedPileUps - Find (exact) overlaps if any in bed input\n"
  "usage:\n"
  "   bedPileUps in.bed\n"
  "Where in.bed is in one of the ascii bed formats.\n"
  "The in.bed file must be sorted by chromosome,start,\n"
  "  to sort a bed file, use the unix sort command:\n"
  "     sort -k1,1 -k2,2n unsorted.bed > sorted.bed\n"
  "\n"
  "Options:\n"
  "  -name - include BED name field 4 when evaluating uniqueness\n"
  "  -tab  - use tabs to parse fields\n"
  "  -verbose=2 - show the location and size of each pileUp\n"
  );
}

static struct optionSpec options[] = {
   {"name", OPTION_BOOLEAN},
   {"tab", OPTION_BOOLEAN},
   {NULL, 0},
};


boolean nameOpt = FALSE;
boolean tabSep = FALSE;

void bedPileUps(char *inName)
/* bedPileUps - Find (exact) overlaps if any in bed input (which must be sorted by chrom, start) */
{
char *line, *row[256];  // limit of 256 columns is arbitrary, but useful to catch pathological input
struct lineFile *lf = lineFileOpen(inName, TRUE);
boolean atEnd = FALSE, sameChrom = FALSE, newPile = FALSE;
char *chrom = NULL;
char *name = NULL;
char *lastName = cloneString("");
char *lastChrom = cloneString("");
int lineNumber = 0;
int fieldCount = 0;
int start = 0, end = 0, lastStart = -1, lastEnd = -1;
struct hash *chromHash = newHash(8);
int pileUpCount = 0;
int maxPileUpCount = 0;
char maxPileUpLocation[1024];
int numberOfPileUps = 0;
long long sumOfPileUpCounts = 0;



for (;;)
    {
    /* Get next line of input if any. */
    if (lineFileNextReal(lf, &line))
	{
	++lineNumber;

	/* Chop up line and make sure the word count is right. */
	int wordCount;
        if (tabSep)
	    wordCount = chopTabs(line, row);
	else
	    wordCount = chopLine(line, row);
	if (fieldCount == 0)
	    {
	    fieldCount = wordCount;
	    if (fieldCount < 3)
		errAbort("expecting at least 3 BED columns, found %d in line %d\n", wordCount, lineNumber);
	    if (fieldCount < 4)
		warn("BED 3 input does not support name column.\n");
	    }
	if (wordCount != fieldCount)
	    errAbort("Expecting at least %d columns in line %d.\n", fieldCount, lineNumber);

	chrom = row[0];
	lineFileAllInts(lf, row, 1, &start, TRUE, 4, "integer", TRUE);
	lineFileAllInts(lf, row, 2, &end  , TRUE, 4, "integer", TRUE);
	if (fieldCount >= 4)
	    {
    	    name = row[3];
	    verbose(3, "%s %d %d %s\n", chrom, start, end, name);
	    }
	else
	    {
	    verbose(3, "%s %d %d\n", chrom, start, end);
	    }

	sameChrom = sameString(chrom, lastChrom);
	}
    else  /* No next line */
	{
	atEnd = TRUE;
	}

    newPile = (!(start == lastStart && end == lastEnd && (!nameOpt || (nameOpt && sameString(name, lastName)))));

    /* Check conditions to finish current pile. */
    if (atEnd || !sameChrom || newPile)
	{
	// finish up any stats for the last pile we are leaving.
	if (pileUpCount > 1)
	    {
	    ++numberOfPileUps;
	    sumOfPileUpCounts += pileUpCount;
	    if (pileUpCount > maxPileUpCount)
		{
		maxPileUpCount = pileUpCount;
		safef(maxPileUpLocation, sizeof maxPileUpLocation, "%s %d %d", lastChrom, lastStart, lastEnd);
		}
	    if (fieldCount >=4)
		verbose(2, "pileUp of size %d at %s %d %d %s\n", pileUpCount, lastChrom, lastStart, lastEnd, lastName);
	    else
		verbose(2, "pileUp of size %d at %s %d %d\n", pileUpCount, lastChrom, lastStart, lastEnd);
	    
	    }

	if (atEnd)
	    break;
	}

    /* Advance to next chromosome. Make sure we have never seen it before. */
    if (!sameChrom)
        {
	if (hashLookup(chromHash, chrom))
	    errAbort("Input is not sorted - have seen this chromosome %s before in line %d.\n", chrom, lineNumber);
	hashAdd(chromHash, chrom, NULL);
	freeMem(lastChrom);
	lastChrom = cloneString(chrom);
	lastStart = -1;	
	lastEnd = -1;
	}

    /* Make sure that within a chrom, chromStart is non-decreasing */
    if (start < lastStart)
	errAbort("Input is not sorted - chromosome %s start %d < lastStart %d in line %d.\n", chrom, start, lastStart, lineNumber);

    if (start == lastStart && end == lastEnd  && (!nameOpt || (nameOpt && sameString(name, lastName))))
	{
	++pileUpCount;
	}
    else
	{
    	lastStart = start;
	lastEnd = end;
	freeMem(lastName);
	lastName = cloneString(name);
	pileUpCount = 1;
	}

    }

lineFileClose(&lf);

/* print totals */
if (numberOfPileUps > 0)
    {
    printf("Largest PileUp Size: %d is located at: %s\n", maxPileUpCount, maxPileUpLocation);
    printf("Average PileUp Size: %3.1f\n", (double)sumOfPileUpCounts / numberOfPileUps);
    }
printf("Total Number of PileUps found: %d\n", numberOfPileUps);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
tabSep = optionExists("tab");
nameOpt = optionExists("name");
if (argc != 2)
    usage();

bedPileUps(argv[1]);
optionFree();
return 0;
}

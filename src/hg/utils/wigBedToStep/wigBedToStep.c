/* wigBedToStep - Convert bed-style wiggle into variable step or fixed step wiggle.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigBedToStep - Convert bed-style wiggle into variable step or fixed step wiggle.\n"
  "usage:\n"
  "   wigBedToStep in.bed out.wiggle\n"
  "options:\n"
  "   -forceVarStep    force variable step output even if it can be fixed step.\n"
  "   -forseSpanOne    force output of each base even if increases the number of datapoints.\n"
  );
}

static struct optionSpec options[] = {
    {"forceVarStep", OPTION_BOOLEAN},
    {"forceSpanOne", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean validVariableStep(struct bed *bedList, int *retSpan)
/* Test whether all the spans are the same i.e. chromEnd-chromStart. */
{
struct bed *cur;
int minSpan = 1000000;
int maxSpan = 0;
if (bedList == NULL)
    return FALSE;
for (cur = bedList; cur != NULL; cur = cur->next)
    {
    int span = cur->chromEnd - cur->chromStart;
    if (span > maxSpan)
	maxSpan = span;
    if (span < minSpan)
	minSpan = span;
    }
if (minSpan != maxSpan)
    warn("Inconsistent spans across bed (min = %d, max = %d). Setting span to minimum.", minSpan, maxSpan);
*retSpan = minSpan;
return TRUE;
}

boolean validFixedStep(struct bed *bedList, int *retStep)
/* Test whether all the starts on each chrom are spaced at a fixed */
/* amount. */
{
struct bed *cur;
struct bed *prev;
int step = -1;
if (!bedList)
    return FALSE;
cur = prev = bedList;
while (cur != NULL)
    {
    if (prev == cur)
	{
	cur = cur->next;
	}
    else if (sameString(cur->chrom, prev->chrom))
	{
	if (step == -1)
	    step = cur->chromStart - prev->chromStart;
	else if ((cur->chromStart - prev->chromStart) != step)
	    return FALSE;	    
	prev = cur;
	cur = cur->next;
	}
    else 
	prev = cur;
    }
*retStep = step;
return TRUE;
}

void printDecLine(FILE *f, char *chrom, int chromStart, int span, int step, 
		  boolean doFixed)
/* print out a wiggle header line */
{
fprintf(f, "%s chrom=%s ", (doFixed) ? "fixedStep" : "variableStep", chrom);
if (doFixed)
    fprintf(f, "start=%d step=%d ", chromStart+1, step);
fprintf(f, "span=%d\n", span);
}

void printDataLine(FILE *f, int chromStart, char *data, boolean doFixed)
/* print out a wiggle data line */
{
if (!doFixed)
    fprintf(f, "%d\t", chromStart+1);
fprintf(f, "%s\n", data);
} 

void wiggleOut(FILE *f, struct bed *bedList, int span, int step)
/* the guts of the program.  go through the data line-by-line and output to a new file. */
{
struct bed *cur = bedList;
boolean doFixed = (step != -1);
boolean spanOne = FALSE;
int theSpan = span;
int theStep = step;
boolean printHeader = TRUE;
if (optionExists("forceSpanOne"))
    /* forcing span=1 changes the step and span to be one... obviously */
    {
    spanOne = TRUE;
    theSpan = 1;
    theStep = 1;
    doFixed = TRUE;
    }
if (optionExists("forceVarStep"))
    doFixed = FALSE;
for (; cur != NULL; cur = cur->next)
    {
    if (printHeader)
	printDecLine(f, cur->chrom, cur->chromStart, theSpan, theStep, doFixed);
    if (spanOne)
	{
	int i;
	for (i = cur->chromStart; i < cur->chromEnd; i++)
	    printDataLine(f, i, cur->name, doFixed);
	}
    else
	printDataLine(f, cur->chromStart, cur->name, doFixed);
    if (cur->next)
	/* check the various ways possible that the next line will need a header */
	{
	struct bed *next = cur->next;
	if (!sameString(cur->chrom, next->chrom))
	    printHeader = TRUE;
	else if (!spanOne && (cur->chromStart + theStep != next->chromStart))
	    printHeader = TRUE;
	else if (spanOne && (cur->chromEnd != next->chromStart))
	    printHeader = TRUE;
	else
	    printHeader = FALSE;
	}
    }
}

void wigBedToStep(char *inFile, char *outFile)
/* wigBedToStep - Convert bed-style wiggle into variable step or fixed step wiggle.. */
{
struct bed *bedList = bedLoadNAll(inFile, 4);
int span = 1;
int step = -1;
FILE *out = mustOpen(outFile, "w");
slSort(&bedList, bedCmp);
if (!optionExists("forceSpanOne"))
    {
    if (!validVariableStep(bedList, &span))
	errAbort("The inputted file is empty.");
    }
if (!optionExists("forceVarStep") && !optionExists("forceSpanOne"))
    validFixedStep(bedList, &step);
wiggleOut(out, bedList, span, step);
bedFreeList(&bedList);
carefulClose(&out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
wigBedToStep(argv[1], argv[2]);
return 0;
}

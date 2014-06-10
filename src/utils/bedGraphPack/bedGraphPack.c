/* bedGraphPack - Pack together adjacent records representing same value.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedGraphPack v1 - Pack together adjacent records representing same value.\n"
  "usage:\n"
  "   bedGraphPack in.bedGraph out.bedGraph\n"
  "The input needs to be sorted by chrom and this is checked.  To put in a pipe\n"
  "use stdin and stdout in the command line in place of file names.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void bedGraphPack(char *input, char *output)
/* bedGraphPack - Pack together adjacent records representing same value.. */
{
/* We'll keep a hash of chroms to help make sure we're sorted. */
struct hash *chromHash = hashNew(0);

/* Open input and output. */
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");

/* Loop is a little complex - it keeps track of previous value, and merges
 * current record into previous where possible.*/
char *prevChrom = "", *newChrom = "";
char prevStart[16] = "", prevEnd[16] = "", prevVal[32] = "";
boolean done = FALSE;
while (!done)
    {
    char *row[5];
    char *chrom = NULL, *start = NULL, *end = NULL, *val = NULL;
    char *newStart = prevStart;
    int rowSize = lineFileChopNext(lf, row, ArraySize(row));
    boolean outputLast = FALSE;
    if (rowSize == 0)
        {
	/* Cope with end of file. */
	outputLast = TRUE;
	done = TRUE;
	}
    else
	{
	/* Fetch next line into local string variables. */
	lineFileExpectWords(lf, 4, rowSize);
	chrom = row[0];
	start = row[1];
	end = row[2];
	val = row[3];

	/* We need to output if on a different chrom or skipping between previous record.
	 * This is also when we need to reset our starting point. */
	if (!sameString(prevChrom, chrom))
	    {
	    if (hashLookup(chromHash, chrom))
		errAbort("%s is hopping at line %d of %s", chrom, lf->lineIx, lf->fileName);
	    hashAddSaveName(chromHash, chrom, NULL, &newChrom);
	    newStart = start;
	    outputLast = TRUE;
	    }
	else
	    {
	    if (!sameString(start, prevEnd) || !sameString(val,prevVal))
	        {
		newStart = start;
		outputLast = TRUE;
		}
	    }

	}

    /* This is the one and only place in loop we output */
    if (outputLast && !isEmpty(prevChrom))
        {
	fprintf(f, "%s\t%s\t%s\t%s\n", prevChrom, prevStart, prevEnd, prevVal);
	outputLast = FALSE;
	}

    /* Copy current record to previous.  The additional done check here prevents
     * us from having to duplicate the output clause to handle EOF */
    if (!done)
	{
	prevChrom = newChrom;
	if (newStart != prevStart)
	    safef(prevStart, sizeof(prevStart), "%s", newStart);
	safef(prevEnd, sizeof(prevEnd), "%s", end);
	safef(prevVal, sizeof(prevVal), "%s", val);
	}
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bedGraphPack(argv[1], argv[2]);
return 0;
}

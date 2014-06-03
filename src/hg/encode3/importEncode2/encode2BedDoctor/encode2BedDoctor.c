/* encode2BedDoctor - Selectively fix up encode2 bed files.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "basicBed.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encode2BedDoctor - Selectively fix up encode2 bed files.\n"
  "usage:\n"
  "   encode2BedDoctor XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

boolean bedFixBlocks(struct bed *bed)
/* Make sure no blocks in bed list overlap. */
{
int i;
boolean anyFix = FALSE;
for (i=0; i<bed->blockCount-1; ++i)
    {
    int start = bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    int nextStart = bed->chromStarts[i+1];
    int overlap = end - nextStart;
    if (overlap > 0)
        {
	verbose(2, "Fixing block overlap of %d\n", overlap);
	anyFix = TRUE;
	int mid = (end + nextStart)/2;
	bed->blockSizes[i] = mid - start;
	int nextEnd = nextStart + bed->blockSizes[i+1];
	bed->chromStarts[i+1] = mid;
	bed->blockSizes[i+1] = nextEnd - mid;
	}
    }
return anyFix;
}

void encode2BedDoctor(char *inBed, char *outBed)
/* encode2BedDoctor - Selectively fix up encode2 bed files.. */
{
struct bed *bed;
int fieldCount = 0;
boolean itemRgb = FALSE;
char *row[256];
struct lineFile *lf = lineFileOpen(inBed, TRUE);
char *line;
int fixCount = 0;
FILE *f = mustOpen(outBed, "w");

int trackCount = 0;
boolean firstTime = TRUE;
while (lineFileNextReal(lf, &line))
    {
    /* Chop line into words, and check first word for a browser or track line which we ignore */
    int wordCount = chopLine(line, row);
    if (wordCount >= ArraySize(row))
        errAbort("Too many words (at least %d) in line %d of %s", 
	    wordCount, lf->lineIx, lf->fileName);	
    if (sameString("track", row[0]))
	 {
	 ++trackCount;
	 if (trackCount > 1)
	    errAbort("Multiple track lines in %s, not a single bed?", inBed);
         continue;   // is a custom track with non-bed stuff
	 }
    if (sameString("browser", row[0]))
         continue;   // is a custom track with non-bed stuff

    /* On first line set field count and whether it has an itemRgb column. */
    if (firstTime)
        {
	fieldCount = wordCount;
	if (fieldCount < 3)
	    errAbort("Bed %s does not have enough fields", lf->fileName);
	itemRgb =  (strchr(row[8], ',') != NULL);
	firstTime = FALSE;
	}
    else
        {
	if (wordCount != fieldCount)
	    errAbort("%d fields in first line, %d line %d of %s",  fieldCount, wordCount,
		lf->lineIx, lf->fileName);
	}
    if (fieldCount > 4)
        {
	char *score = row[4];
	char *dot = strchr(score, '.');
	if (dot) *dot = 0;  /* Chop off floating point. */
	}
    if (fieldCount > 6)
        {
	char *thickStart = row[6];
	if (sameString(thickStart, "."))
	    row[6] = row[1];	    /* Set it to same as chromStart */
	}
    if (fieldCount > 7)
        {
	char *thickEnd = row[7];
	if (sameString(thickEnd, "."))
	    row[7] = row[2];	    /* Set it to same as chromEnd */
	}
    bed = bedLoadN(row, fieldCount);
    if (fieldCount >= 12)
	if (bedFixBlocks(bed))
	    ++fixCount;
    bedOutFlexible(bed, fieldCount, f, '\t', '\n', itemRgb);
    }
carefulClose(&f);
verbose(1, "Fixed %d items in %s\n", fixCount, inBed);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
encode2BedDoctor(argv[1], argv[2]);
return 0;
}

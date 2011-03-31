/* bedRemoveOverlap - Remove overlapping records from a (sorted) bed file.  Gets rid of the 
 * smaller of overlapping records.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedRemoveOverlap - Remove overlapping records from a (sorted) bed file.  Gets rid of\n"
  "`the smaller of overlapping records.\n"
  "usage:\n"
  "   bedRemoveOverlap in.bed out.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

boolean isCommentLine(char *line)
/* Return TRUE if line is a BED comment - blank or starting with '#' */
{
char *s = skipLeadingSpaces(line);
char c = s[0];
return c == '#' || c == 0;
}

void bedRemoveOverlap(char *input, char *output)
/* bedRemoveOverlap - Remove overlapping records from a (sorted) bed file.  Gets rid of 
 * the smaller of overlapping records.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *prevChrom = NULL;
unsigned int prevStart = 0, prevEnd = 0;
char *row[1024];
int wordCount;
int maxLineSize = 1024*1024;
char *prevLine = needMem(maxLineSize);
char *curLine = needMem(maxLineSize);
int lineSize;
char *line;
boolean firstTime = TRUE;
while (lineFileNext(lf, &line, &lineSize))
    {
    /* Skip comments and make sure other lines not too long. */
    if (isCommentLine(line))
        continue;
    if (lineSize >= maxLineSize)
        errAbort("Line too long (%d chars, max is %d) line %d of %s", lineSize, 
	    maxLineSize, lf->lineIx, lf->fileName);

    /* Swap existing prevLine and curLine buffers. */
    char *tmp = prevLine;
    prevLine = curLine;
    curLine = tmp;

    /* Save current line for use next time through loop. */
    strcpy(curLine, line);

    /* Parse current line. */
    wordCount = chopLine(line, row);
    if (wordCount == ArraySize(row))
         errAbort("Too many fields (%d max is %lu) line %d of %s", wordCount, (unsigned long)ArraySize(row),
	     lf->lineIx, lf->fileName);
    char *chrom = row[0];
    unsigned int start = lineFileNeedNum(lf, row, 1);
    unsigned int end = lineFileNeedNum(lf, row, 2);

    if (firstTime)
        {
	/* First time through the loop don't output anything. */
	firstTime = FALSE;
	prevChrom = cloneString(chrom);
	}
    else
        {
	/* Check to see if we are on a new chromosome. */
	int cmp = strcmp(chrom, prevChrom);
	if (cmp != 0)
	    {
	    if (cmp < 0)
	       errAbort("%s not sorted line %d, %s before %s", 
		    lf->fileName, lf->lineIx, prevChrom, chrom);
	    fprintf(f, "%s\n", prevLine);
	    freeMem(prevChrom);
	    prevChrom = cloneString(chrom);
	    }
	else
	    {
	    if (prevStart > start)
	        errAbort("File not sorted line %d of %s", lf->lineIx, lf->fileName);
	    if (rangeIntersection(start, end, prevStart, prevEnd) > 0)
	        {
		/* Detected overlap.  Don't overlap anything this time through.
		 * instead dummy things up so bigger piece of overlap gets put
		 * out next time. */
		int curSize = end - start;
		int prevSize = prevEnd - prevStart;
		if (prevSize > curSize)
		    {
		    verbose(2, "%s:%d-%d overlaps %s:%d-%d.  Skipping %s:%d-%d line %d of %s\n",
		    	prevChrom, prevStart, prevEnd, chrom, start, end, chrom, start, end,
			lf->lineIx, lf->fileName);
		    start = prevStart;
		    end = prevEnd;
		    strcpy(curLine, prevLine);
		    }
		else
		    {
		    verbose(2, "%s:%d-%d overlaps %s:%d-%d.  Skipping %s:%d-%d line %d of %s\n",
		    	prevChrom, prevStart, prevEnd, chrom, start, end, 
			prevChrom, prevStart, prevEnd, lf->lineIx, lf->fileName);
		    /* This last bit is only needed for last line of file... */
		    prevStart = start;
		    prevEnd = end;
		    strcpy(prevLine, curLine);
		    }
		}
	    else
		fprintf(f, "%s\n", prevLine);
	    }
	}
    prevStart = start;
    prevEnd = end;
    }

/* Print last line if any. */
if (!firstTime)
    fprintf(f, "%s\n", curLine);
   

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bedRemoveOverlap(argv[1], argv[2]);
return 0;
}

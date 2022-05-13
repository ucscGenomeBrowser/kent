/* bedGraphToWig - Convert a bedGraph to a wig format that is denser.  BedGraph must be sorted.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

boolean isVar = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedGraphToWig - Convert a bedGraph to a wig format that is denser.  BedGraph must be sorted.\n"
  "usage:\n"
  "   bedGraphToWig in.bedGraph out.wig\n"
  "options:\n"
  "   -var - if set then output is variable-step\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"var", OPTION_BOOLEAN},
   {NULL, 0},
};

void bedGraphToWig(char *input, char *output)
/* bedGraphToWig - Convert a bedGraph to a wig format that is denser.  BedGraph must be sorted.. */
{
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *row[4];
char lastChrom[256] = "";
int lastEnd = 0;
int span = 0;
int stepSize = 0;
struct hash *chromHash = hashNew(0);

while (lineFileRow(lf, row))
    {
    /* Parse out input four fields */
    char *chrom = row[0];
    int start = lineFileNeedNum(lf, row, 1);
    int end = lineFileNeedNum(lf, row, 2);
    double val = lineFileNeedDouble(lf, row, 3);

    /* Make sure span is consistent */
    int oneSpan = end - start;
    if (span == 0)
        {
	if (oneSpan <= 0)
	    errAbort("end needs to be after start line %d of %s", lf->lineIx, lf->fileName);
	span = oneSpan;
	if (!isVar) // calculate fixed step,  need to lookahead a line
	    {
	    char *line;
	    if (!lineFileNextReal(lf, &line))
	        errAbort("Too short, sorry");
	    char *dupe = cloneString(line);
	    lineFileReuse(lf);
	    char *row[4];
	    int wordCount = chopLine(dupe, row);
	    lineFileExpectWords(lf, 4, wordCount);
	    if (!sameString(chrom, row[0]))
	       errAbort("odd input, only one record on chrom %s, can't handle it.", chrom);
	    int secondStart = lineFileNeedNum(lf, row, 1);
	    stepSize = secondStart - start;
	    freez(&dupe);
	    }
	    
	}
    else if (span != oneSpan)
        errAbort("Span was %d at start of file but is %d line %d of %s.  Can only handle one span.",
	    span, oneSpan, lf->lineIx, lf->fileName);

    if (!sameString(chrom, lastChrom))
        {
	if (hashLookup(chromHash, chrom) != NULL)
	    errAbort("Input needs to have all records for one chromosome together.");
	hashAdd(chromHash, chrom, NULL);
	safef(lastChrom, sizeof(lastChrom), "%s", chrom);
	lastEnd = 0;
	}

    /* Make sure is sorted */
    if (start < lastEnd)
       errAbort("%s needs to be sorted line %d", lf->fileName, lf->lineIx);


    if (!isVar)
        {
	/* Trigger indirectly fixed step to issue a new record if step size not the usual */
	if (lastEnd + stepSize != end)
	     lastEnd = 0;
	}

    /* See if on a new record */
    if (lastEnd == 0)
        {
	if (isVar)
	    {
	    fprintf(f, "variableStep chrom=%s span=%d\n", chrom, span);
	    }
	else
	    {
	    fprintf(f, "fixedStep chrom=%s start=%d span=%d step=%d\n", 
		chrom, start+1, span, stepSize);
	    }
	}

    if (isVar)
       {
       fprintf(f, "%d %g\n", start+1, val);
       }
    else
       {
       fprintf(f, "%g\n", val);
       }

    lastEnd = end;
    }
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
isVar = optionExists("var");
bedGraphToWig(argv[1], argv[2]);
return 0;
}

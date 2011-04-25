/* regCompanion5C - Analyse 5C data initially from the Dekker lab.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

double threshold = 100;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanion5C - Analyse 5C data initially from the Dekker lab.\n"
  "usage:\n"
  "   regCompanion5C input.download output.bed\n"
  "options:\n"
  "   -threshold=%g - Items over this threshold are turned into beds\n"
  , threshold
  );
}

static struct optionSpec options[] = {
   {"threshold", OPTION_DOUBLE},
   {NULL, 0},
};

struct pairItem
/* A parsed out column or row label. */
    {
    struct pairItem *next;	/* Next in list */
    char *chrom;		/* Chromosome */
    int chromStart, chromEnd;	/*Start/end locations */
    char *unparsed;		/* Full unparsed out label */
    char *id;			/* Unique item ID */
    char *db;			/* UCSC type genome database id.  hg19 currently */
    char *pos;			/* Position in chr1:1-1000 style */
    };

struct pairRow
/* A row in a pairMatrix */
     {
     struct pairRow *next;	/* Next in list. */
     struct pairItem *item;	/* Item associated with row. */
     double *array;		/* The array.  We don't keep track of size here. */
     };

struct pairMatrix 
/* A parsed out 5C matrix. */
    {
    struct pairMatrix *next;
    int colCount, rowCount;	/* dimensions of matrix */
    struct pairItem **colItems;	/* Info on each column */
    struct pairItem **rowItems;  /* Info on each row */
    double **matrix;            /* Index into this ->matrix[row][column] */
    struct pairRow *rowList;	/* We build this as we read it before we make the matrix. */
    };

char *lineFileNeedNextReal(struct lineFile *lf)
/* Get the next line that is not all whitespace and does not start with a # 
 * Complain and abort if that line doesn't exist. */
{
char *line;
if (!lineFileNextReal(lf, &line))
    lineFileUnexpectedEnd(lf);
return line;
}

void parseChromStartEnd(char *s, struct lineFile *lf, char **retChrom, int *retStart, int *retEnd)
/* Parse out chrN:start-end or die trying. Puts some 0's into s*/
{
/* Figure out where dividing elements are and complain if they aren't there. */
char *colon = strchr(s, ':');
if (colon == NULL)
    errAbort("Expecting chr:start-end got %s, line %d of %s", s, lf->lineIx, lf->fileName);
char *dash = strchr(colon+1, '-');
if (dash == NULL)
    errAbort("Expecting chr:start-end got %s, line %d of %s", s, lf->lineIx, lf->fileName);

*colon = *dash = 0;
*retChrom = s;
*retStart = sqlUnsigned(colon+1);
*retEnd = sqlUnsigned(dash+1);
}

struct pairItem *pairItemFromLabel(char *label, struct lineFile *lf)
/* Parse out label into a pair item.  Here is an example label:
 *	5C_299_ENm002_FOR_2|hg19|chr5:131258341-131260605
 * The lf argument is just for error reporting. */
{
char *dupe = cloneString(label);  /* Eaten by parsing. */

char *pipeParts[3];
int pipePartCount = chopByChar(dupe, '|', pipeParts, 3);
if (pipePartCount != 3)
    errAbort("Expecting 3 '|' separated fields, got %d line %d of %s", 
        pipePartCount, lf->lineIx, lf->fileName);

char *pos = pipeParts[2];
char *chrom;
int chromStart, chromEnd;
parseChromStartEnd(pos, lf, &chrom, &chromStart, &chromEnd);

struct pairItem *item;
AllocVar(item);
item->unparsed = cloneString(label);
item->id = cloneString(pipeParts[0]);
item->db = cloneString(pipeParts[1]);
item->chrom = cloneString(chrom);
item->chromStart = chromStart;
item->chromEnd = chromEnd;

freeMem(dupe);
return item;
}


struct pairMatrix *pairMatrixRead(char *fileName)
/* Read a file in the "download" file format used to exchange 5C data in the 
 * ENCODE project. The file can contain multiple matrices, which currently
 * are preceeded by # line type comments, some of which actually may be 
 * required metadata.  At any rate for now this routine ignores the # lines,
 * and just parses out the matrices.  The matrix starts with a tab, and then has
 * N column labels.  The next lines each begin with a row label, and have the N
 * column values.  A lot of information is encoded in the labels. */
{
struct pairMatrix *pmList = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;

/* Main loop - happens once per matrix until end of file. */
while (lineFileNextReal(lf, &line))
    {
    /* Make sure first line starts with a tab, and then chop it up by remaining tabs */
    if (*line != '\t')
	errAbort("Expecting tab at the start of first line of matrix, got '%c'", *line);
    line += 1;
    int colCount = chopByChar(line, '\t', NULL, 0);
    int wordCount = colCount+1;
    char **words;
    AllocArray(words, wordCount);
    chopByChar(line, '\t', words, colCount);

    struct pairMatrix *pm;
    AllocVar(pm);
    pm->colCount = colCount;
    struct pairItem **colItems = AllocArray(pm->colItems, colCount);

    /* Parse out labels. */
    int i;
    for (i=0; i<colCount; ++i)
	{
	colItems[i] = pairItemFromLabel(words[i], lf);
	}

    /* Loop through rows until get a short one or EOF */
    int rowCount = 0;
    struct pairRow *row, *rowList = NULL;
    while (lineFileNextReal(lf, &line))
        {
	/* Stop at first shorter-than expected line or end of file */
	int thisLineWordCount = chopByChar(line, '\t', NULL, 0);
	if (wordCount != thisLineWordCount)
	    {
	    lineFileReuse(lf);
	    break;
	    }
	chopByChar(line, '\t', words, wordCount);

	AllocVar(row);
	row->item = pairItemFromLabel(words[0], lf);
	double *array = AllocArray(row->array, colCount);
	int i;
	for (i=1; i<wordCount; ++i)
	    array[i-1] = lineFileNeedDouble(lf, words, i);

	slAddHead(&rowList, row);
	++rowCount;
	}
    slReverse(&rowList);
    
    pm->rowList = rowList;
    pm->rowCount = rowCount;
    struct pairItem **rowItems = AllocArray(pm->rowItems, rowCount);
    double **matrix = AllocArray(pm->matrix, rowCount);
    for (i=0, row = rowList; i < rowCount; ++i, row = row->next)
        {
	rowItems[i] = row->item;
	matrix[i] = row->array;
	}

    slAddHead(&pmList, pm);
    }
slReverse(&pmList);
return pmList;
}

void regCompanion5C(char *inFile, char *outFile)
/* regCompanion5C - Analyse 5C data initially from the Dekker lab.. */
{
struct pairMatrix *pm, *pmList = pairMatrixRead(inFile);
FILE *f = mustOpen(outFile, "w");
for (pm = pmList; pm != NULL; pm = pm->next)
    {
    verbose(1, "matrix %dx%d\n", pm->colCount, pm->rowCount);
    int i,j;
    for (i=0; i<pm->rowCount; ++i)
        {
	struct pairItem *rowItem = pm->rowItems[i];
	char *rowChrom = rowItem->chrom;
        for (j=0; j<pm->colCount; ++j)
	   {
	   struct pairItem *colItem = pm->colItems[j];
	   if (sameString(colItem->chrom, rowChrom))
	       {
	       double x = pm->matrix[i][j];
	       if (x >= threshold)
		    {
		    struct pairItem *startItem, *endItem;
		    if (rowItem->chromStart < colItem->chromStart)
		       {
		       startItem = rowItem;
		       endItem = colItem;
		       }
		    else
		       {
		       startItem = colItem;
		       endItem = rowItem;
		       }
		    int chromStart = startItem->chromStart;
		    int chromEnd = endItem->chromEnd;
		    fprintf(f, "%s\t%d\t%d\t", rowChrom, chromStart, chromEnd);
		    fprintf(f, "a%db%d\t", i+1, j+1);	// name
		    fprintf(f, "%d\t", round(x));	// score
		    fprintf(f, ".\t");  			// strand
		    fprintf(f, "%d\t%d\t", chromStart, chromEnd);  // thickStart/end
		    fprintf(f, "0\t");	// Reserved/itemRGB
		    fprintf(f, "2\t");	// blockCount
		    fprintf(f, "%d,%d,\t",	// blockSizes
			startItem->chromEnd - startItem->chromStart,
			endItem->chromEnd - endItem->chromStart);
		    fprintf(f, "0,%d,\n",  // chromStarts
			endItem->chromStart - chromStart);
		    }
	       }
	   }
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
threshold = optionInt("threshold", threshold);
regCompanion5C(argv[1], argv[2]);
return 0;
}

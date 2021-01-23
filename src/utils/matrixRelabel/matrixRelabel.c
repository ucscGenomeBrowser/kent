/* matrixRelabel - Relabel rows and/or columns of a matrix. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "matrixRelabel - Relabel rows and/or columns of a matrix, that is a tab-sep-value\n"
  "file where first row and first column are labels and rest are values\n"
  "usage:\n"
  "   matrixRelabel in.tsv out.tsv\n"
  "options:\n"
  "   -setCol=colLabels - set new column labels. Input is one line per label in a file\n"
  "   -setRow=rowLabels - set new row labels. Input is one line per label in a file\n"
  "   -lookupRow=lookup.tsv - pass row labels through lookup table of form old/new label\n"
  "   -trim - remove rows that don't lookup rather than aborting\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"setCol", OPTION_STRING},
   {"setRow", OPTION_STRING},
   {"lookupRow", OPTION_STRING},
   {"trim", OPTION_BOOLEAN},
   {NULL, 0},
};

struct hash *hashTwoColTsvFile(char *fileName)
/* Make a hash out of a two column tsv file where first col is key, second val */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRowTab(lf, row))
    {
    hashAdd(hash, row[0], lmCloneString(hash->lm, row[1]));
    }
lineFileClose(&lf);
return hash;
}

void readLineArray(char *fileName, int *retCount, char ***retLines)
/* Return an array of strings, one for each line of file.  Return # of lines in file too */
{
/* This is sloppy with memory but it doesn't matter since we won't free it. */
struct slName *el, *list = readAllLines(fileName);
if (list == NULL)
    errAbort("%s is empty", fileName);
int count = slCount(list);
char **lines;
AllocArray(lines, count);
int i;
for (i=0, el=list; i<count; ++i, el = el->next)
    {
    lines[i] = el->name;
    }
*retCount = count;
*retLines = lines;
}

void matrixRelabel(char *input, char *output)
/* matrixRelabel - Relabel rows and/or columns of a matrix. */
{
/* Set up stuff to relabel a column if need be */
char **newColumns = NULL;
int newColumnCount = 0;
char *newColumnFile = optionVal("setCol", NULL);
if (newColumnFile != NULL)
    readLineArray(newColumnFile, &newColumnCount, &newColumns);

/* Set up stuff to relabel a row if new be */
char **newRows = NULL;
int newRowCount = 0;
char *newRowFile = optionVal("setRow", NULL);
if (newRowFile != NULL)
    readLineArray(newRowFile, &newRowCount, &newRows);

/* Handle row lookup */
struct hash *lookupHash = NULL;
char *lookupRowFile = optionVal("lookupRow", NULL);
if (lookupRowFile != NULL)
    lookupHash = hashTwoColTsvFile(lookupRowFile);
boolean doTrim = optionExists("trim");

/* Open main input and output */
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");

/* Get first row.  Set colCount from it */
char *line;
int lineSize;
lineFileNeedNext(lf, &line, &lineSize);
int colCount = 0;
if (newColumns != NULL)
    {
    colCount = newColumnCount;
    fputs(newColumns[0], f);
    int i;
    for (i=1; i<colCount; ++i)
        fprintf(f, "\t%s", newColumns[i]);
    fputc('\n', f);
    }
else
    {
    char *word = nextTabWord(&line);
    if (word != NULL)
	{
	fprintf(f, "%s", word);
	colCount += 1;
	}
    while ((word = nextTabWord(&line)) != NULL)
         {
	 fprintf(f, "\t%s", word);
	 colCount += 1;
	 }
    fputc('\n', f);
    }

int rowIx = 1;
boolean checkedColCount = FALSE;
while (lineFileNext(lf, &line, NULL))
    {
    char *rowLabel = nextTabWord(&line);
    if (newRows != NULL)
        {
	if (rowIx >= newRowCount)
	    errAbort("Not enough lines in %s for %s", newRowFile, input);
	rowLabel = newRows[rowIx];
	++rowIx;
	}
    if (lookupHash != NULL)
        {
	char *newLabel = hashFindVal(lookupHash, rowLabel);
	if (newLabel == NULL)
	    {
	    if (doTrim)
	       continue;
	    else
	       errAbort("Couldn't find %s from line %d of %s in %s", 
			rowLabel, lf->lineIx, lf->fileName, lookupRowFile);
	    }
	rowLabel = newLabel;
	}
    if (!checkedColCount)
        {
	char *dupe = cloneString(line);
	char *s = dupe;
	int count = 1;  // Include rowLabel
	while (nextTabWord(&s) != NULL)
	    ++count;
	freez(&dupe);
	if (count != colCount)
	    lineFileExpectWords(lf, colCount, count);
	checkedColCount = TRUE;
	}
    fprintf(f, "%s\t%s\n", rowLabel, line);
    }
if (newRows != NULL)
    {
    if (rowIx != newRowCount)
        errAbort("%s has %d rows and %s has %d, not all row labels written",
	    input, rowIx, newRowFile, newRowCount);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
matrixRelabel(argv[1], argv[2]);
return 0;
}

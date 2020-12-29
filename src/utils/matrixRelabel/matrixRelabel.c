/* matrixRelabel - Relabel rows and/or columns of a matrix. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "matrixRelabel - Relabel rows and/or columns of a matrix\n"
  "usage:\n"
  "   matrixRelabel in.tsv out.tsv\n"
  "options:\n"
  "   -newCol=colLabels - one line per label in a file\n"
  "   -newRow=rowLabels - one line per label in a file\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"newCol", OPTION_STRING},
   {"newRow", OPTION_STRING},
   {"first", OPTION_STRING},
   {NULL, 0},
};

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
char *newColumnFile = optionVal("newCol", NULL);
if (newColumnFile != NULL)
    readLineArray(newColumnFile, &newColumnCount, &newColumns);


/* Set up stuff to relabel a row if new be */
char **newRows = NULL;
int newRowCount = 0;
char *newRowFile = optionVal("newRow", NULL);
if (newRowFile != NULL)
    readLineArray(newRowFile, &newRowCount, &newRows);

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
    char *word;
    while ((word = nextTabWord(&line)) != NULL)
         {
	 fprintf(f, "\t%s", word);
	 colCount += 1;
	 }
    fputc('\n', f);
    }

int rowIx = 0;
while (lineFileNext(lf, &line, NULL))
    {
    if (newRows != NULL)
        {
	++rowIx;
	if (rowIx >= newRowCount)
	    errAbort("Not enough lines in %s for %s", newRowFile, input);
	fputs(newRows[rowIx], f);
	fputc('\t', f);
	nextTabWord(&line); // skip over old first word
	}
    fputs(line, f);
    fputc('\n', f);
    }
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

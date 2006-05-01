/* rowsToCols - Convert rows to columns and vice versa in a text file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"

boolean variousWidth = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rowsToCols - Convert rows to columns and vice versa in a text file.\n"
  "usage:\n"
  "   rowsToCols in.txt out.txt\n"
  "options:\n"
  "   -variousWidth - rows may to have various numbers of columns.\n"
  );
}

static struct optionSpec options[] = {
   {"variousWidth", OPTION_BOOLEAN},
   {NULL, 0},
};

struct row
/* A parsed out row. */
    {
    struct row *next;
    int colCount;
    char *columns[1];
    };

int maxColCount(struct row *rowList)
/* Return largest column count */
{
int maxCount = 0;
struct row *row;
for (row = rowList; row != NULL; row = row->next)
    if (row->colCount > maxCount)
        maxCount = row->colCount;
return maxCount;
}

struct row *rowNew(int colCount)
/* Return row of given size. */
{
struct row *row;
row = needMem(sizeof(row) + colCount*sizeof(char*) );
row->colCount = colCount;
return row;
}


struct row *spacedLinesToRows(struct slName *lineList, char *fileName)
/* Convert lines to rows based on spaces. */
{
struct slName *line;
struct row *rowList = NULL, *row;

if (variousWidth)
    {
    for (line = lineList; line != NULL; line = line->next)
        {
	char *s = line->name;
	int rowSize = chopByWhite(s, NULL, 0);
	row = rowNew(rowSize);
	// uglyf("rowSize = %d\n", rowSize);
	// uglyf("line: %s\n", s);
	chopByWhite(s, row->columns, rowSize);
	// {int i; for (i=0; i<rowSize; ++i) uglyf("\t%s", row->columns[i]); uglyf("\n");}
	slAddHead(&rowList, row);
	}
    }
else
    {
    if (lineList)
        {
	int rowSize = chopByWhite(lineList->name, NULL, 0);
	int extraSize = rowSize+1;
	int ix = 1;
	for (line = lineList; line != NULL; line = line->next, ++ix)
	    {
	    int oneSize;
	    row = rowNew(rowSize);
	    oneSize = chopByWhite(line->name, row->columns, extraSize);
	    if (oneSize != rowSize)
	        {
		if (oneSize > rowSize)
		    errAbort("Got more than the expected %d columns line %d of %s",
			    rowSize, ix, fileName);
		else
		    errAbort("Expecting %d columns got %d, line %d of %s",
		    	rowSize, oneSize, ix, fileName);

		}
	    slAddHead(&rowList, row);
	    }
	}
    }
slReverse(&rowList);
return rowList;
}

struct row *charSepLinesToRows(struct slName *lineList, char c, char *fileName)
/* Convert lines to rows based on character separation. */
{
struct row *rowList = NULL;

return rowList;
}

void rowsToCols(char *in, char *out)
/* rowsToCols - Convert rows to columns and vice versa in a text file.. */
{
struct slName *lineList = readAllLines(in);
struct row *row, *rowList;
FILE *f;
int i, origCols;

rowList = spacedLinesToRows(lineList, in);
origCols = maxColCount(rowList);
f = mustOpen(out, "w");
for (i=0; i<origCols; ++i)
    {
    for (row = rowList; row != NULL; row = row->next)
        {
	char *s = "";
	if (i < row->colCount)
	    s = row->columns[i];
	fprintf(f, "%s", s);
	if (row->next == NULL)
	    fprintf(f, "\n");
	else
	    fprintf(f, "\t");
	}
    }
uglyf("========\n");
for (row = rowList; row != NULL; row = row->next)
    {
    uglyf("%d: ", row->colCount);
    for (i=0; i<row->colCount; ++i)
        uglyf("\t%s", row->columns[i]);
    uglyf("\n");
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
variousWidth = optionExists("variousWidth");
rowsToCols(argv[1], argv[2]);
return 0;
}

/* rowsToCols - Convert rows to columns and vice versa in a text file.. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"
#include "tabRow.h"

boolean varCol = FALSE;
boolean fixed = FALSE;
boolean tab = FALSE;
char *fs = NULL;
char *offsets = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rowsToCols - Convert rows to columns and vice versa in a text file.\n"
  "usage:\n"
  "   rowsToCols in.txt out.txt\n"
  "By default all columns are space-separated, and all rows must have the\n"
  "same number of columns.\n"
  "options:\n"
  "   -varCol - rows may to have various numbers of columns.\n"
  "   -tab - fields are separated by tab\n"
  "   -fs=X - fields are separated by given character\n"
  "   -fixed - fields are of fixed width with space padding\n"
  "   -offsets=X,Y,Z - fields are of fixed width at given offsets\n"
  );
}

static struct optionSpec options[] = {
   {"varCol", OPTION_BOOLEAN},
   {"tab",	OPTION_BOOLEAN},
   {"fs",	OPTION_STRING},
   {"fixed",    OPTION_BOOLEAN},
   {"offsets", OPTION_STRING},
   {NULL, 0},
};

void rowsToCols(char *in, char *out)
/* rowsToCols - Convert rows to columns and vice versa in a text file.. */
{
struct slName *lineList = readAllLines(in);
struct tabRow *row, *rowList;
FILE *f;
int i, origCols;
char *s;

if (fs != NULL)
    rowList = tabRowByChar(lineList, fs[0], in, varCol);
else if (fixed)
    rowList = tabRowByFixedGuess(lineList, in);
else if (offsets)
    {
    struct slName *nameList = commaSepToSlNames(offsets), *name;
    struct slInt *offList=NULL, *off;
    for (name = nameList; name != NULL; name = name->next)
         {
	 off = slIntNew(atoi(name->name));
	 slAddTail(&offList, off);
	 }
    rowList = tabRowByFixedOffsets(lineList, offList, in);
    }
else
    rowList = tabRowByWhite(lineList, in, varCol);
origCols = tabRowMaxColCount(rowList);
f = mustOpen(out, "w");
for (i=0; i<origCols; ++i)
    {
    for (row = rowList; row != NULL; row = row->next)
        {
	if (i < row->colCount)
	    s = row->columns[i];
	else
	    s = "";
	fprintf(f, "%s", s);
	if (row->next == NULL)
	    fprintf(f, "\n");
	else
	    fprintf(f, "\t");
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
varCol = optionExists("varCol");
fixed = optionExists("fixed");
fs = optionVal("fs", NULL);
if (optionExists("tab"))
    fs = "\t";
if (fs != NULL && strlen(fs) != 1)
    errAbort("fs option needs to be just one character");
offsets = optionVal("offsets", NULL);
rowsToCols(argv[1], argv[2]);
return 0;
}

/* raFromTab - Convert tab-delimited file to ra file.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "asParse.h"
#include "obscure.h"


struct slName *colList = NULL;
boolean space;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "raFromTab - Convert tab-delimited file to ra file.\n"
  "usage:\n"
  "   raFromTab in.tab out.ra\n"
  "required flags - one of:\n"
  "   as=file.as - Use autoSql file for column labels\n"
  "   cols=a,b,c - Specify column labels as comma-separated list\n"
  "options:\n"
  "   -space - Use whitespace rather than tab as field separator\n"
  );
}

static struct optionSpec options[] = {
   {"as", OPTION_STRING},
   {"cols", OPTION_STRING},
   {"space", OPTION_BOOLEAN},
   {NULL, 0},
};

void rowOut(char **row, FILE *f)
/* Write out row. */
{
int i;
struct slName *col;
for (i=0, col=colList; col != NULL; col=col->next, ++i)
    fprintf(f, "%s %s\n", col->name, row[i]);
fprintf(f, "\n");
}

void raFromTab(char *inFile, char *outFile)
/* raFromTab - Convert tab-delimited file to ra file.. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
int rowSize = slCount(colList);
char *row[rowSize];
if (space)
    {
    while (lineFileRow(lf, row))
	rowOut(row, f);
    }
else
    {
    while (lineFileRowTab(lf, row))
        rowOut(row, f);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
if (optionExists("cols"))
    colList = commaSepToSlNames(optionVal("cols", NULL));
char *fileName = optionVal("as", NULL);
if (fileName != NULL)
    {
    if (colList != NULL)
        errAbort("Use just one of options  'cols' and 'as' please.");
    struct asObject *objList = asParseFile(fileName);
    if (objList == NULL)
        errAbort("%s is empty", fileName);
    if (objList->next != NULL)
        verbose(1, "%s has multiple definitions, just using first.\n", fileName);
    struct asColumn *col;
    for (col = objList->columnList; col != NULL; col = col->next)
	slNameAddTail(&colList, col->name);
    }
if (colList == NULL)
   errAbort("Please use either 'cols' or 'as' option");
space = optionExists("space");
raFromTab(argv[1], argv[2]);
return 0;
}

/* addUcscCopyright - Add a UCSC type copyright to files without copyright. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"

boolean dry = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "addUcscCopyright - Add a UCSC type copyright to files without copyright.\n"
  "usage:\n"
  "   addUcscCopyright fileList\n"
  "options:\n"
  "   -dry - if set will just list files and years, but not add copyright\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean lowerCaseAndTest(char *s)
/* Convert s to lower case, and then return TRUE if it has 
 * copyright info in it. */
{
strLower(s);
return stringIn("copyright", s) || stringIn("(c)", s);
}

boolean containsCopyright(char *fileName, int maxLines)
/* Look for the string 'copyright' or '(C)' regardless of case in up
 * to maxLines of file.  Return TRUE if it is found, FALSE otherwise. */
{
boolean result = FALSE;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int i;
for (i=0; i<maxLines; ++i)
    {
    char *line;
    if (!lineFileNext(lf, &line, NULL))
        break;
    if (lowerCaseAndTest(line))
        {
	result = TRUE;
	break;
	}
    }
lineFileClose(&lf);
return result;
}

void addCopyright(char *fileName, int date, char *copyrightHolder, char *license)
/* Open file, load it into memory, and write it back out (replacing current file)
 * with copyright and license comments somewhere near top. */
{
verbose(1, "%s: Adding copyright %d %s\n", fileName, date, copyrightHolder);
if (fileSize(fileName) > 10000000)  /* This is a genomics file or something, not text */
    errAbort("Deciding not to add copyright to %s, which is over 10M characters long", fileName);
struct slName *line, *lineList = readAllLines(fileName);
verbose(2, "%d lines in %s\n", slCount(lineList), fileName);
if (dry)
    return;

/* Try to figure out where to put copyright.  We try to respect up to
 * 10 lines of opening comments, and will put it after opening comments
 * if possible.  However if opening comments go on too long, we put it at
 * very top. */
int maxInitialVerbage = 10, i;
struct slName *insertPoint = NULL;
boolean inBlockComment = FALSE;
for (line = lineList, i=0; line != NULL && i<maxInitialVerbage; line = line->next, ++i)
    {
    char c;
    char *s = skipLeadingSpaces(line->name);
    boolean inLineComment = (s[0] == '/' && s[1] == '/');
    boolean gotBlockComment = FALSE;
    if (inBlockComment || !inLineComment)
	{
	while ((c = *s++) != 0)
	    {
	    if (inBlockComment)
		{
		if ( c == '*' && *s == '/')
		    {
		    s += 1;
		    inBlockComment = FALSE;
		    gotBlockComment = TRUE;
		    }
		}
	    else
		{
		if ( c == '/' && *s == '*')
		    {
		    s += 1;
		    inBlockComment = TRUE;
		    gotBlockComment = TRUE;
		    }
		}
	    }
	}
    if (!inLineComment && !inBlockComment && !gotBlockComment)
        {
	insertPoint = line;
	break;
	}
    }
if (insertPoint == NULL)
    insertPoint = lineList;

FILE *f = mustOpen(fileName, "w");
for (line = lineList; line != NULL; line = line->next)
    {
    if (line == insertPoint)
        {
	if (insertPoint != lineList)
	    fprintf(f, "\n");
	fprintf(f, "/* Copyright (C) %d %s \n", date, copyrightHolder);
	fprintf(f, " * %s */\n", license);
	if (insertPoint == lineList)
	    fprintf(f, "\n");
	}
    fprintf(f, "%s\n", line->name);
    }
carefulClose(&f);
}

void addUcscCopyright(char *fileList)
/* addUcscCopyright - Add a UCSC type copyright to files. */
{
struct lineFile *lf = lineFileOpen(fileList, TRUE);
char *line;
int size;
while (lineFileNext(lf, &line, &size))
    {
    verbose(2, "%d %s\n", lf->lineIx, line);
    char *fileName = nextWordRespectingQuotes(&line);
    char *date = nextWord(&line);
    if (date != NULL)
        {
	if (!containsCopyright(fileName, 10))
	    {
	    addCopyright(fileName, sqlUnsigned(date), "The Regents of the University of California",
		"See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information.");
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
dry = optionExists("dry");
addUcscCopyright(argv[1]);
return 0;
}

/* Freen.c - do some test something */

/* Copyright (C) 2015 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "htmlPage.h"
#include "sqlList.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen fileList\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

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
    strLower(line);
    if (stringIn("copyright", line) != NULL || stringIn("(c)", line) != NULL)
        {
	result = TRUE;
	break;
	}
    }
lineFileClose(&lf);
return result;
}

void freen(char *fileList)
/* Do something, who knows what really */
{
struct lineFile *lf = lineFileOpen(fileList, TRUE);
char *line;
int size;
while (lineFileNext(lf, &line, &size))
    {
    verbose(1, "%d %s\n", lf->lineIx, line);
    char *word;
    while ((word = nextWordRespectingQuotes(&line)) != NULL)
        {
	if (containsCopyright(word, 10))
	    printf("%s\n", word);
	}
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}

/* weedLines - Selectively remove lines from file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: weedLines.c,v 1.4 2007/02/23 00:24:26 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "weedLines - Selectively remove lines from file\n"
  "usage:\n"
  "   weedLines weeds file output\n"
  "The prints all lines in file to output except those that start with\n"
  "one of the words in weeds\n"
  "options:\n"
  "   -invert Keep weeds and throw out rest\n"
  "   -weeded=fileName - Put lines weeded out here\n"
  );
}

static struct optionSpec options[] = {
   {"invert", OPTION_BOOLEAN},
   {"weeded", OPTION_STRING},
   {NULL, 0},
};

void weedLines(char *weedFile, char *file, char *output, 
	boolean invert, char *invertOutput)
/* weedLines - Selectively remove lines from file. */
{
struct hash *hash = hashWordsInFile(weedFile, 16);
verbose(2, "%d words in weed file %s\n", hash->elCount, weedFile);
struct lineFile *lf = lineFileOpen(file, TRUE);
char *line, *word, *dupe;
FILE *f = mustOpen(output, "w");
FILE *fInvert = NULL;
if (invertOutput != NULL)
    fInvert = mustOpen(invertOutput, "w");

while (lineFileNext(lf, &line, NULL))
    {
    boolean doWeed = FALSE;
    dupe = cloneString(line);
    while ((word = nextWord(&line)) != NULL)
        {
	if (hashLookup(hash, word))
	    doWeed = TRUE;
	}
    if (invert)
        doWeed = !doWeed;
    if (!doWeed)
        fprintf(f, "%s\n", dupe);
    else
        {
	if (fInvert != NULL)
	    fprintf(fInvert, "%s\n", dupe);
	}
    freez(&dupe);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
weedLines(argv[1], argv[2], argv[3], 
	optionExists("invert"), optionVal("weeded", NULL));
return 0;
}

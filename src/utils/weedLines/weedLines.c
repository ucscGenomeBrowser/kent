/* weedLines - Selectively remove lines from file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: weedLines.c,v 1.2 2007/02/15 19:57:36 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "weedLines - Selectively remove lines from file\n"
  "usage:\n"
  "   weedLines weeds file\n"
  "The prints all lines in file except those that start with\n"
  "one of the words in weeds\n"
  "options:\n"
  "   -invert Keep weeds and throw out rest\n"
  );
}

static struct optionSpec options[] = {
   {"invert", OPTION_BOOLEAN},
   {NULL, 0},
};

void weedLines(char *weedFile, char *file, boolean invert)
/* weedLines - Selectively remove lines from file. */
{
struct hash *hash = hashWordsInFile(weedFile, 16);
struct lineFile *lf = lineFileOpen(file, TRUE);
char *line, *word, *dupe;

while (lineFileNext(lf, &line, NULL))
    {
    boolean doWeed;
    dupe = cloneString(line);
    word = nextWord(&line);
    doWeed = (hashLookup(hash, word) != NULL);
    if (invert)
        doWeed = !doWeed;
    if (!doWeed)
        printf("%s\n", dupe);
    freez(&dupe);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
weedLines(argv[1], argv[2], optionExists("invert"));
return 0;
}

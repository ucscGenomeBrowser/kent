/* weedLines - Selectively remove lines from file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"


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
  "   -embedded - If this is true look for matches inside of whitespace delimited words\n"
  "               Noticably slower on large files with large weed sets.\n"
  );
}

static struct optionSpec options[] = {
   {"invert", OPTION_BOOLEAN},
   {"weeded", OPTION_STRING},
   {"embedded", OPTION_BOOLEAN},
   {NULL, 0},
};

void weedLines(char *weedFile, char *file, char *output, 
	boolean invert, char *invertOutput)
/* weedLines - Selectively remove lines from file. */
{
struct hash *hash = hashWordsInFile(weedFile, 16);
struct hashEl *weedList = hashElListHash(hash);
verbose(2, "%d words in weed file %s\n", hash->elCount, weedFile);
struct lineFile *lf = lineFileOpen(file, TRUE);
char *line, *word;
FILE *f = mustOpen(output, "w");
FILE *fInvert = NULL;
boolean embedded = optionExists("embedded");
if (invertOutput != NULL)
    fInvert = mustOpen(invertOutput, "w");

while (lineFileNext(lf, &line, NULL))
    {
    boolean doWeed = FALSE;
    char *dupe = NULL;
    if (embedded)
	{
	struct hashEl *hel;
	for (hel = weedList; hel != NULL; hel = hel->next)
	    {
	    if (stringIn(hel->name, line))
	        doWeed = TRUE;
	    }
	}
    else
	{
	dupe = cloneString(line);
	while ((word = nextWord(&line)) != NULL)
	    {
	    if (hashLookup(hash, word))
		doWeed = TRUE;
	    }
	line = dupe;
	}
    if (invert)
	doWeed = !doWeed;
    if (!doWeed)
	fprintf(f, "%s\n", line);
    else
	{
	if (fInvert != NULL)
	    fprintf(fInvert, "%s\n", line);
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

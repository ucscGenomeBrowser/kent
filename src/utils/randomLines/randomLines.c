/* randomLines - Pick out random lines from file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: randomLines.c,v 1.6 2008/10/24 01:09:24 kent Exp $";

int seed;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "randomLines - Pick out random lines from file\n"
  "usage:\n"
  "   randomLines inFile count outFile\n"
  "options:\n"
  "   -seed=N - Set seed used for randomizing, useful for debugging.\n"
  "   -decomment - remove blank lines and those starting with \n"
  );
}

static struct optionSpec options[] = {
   {"seed", OPTION_INT},
   {"decomment", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean decomment = FALSE;

void randomLines(char *inName, int count, char *outName)
/* randomLines - Pick out random lines from file. */
{
srand(seed);

/* Read all lines of input and put into an array. */
struct slName *slPt, *slList= readAllLines(inName);
int lineCount = slCount(slList);
char **lineArray;
AllocArray(lineArray, lineCount);
int i;
for (i=0, slPt=slList; i<lineCount; ++i, slPt = slPt->next)
    lineArray[i] = slPt->name;

/* Avoid an infinite, or very long loop by not letting them ask for all
 * the lines except in the small case. */
int maxCount = lineCount/2;
if (maxCount < 1000)
    maxCount = lineCount;
if (count > maxCount)
    errAbort("%s has %d lines.  Random lines will only output %d or less lines\n"
             "on a file this size. You asked for %d. Sorry.",
    	inName, lineCount, maxCount, count);

FILE *f = mustOpen(outName, "w");
int outCount = 0;
while (outCount < count)
    {
    int randomIx = rand()%lineCount;
    char *line = lineArray[randomIx];
    if (line)
	{
        fprintf(f, "%s\n", line);
	++outCount;
	lineArray[randomIx] = NULL;
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4 || !isdigit(argv[2][0]))
    usage();
seed = optionInt("seed", (int)time(NULL));
decomment = optionExists("decomment");
randomLines(argv[1], atoi(argv[2]), argv[3]);
return 0;
}

/* addCols - Add together columns. */
#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "addCols - Sum columns in a text file.\n"
  "usage:\n"
  "   addCols <fileName>\n"
  "adds all columns (up to 16 columns) in the given file, \n"
  "outputs the sum of each column.  <fileName> can be the\n"
  "name: stdin to accept input from stdin.");
}

void addCols(char *fileName)
/* addCols - Add together columns. */
{
struct lineFile *lf;
char *words[16];
int wordCount;
static double totals[16];
int maxCount = 0;
int i;

if (sameString(fileName, "stdin"))
    lf = lineFileStdin(TRUE);
else
    lf = lineFileOpen(fileName, TRUE);
for (i=0; i<ArraySize(words); ++i)
     totals[i] = 0;
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    if (maxCount < wordCount)
        maxCount = wordCount;
    for (i=0; i<wordCount; ++i)
        totals[i] += atof(words[i]);
    }
for (i=0; i<maxCount; ++i)
     printf("%6.2f ", totals[i]);
printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
addCols(argv[1]);
return 0;
}

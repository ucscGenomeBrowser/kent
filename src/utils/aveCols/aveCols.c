/* aveCols - Add together columns. */
#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "aveCols - average together columns\n"
  "usage:\n"
  "   aveCols file\n"
  "adds all columns (up to 16 columns) in the given file, \n"
  "outputs the average (sum/#ofRows) of each column.  <fileName> can be the\n"
  "name: stdin to accept input from stdin.");
}

void aveCols(char *fileName)
/* aveCols - Add together columns. */
{
struct lineFile *lf;
char *words[16];
int wordCount;
double totals[16];
int maxCount = 0;
int i;
int lineCount = 0;

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
    ++lineCount;
    }
for (i=0; i<maxCount; ++i)
     printf("%7.3f ", totals[i]/lineCount);
printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
aveCols(argv[1]);
return 0;
}

/* lineCount - Count lines in a file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "lineCount - Count lines in a file\n"
  "usage:\n"
  "   lineCount file(s)\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void lineCount(int fileCount, char *fileNames[])
/* lineCount - Count lines in a file. */
{
long total = 0;
long oneFile = 0;
struct lineFile *lf;
char *line;
int i;
for (i=0; i<fileCount; ++i)
    {
    lf = lineFileOpen(fileNames[i], FALSE);
    oneFile = 0;
    while (lineFileNext(lf, &line, NULL))
        ++oneFile;
    printf("%ld\t%s\n", oneFile, lf->fileName);
    lineFileClose(&lf);
    total += oneFile;
    }
if (fileCount > 1)
    printf("%ld\n", total);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 2)
    usage();
lineCount(argc-1, argv+1);
return 0;
}

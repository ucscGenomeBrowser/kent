/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "freen - My Pet Freen\n"
  "usage:\n"
  "   freen inFile\n");
}

void freen(char *inFile)
/* freen - My Pet Freen. */
{
struct lineFile *lf = lineFileOpen(inFile, FALSE);
char *line;
int lineSize;

while (lineFileNext(lf, &line, &lineSize))
    {
    mustWrite(stdout, line, lineSize);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}

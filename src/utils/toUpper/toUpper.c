/* toUpper - Convert lower case to upper case in file. Leave other chars alone. */
#include "common.h"
#include "linefile.h"

static char const rcsid[] = "$Id: toUpper.c,v 1.2 2003/05/06 07:41:09 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "toUpper - Convert lower case to upper case in file. Leave other chars alone\n"
  "usage:\n"
  "   toUpper in out\n");
}

void toUpper(char *inName, char *outName)
/* toUpper - Convert lower case to upper case in file. Leave other chars alone. */
{
struct lineFile *lf = lineFileOpen(inName, FALSE);
FILE *f = mustOpen(outName, "w");
int lineSize;
char *line;

while (lineFileNext(lf, &line, &lineSize))
    {
    toUpperN(line, lineSize);
    mustWrite(f, line, lineSize);
    }
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
toUpper(argv[1], argv[2]);
return 0;
}

/* faOneRecord - Extract a single record from a .FA file. */
#include "common.h"
#include "linefile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faOneRecord - Extract a single record from a .FA file\n"
  "usage:\n"
  "   faOneRecord in.fa recordName\n");
}

void faOneRecord(char *fileName, char *recordName)
/* faOneRecord - Extract a single record from a .FA file. */
{
FILE *f = mustOpen(fileName, "r");
struct lineFile *lf = lineFileOpen(fileName, FALSE);
int lineSize;
char *line;
boolean inRecord = FALSE;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
	inRecord =  startsWith(recordName, line+1);
    if (inRecord)
        mustWrite(stdout, line, lineSize);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
faOneRecord(argv[1], argv[2]);
return 0;
}

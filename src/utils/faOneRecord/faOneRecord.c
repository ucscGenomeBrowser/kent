/* faOneRecord - Extract a single record from a .FA file. */
#include "common.h"
#include "linefile.h"

static char const rcsid[] = "$Id: faOneRecord.c,v 1.3 2003/11/14 23:53:17 angie Exp $";

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
	{
	char *header = cloneString(line+1);
	char *words[2];
	chopLine(header, words);
	inRecord = sameString(recordName, words[0]);
	freeMem(header);
	}
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

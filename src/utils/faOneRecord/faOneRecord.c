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

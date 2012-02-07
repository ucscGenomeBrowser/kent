/* fixGdup - Reformat genomic dups table a little.. */
#include "common.h"
#include "linefile.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "fixGdup - Reformat genomic dups table a little.\n"
  "usage:\n"
  "   fixGdup inFile outFile\n");
}

void fixGdup(char *inName, char *outName)
/* fixGdup - Reformat genomic dups table a little.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
int wordCount, lineSize;
char *words[32], *line;
int i;


while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopTabs(line, words);
    if (wordCount == 0)
        continue;
    lineFileExpectWords(lf, 15, wordCount);
    for (i=0; i<3; ++i)
        fprintf(f, "%s\t", words[i]);
    fprintf(f, "%s:%s\t", words[6], words[7]);
    for (i=4; i<9; ++i)
        fprintf(f, "%s\t", words[i]);
    for (i=10; i<wordCount; ++i)
	{
        fprintf(f, "%s", words[i]);
	if (i == wordCount-1)
	    fprintf(f, "\n");
	else
	    fprintf(f, "\t");
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
fixGdup(argv[1], argv[2]);
return 0;
}

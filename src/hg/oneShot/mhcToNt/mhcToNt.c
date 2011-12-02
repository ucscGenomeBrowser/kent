/* mhcToNt - Convert Roger's MHC file to NT file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mhcToNt - Convert Roger's MHC file to NT file\n"
  "usage:\n"
  "   mhcToNt mhcOrder.txt output.agp\n");
}

void mhcToNt(char *inName, char *outName)
/* mhcToNt - Convert Roger's MHC file to NT file. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *line, *words[16];
int lineSize, wordCount;
int mhcIx = 1;
int contigIx = 0;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '#')
        continue;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        {
	++mhcIx;
	contigIx = 0;
	continue;
	}
    lineFileExpectWords(lf, 7, wordCount);
    fprintf(f, "NT_MHC%d\t%s\t%s\t%d\tF\t%s\t%s\t%s\t%s\n",
    	mhcIx, words[5], words[6], ++contigIx, 
	words[1], words[3], words[4], words[2]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
mhcToNt(argv[1], argv[2]);
return 0;
}

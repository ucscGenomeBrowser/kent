/* netToBed - Convert target coverage of net to a bed file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netToBed - Convert target coverage of net to a bed file.\n"
  "This is not sophisticated.  It will not remove gaps.\n"
  "usage:\n"
  "   netToBed in.net out.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netToBed(char *inName, char *outName)
/* netToBed - Convert target coverage of net to a bed file.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
char *row[3];
char *chrom = NULL;
int start, size;

while (lineFileRow(lf, row))
    {
    if (sameWord("net", row[0]))
	{
	freeMem(chrom);
	chrom = cloneString(row[1]);
	}
    else
        {
	start = lineFileNeedNum(lf, row, 1);
	size = lineFileNeedNum(lf, row, 2);
	fprintf(f, "%s\t%d\t%d\n", chrom, start, start+size);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
netToBed(argv[1], argv[2]);
return 0;
}

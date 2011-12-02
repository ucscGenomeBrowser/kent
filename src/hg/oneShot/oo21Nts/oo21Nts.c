/* oo21Nts - Make table of locations of NT contigs on oo21. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "ooUtils.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "oo21Nts - Make table of locations of NT contigs on oo21\n"
  "usage:\n"
  "   oo21Nts out.tab\n");
}

FILE *out;

void doContig(char *dir, char *chrom, char*contig)
/* Extract NT info from one contig. */
{
char fileName[512];
struct lineFile *lf = NULL;
char *words[4];

sprintf(fileName, "%s/ooGreedy.93.gl", dir);
lf = lineFileMayOpen(fileName, TRUE);
if (lf != NULL)
    {
    while (lineFileRow(lf, words))
	{
	if (startsWith("NT_", words[0]))
	    {
	    fprintf(out, "%s\t%s\t%s\t%s\t%s\n",
		    contig, words[1], words[2], words[0], words[3]);
	    }
	}
    }
lineFileClose(&lf);
}

void oo21Nts(char *outName)
/* oo21Nts - Make table of locations of NT contigs on oo21. */
{
out = mustOpen(outName, "w");
ooToAllContigs("/projects/hg/gs.5/oo.21", doContig);
carefulClose(&out);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
oo21Nts(argv[1]);
return 0;
}

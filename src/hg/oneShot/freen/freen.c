/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "fa.h"

static char const rcsid[] = "$Id: freen.c,v 1.41 2004/02/23 06:49:42 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

void faToEveryOtherN(char *inName, char *outName)
/* Read fa file and scan for N's. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct dnaSeq seq;

ZeroVar(&seq);
while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    int nCount, realCount;
    char *dna = seq.dna;
    fprintf(f, "%s\n", seq.name);
    for (;;)
        {
	int nCount = 0, realCount=0;
	char c;
	if ((c = *dna) == 0)
	    break;
	while (c == 'n')
	    {
	    ++nCount;
	    c = *++dna;
	    }
	while (c != 0 && c != 'n')
	    {
	    ++realCount;
	    c = *++dna;
	    }
	printf("%d\t%d\n", nCount, realCount);
	}
    }
}

void freen(char *in)
/* Test some hair-brained thing. */
{
faToEveryOtherN(in, "stdout");
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}

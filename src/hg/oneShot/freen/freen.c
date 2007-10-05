/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "jksql.h"
#include "hdb.h"


static char const rcsid[] = "$Id: freen.c,v 1.79 2007/09/27 19:49:31 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

void freen(char *fileName)
/* Test some hair-brained thing. */
{
hSetDb("hg18");
struct slName *chrom, *chromList = hAllChromNames();
FILE *f = mustOpen(fileName, "w");
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    int size = hChromSize(chrom->name);
    int start = 0, end;
    int step = 100000;
    int itemSize = 100;
    for (end = start + step; end <= size; end += step)
        {
	fprintf(f, "%s\t%d\t%d\tnothing\t%d\n",
		chrom->name, start, start+itemSize,
		start/step*100%1000 + 90);
	start = end;
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000*1024L);
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}

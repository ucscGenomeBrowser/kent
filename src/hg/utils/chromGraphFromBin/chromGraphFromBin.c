/* chromGraphFromBin - Convert chromGraph binary to ascii format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "chromGraph.h"

static char const rcsid[] = "$Id: chromGraphFromBin.c,v 1.2 2006/06/13 16:15:18 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chromGraphFromBin - Convert chromGraph binary to ascii format.\n"
  "usage:\n"
  "   chromGraphFromBin in.chromGraph out.tab\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void chromGraphFromBin(char *inFile, char *outFile)
/* chromGraphFromBin - Convert chromGraph binary to ascii format.. */
{
struct chromGraphBin *cgb = chromGraphBinOpen(inFile);
FILE *f = mustOpen(outFile, "w");
while (chromGraphBinNextChrom(cgb))
    {
    while (chromGraphBinNextVal(cgb))
        {
	fprintf(f, "%s\t%d\t%g\n", cgb->chrom, cgb->chromStart, cgb->val);
	}
    }
chromGraphBinFree(&cgb);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
chromGraphFromBin(argv[1], argv[2]);
return 0;
}

/* chromGraphFromBin - Convert chromGraph binary to ascii format.. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "chromGraph.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "chromGraphFromBin - Convert chromGraph binary to ascii format.\n"
  "usage:\n"
  "   chromGraphFromBin in.chromGraph out.tab\n"
  "options:\n"
  "   -chrom=chrX - restrict output to single chromosome\n"
  );
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {NULL, 0},
};

void chromGraphFromBin(char *inFile, char *outFile, char *chrom)
/* chromGraphFromBin - Convert chromGraph binary to ascii format.. */
{
struct chromGraphBin *cgb = chromGraphBinOpen(inFile);
FILE *f = mustOpen(outFile, "w");
if (chrom == NULL)
    {
    while (chromGraphBinNextChrom(cgb))
	{
	while (chromGraphBinNextVal(cgb))
	    {
	    fprintf(f, "%s\t%d\t%g\n", cgb->chrom, cgb->chromStart, cgb->val);
	    }
	}
    }
else
    {
    if (chromGraphBinSeekToChrom(cgb, chrom))
        {
	while (chromGraphBinNextVal(cgb))
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
chromGraphFromBin(argv[1], argv[2], optionVal("chrom", NULL));
return 0;
}

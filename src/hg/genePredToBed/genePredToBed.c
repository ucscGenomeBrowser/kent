/* genePredToBed - Convert from genePred to bed format.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genePred.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredToBed - Convert from genePred to bed format. Does not yet handle genePredExt\n"
  "usage:\n"
  "   genePredToBed in.genePred out.bed\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void convertGenePredToBed(char *inFile, char *outFile)
/* genePredToBed - Convert from genePred to bed format.. */
{
struct genePred *gp, *gpList= genePredLoadAll(inFile);
FILE *f = mustOpen(outFile, "w");
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    /* Print scalar bed fields. */
    fprintf(f, "%s\t", gp->chrom);
    fprintf(f, "%u\t", gp->txStart);
    fprintf(f, "%u\t", gp->txEnd);
    fprintf(f, "%s\t", gp->name);
    fprintf(f, "%u\t", 0);
    fprintf(f, "%s\t", gp->strand);
    fprintf(f, "%u\t", gp->cdsStart);
    fprintf(f, "%u\t", gp->cdsEnd);
    fprintf(f, "%u\t", 0);
    fprintf(f, "%u\t", gp->exonCount);

    /* Print exon-by-exon fields. */
    int i;

    /* Print exon sizes */
    for (i=0; i<gp->exonCount; ++i)
        fprintf(f, "%u,", gp->exonEnds[i] - gp->exonStarts[i]);
    fprintf(f, "\t");

    /* Print exons starts */
    for (i=0; i<gp->exonCount; ++i)
	fprintf(f, "%u,", gp->exonStarts[i] - gp->txStart);
    fprintf(f, "\n");
    }
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
convertGenePredToBed(argv[1], argv[2]);
return 0;
}

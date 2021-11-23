/* genePredToBed - Convert from genePred to bed format.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
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
  "   -tab - genePred fields are separated by tab instead of just white space\n"
  "   -fillSpace - when tab input, fill space chars in 'name' with underscore: _\n"
  "   -score=N - set score to N in bed output (default 0)"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"tab", OPTION_BOOLEAN},
   {"fillSpace", OPTION_BOOLEAN},
   {"score", OPTION_INT},
   {NULL, 0},
};

boolean tabSep = FALSE;
boolean fillSpace = FALSE;
int defaultScore = 0;

void convertGenePredToBed(char *inFile, char *outFile)
/* genePredToBed - Convert from genePred to bed format.. */
{
struct genePred *gp = NULL;
struct genePred *gpList = NULL;
if (tabSep)
  gpList= genePredLoadAllByChar(inFile, '\t');
else
  gpList= genePredLoadAll(inFile);

FILE *f = mustOpen(outFile, "w");
for (gp = gpList; gp != NULL; gp = gp->next)
    {
    /* Print scalar bed fields. */
    fprintf(f, "%s\t", gp->chrom);
    fprintf(f, "%u\t", gp->txStart);
    fprintf(f, "%u\t", gp->txEnd);
    if (tabSep && fillSpace)
        {
        replaceChar(gp->name, ' ', '_');
        fprintf(f, "%s\t", gp->name);
        }
    else
        fprintf(f, "%s\t", gp->name);
    fprintf(f, "%u\t", defaultScore);
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
tabSep = optionExists("tab");
fillSpace = optionExists("fillSpace");
defaultScore = optionInt("score", defaultScore);
convertGenePredToBed(argv[1], argv[2]);
return 0;
}

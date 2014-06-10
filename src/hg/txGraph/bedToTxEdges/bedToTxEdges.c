/* bedToTxEdges - Convert a bed file into txEdgeBed, which can be used with txgAddEvidence.. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "txEdgeBed.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedToTxEdges - Convert a bed file into txEdgeBed, which can be used with txgAddEvidence.\n"
  "usage:\n"
  "   bedToTxEdges in.bed out.edges\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void edgeOut(char *chrom, int chromStart, int chromEnd, char *name, int score, char *strand,
	char *startType, char *edgeType, char *endType, FILE *f)
/* Write edge to file. */
{
fprintf(f, "%s\t%d\t%d\t", chrom, chromStart, chromEnd);
fprintf(f, "%s\t%d\t%s\t", name, score, strand);
fprintf(f, "%s\t%s\t%s\n", startType, edgeType, endType);
}

void bedToTxEdges(char *inBed, char *outTxg)
/* bedToTxEdges - Convert a bed file into txEdgeBed, which can be used with txgAddEvidence. */
{
struct bed *bedList = bedLoadAll(inBed);
FILE *f = mustOpen(outTxg, "w");
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->strand[0] == 0)
        errAbort("No strand defined.  Input must have fields up to strand at least.");
    if (bed->blockCount == 0)
        {
	edgeOut(bed->chrom, bed->chromStart, bed->chromEnd, bed->name, bed->score,
		bed->strand, "(", "exon", ")", f);
	}
    else
        {
	char *startType = "(", *endType = "]";
	int i;
	int lastBlockIx = bed->blockCount-1;

	/* Loop through all but last exon, and all introns. */
	for (i=0; i<lastBlockIx; ++i)
	    {
	    int start = bed->chromStart + bed->chromStarts[i];
	    int end = start + bed->blockSizes[i];
	    edgeOut(bed->chrom, start, end, bed->name, bed->score, bed->strand,
	    	startType, "exon", endType, f);
	    int nextStart = bed->chromStart + bed->chromStarts[i+1];
	    edgeOut(bed->chrom, end, nextStart, bed->name, bed->score, bed->strand,
	    	"]", "intron", "[", f);
	    startType = "[";
	    }
	/* Do final exon. */
	int start = bed->chromStart + bed->chromStarts[lastBlockIx];
	int end = start + bed->blockSizes[lastBlockIx];
	edgeOut(bed->chrom, start, end, bed->name, bed->score, bed->strand,
	    (lastBlockIx == 0 ? "(" : "["), 
	    "exon", ")", f);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bedToTxEdges(argv[1], argv[2]);
return 0;
}

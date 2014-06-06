/* edwBamToWig - Convert a bam file to a wig file by measuring depth of coverage, optionally adjusting hit size to average for library.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "bamFile.h"
#include "genomeRangeTree.h"

int clPad = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwBamToWig - Convert a bam file to a wig file by measuring depth of coverage, optionally adjusting hit size to average for library.\n"
  "usage:\n"
  "   edwBamToWig in.bam out.wig\n"
  "options:\n"
  "   -pad=N - add a number, usually related to library insert size, to end of bam hit\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"pad", OPTION_INT},
   {NULL, 0},
};

void edwBamToWig(char *input, char *output)
/* edwBamToWig - Convert a bam file to a wig file by measuring depth of coverage, optionally adjusting hit size to average for library.. */
{
FILE *f = mustOpen(output, "w");
/* Open file and get header for it. */
samfile_t *sf = samopen(input, "rb", NULL);
if (sf == NULL)
    errnoAbort("Couldn't open %s.\n", input);
bam_header_t *head = sf->header;
if (head == NULL)
    errAbort("Aborting ... Bad BAM header in file: %s", input);


/* Scan through input populating genome range trees */
struct genomeRangeTree *grt = genomeRangeTreeNew();
bam1_t one = {};
for (;;)
    {
    /* Read next record. */
    if (bam_read1(sf->x.bam, &one) < 0)
	break;
    if (one.core.tid >= 0 && one.core.n_cigar > 0)
	{
	char *chrom = head->target_name[one.core.tid];
	int start = one.core.pos;
	int end = start + one.core.l_qseq;
	if (one.core.flag & BAM_FREVERSE)
	    {
	    start -= clPad;
	    }
	else
	    {
	    end += clPad;
	    }
	struct rbTree *rt = genomeRangeTreeFindOrAddRangeTree(grt,chrom);
	rangeTreeAddToCoverageDepth(rt, start, end);
	}
    }


/* Convert genome range tree into output wig */

/* Get list of chromosomes. */
struct hashEl *hel, *helList = hashElListHash(grt->hash);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    char *chrom = hel->name;
    struct rbTree *rt = hel->val;
    struct range *range, *rangeList = rangeTreeList(rt);
    for (range = rangeList; range != NULL; range = range->next)
         {
	 fprintf(f, "%s\t%d\t%d\t%d\n",  chrom, range->start, range->end, ptToInt(range->val));
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
clPad = optionInt("pad", clPad);
edwBamToWig(argv[1], argv[2]);
return 0;
}

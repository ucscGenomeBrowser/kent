/* edwBamRemoveChrom - Remove a chromosome from BAM file.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwBamRemoveChrom - Remove a chromosome from BAM file.\n"
  "usage:\n"
  "   edwBamRemoveChrom input chrom output\n"
  "You can also do this with the more general purpose edwBamFilter.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwBamRemoveChrom(char *inBam, char *chrom, char *outBam)
/* edwBamRemoveChrom - Remove a chromosome from BAM file.. */
{
/* Open file and get header for it. */
samfile_t *in = bamMustOpenLocal(inBam, "rb", NULL);
bam_hdr_t *head = sam_hdr_read(in);

/* Find out index number of chromosome. */
int i;
int rmTid = -1;
for (i=0; i<head->n_targets; ++i)
    {
    if (sameString(chrom, head->target_name[i]))
	{
        rmTid = i;
	break;
	}
    }
if (rmTid == -1)
    errAbort("Couldn't find %s in %s", chrom, inBam);
verbose(2, "rmTid for %s is %d\n", chrom, rmTid);

/* Open up sam output and write header */
samfile_t *out = bamMustOpenLocal(outBam, "wb", head);

/* Loop through copying in to out except where it is chromosome to be removed */
int rmCount = 0;
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
for (;;)
    {
    /* Read next record. */
    if (sam_read1(in, head, &one) < 0)
	break;

    /* Just consider mapped reads. */
    if ((one.core.flag & BAM_FUNMAP) == 0 && one.core.tid >= 0)
        {
	if (one.core.tid == rmTid)
	    ++rmCount;
	else
	    sam_write1(out, head, &one);
	}
    }
samclose(in);
samclose(out);
verbose(1, "Removed %d reads from %s\n", rmCount, chrom);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
edwBamRemoveChrom(argv[1], argv[2], argv[3]);
return 0;
}

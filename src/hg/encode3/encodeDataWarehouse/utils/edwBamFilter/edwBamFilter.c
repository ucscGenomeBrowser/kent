/* edwBamFilter - Remove reads from a BAM file based on a number of criteria.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* version history: 
 *    1 - initial release 
 *    2 - added sponge option
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"
#include "edwLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwBamFilter v2 - Remove reads from a BAM file based on a number of criteria.\n"
  "usage:\n"
  "   edwBamFilter in.bam out.bam\n"
  "options:\n"
  "   -chrom=chrM - remove a chromosome (in this case chrM)\n"
  "   -multimapped - remove multiply mapping reads\n"
  "   -unmapped - remove unmapped reads\n"
  "   -sponge - remove reads with names starting with mito| or ribo| or other|\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"multimapped", OPTION_BOOLEAN},
   {"unmapped", OPTION_BOOLEAN},
   {"sponge", OPTION_BOOLEAN},
   {NULL, 0},
};

void edwBamFilter(char *inBam, char *outBam, char *chrom, boolean multimapped, boolean unmapped,
    boolean sponge)
/* edwBamFilter - Remove reads from a BAM file based on a number of criteria.. */
{
/* Open file and get header for it. */
samfile_t *in = bamMustOpenLocal(inBam, "rb", NULL);
bam_hdr_t *head = sam_hdr_read(in);
unsigned char *skipChrom = needLargeZeroedMem(head->n_targets);

/* Find out index number of chromosome and force it to be skipped. */
if (chrom != NULL)
    {
    int i;
    for (i=0; i<head->n_targets; ++i)
	{
	if (sameString(chrom, head->target_name[i]))
	    {
	    skipChrom[i] = TRUE;
	    break;
	    }
	}
    if (i == head->n_targets)
	errAbort("Couldn't find %s in %s", chrom, inBam);
    }

/* If sponge in play filter out sponge-named-sequences */
if (sponge)
    {
    int spongeSize = 0;
    int i;
    for (i=0; i<head->n_targets; ++i)
	{
	char *name = head->target_name[i];
	if (startsWith("other|", name) || startsWith("mito|", name) || startsWith("ribo|", name)
	   || startsWith("cen", name))
	    {
	    assert(i < head->n_targets);
	    skipChrom[i] = TRUE;
	    ++spongeSize;
	    }
	}
    verbose(1, "Sponge size %d\n", spongeSize);
    }


/* Open up sam output and write header */
samfile_t *out = bamMustOpenLocal(outBam, "wb", head);

/* Loop through copying in to out except where filtered out */
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
int readCount = 0, writeCount = 0;
for (;;)
    {
    /* Read next record. */
    if (sam_read1(in, head, &one)  < 0)
	break;
    ++readCount;

    /* Just consider mapped reads. */
    if (one.core.flag & BAM_FUNMAP)
        {
	if (unmapped)
	    continue;
	}
    else
        {
	if (skipChrom[one.core.tid])
	    continue;
        if (multimapped && one.core.qual <= edwMinMapQual)
            continue;
	}
    
    sam_write1(out, head, &one);
    ++writeCount;
    }
samclose(in);
samclose(out);
verbose(1, "Wrote %d of %d reads\n", writeCount, readCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwBamFilter(argv[1], argv[2], optionVal("chrom", NULL), optionExists("multimapped"),
    optionExists("unmapped"), optionExists("sponge"));
return 0;
}

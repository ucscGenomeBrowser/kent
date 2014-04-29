/* edwBamFilter - Remove reads from a BAM file based on a number of criteria.. */

/* version history: 
 *    1 - initial release 
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
  "edwBamFilter v1 - Remove reads from a BAM file based on a number of criteria.\n"
  "usage:\n"
  "   edwBamFilter in.bam out.bam\n"
  "options:\n"
  "   -chrom=chrM - remove a chromosome (in this case chrM)\n"
  "   -multimapped - remove multiply mapping reads\n"
  "   -unmapped - remove unmapped reads\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"multimapped", OPTION_BOOLEAN},
   {"unmapped", OPTION_BOOLEAN},
   {NULL, 0},
};

void edwBamFilter(char *inBam, char *outBam, char *chrom, boolean multimapped, boolean unmapped)
/* edwBamFilter - Remove reads from a BAM file based on a number of criteria.. */
{
/* Open file and get header for it. */
samfile_t *in = bamMustOpenLocal(inBam, "rb", NULL);
bam_header_t *head = in->header;

/* Find out index number of chromosome. */
int i;
int rmTid = -1;
if (chrom != NULL)
    {
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
    if (samread(in, &one) < 0)
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
	if (rmTid == one.core.tid)
	    continue;
        if (multimapped && one.core.qual <= edwMinMapQual)
            continue;
	}
    
    samwrite(out, &one);
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
    optionExists("unmapped"));
return 0;
}

/* edwBamRemoveChrom - Remove a chromosome from BAM file.. */
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
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

samfile_t *samMustOpen(char *fileName, char *mode, void *extraHeader)
/* Open up samfile or die trying */
{
samfile_t *sf = samopen(fileName, mode, extraHeader);
if (sf == NULL)
    errnoAbort("Couldn't open %s.\n", fileName);
return sf;
}

void edwBamRemoveChrom(char *inBam, char *chrom, char *outBam)
/* edwBamRemoveChrom - Remove a chromosome from BAM file.. */
{
/* Open file and get header for it. */
samfile_t *in = samMustOpen(inBam, "rb", NULL);
bam_header_t *head = in->header;

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
samfile_t *out = samMustOpen(outBam, "wb", head);

/* Loop through copying in to out except where it is chromosome to be removed */
int rmCount = 0;
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
for (;;)
    {
    /* Read next record. */
    if (samread(in, &one) < 0)
	break;

    /* Just consider mapped reads. */
    if ((one.core.flag & BAM_FUNMAP) == 0 && one.core.tid >= 0)
        {
	if (one.core.tid == rmTid)
	    ++rmCount;
	else
	    samwrite(out, &one);
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

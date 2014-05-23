/* bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome . */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"
void usage()
/* Explain usage and exit. */
{
errAbort(
  "bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome \n"
  "usage:\n"
  "   bamSplitByChrom input.bam\n"
  "options:\n"
  "   -prefix= chromosome prefix\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};
void printBamSeq()
{
}

samfile_t *samMustOpen(char *fileName, char *mode, void *extraHeader)
/* Open up samfile or die trying. */
{
samfile_t *sf = samopen(fileName, mode, extraHeader);
if (sf == NULL)
    errnoAbort("Couldn't open %s.\n", fileName);
return sf;
}


void bamSplitByChrom(char *inBam)
/* bamSplitByChrom -  Splits a bam file into multiple bam files based on chromosome . */
{
/* Open file and get header for it. */
samfile_t *in = samMustOpen(inBam, "rb", NULL);
bam_header_t *head = in->header;

/* Find out index number of chromosome. */
int i;
int rmTid = -1;
for (i=0; i<head->n_targets; ++i)
    {


        rmTid = i;
	break;

    }




/* Open up sam output and write header */
char *outBam = "output";
samfile_t *out = bamMustOpenLocal(outBam, "wb", head);

/* Loop through copying in to out except where it is chromosome to be removed */
/* Lets try and 'borrow' this loop and throw in an if statment checking the *
 * chromosome of the read, then it should be a simple matter to write each 
 * one to its respective output */

int rmCount = 0;
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
for (;;)
    {
    /* Read next record. */
    if (samread(in, &one) < 0)
	break;
//	int32_t chrom = one.core.tid;
    
   

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

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
bamSplitByChrom(argv[1]);
return 0;
}

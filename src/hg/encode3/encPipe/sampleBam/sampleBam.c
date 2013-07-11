/* sampleBam - Generate a sample of a BAM file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bamFile.h"
#include "obscure.h"
#include "sqlNum.h"

int seed = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sampleBam - Generate a sample of a BAM file.\n"
  "usage:\n"
  "   sampleBam in.bam inSize outSize out.bed\n"
  "where:\n"
  "   in.bam is the full input bam\n"
  "   inSize is the number of reads in in.bam\n"
  "   outSize is the number of reads we want to sample\n"
  "   out.sam is the output in sam format (text format, needs to be converted to BAM)\n"
  "options:\n"
  "   -seed=N - random number seed, defaults to 0\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"seed", OPTION_INT},
   {NULL, 0},
};

void sampleBam(char *inputBam, char *totalSizeString, char *sampleSizeString, char *output)
/* sampleBam - Generate a sample of a BAM file. */
{
FILE *f = mustOpen(output, "w");
int sampleSize = sqlUnsigned(sampleSizeString);
int totalSize = sqlUnsigned(totalSizeString);
if (sampleSize > totalSize)
    errAbort("Sample size greater thatn totalSize");

/* Make up an array of values 0-totalSize-1, and shuffle it randomly. */
int *array;
AllocArray(array, totalSize);
int i;
for (i=0; i<totalSize; ++i)
   array[i] = i;
shuffleArrayOfInts(array, totalSize);

/* Sort first sampleSize elements - all we'll use. */
intSort(sampleSize, array);

/* Read sampleSized elements positioned randomly through file as
 * guided by sorted array. */
samfile_t *sf = samopen(inputBam, "rb", NULL);
bam_header_t *bamHeader = sf->header;
bam1_t one;
ZeroVar(&one);	// This seems to be necessary!
int curPos = 0;
for (i=0; i<sampleSize; ++i)
    {
    int nextPos = array[i];
    while (++curPos <= nextPos)
        {
	int err = bam_read1(sf->x.bam, &one);
	if (err < 0)
	    errAbort("Couldn't get sample %d from %s", nextPos, inputBam);
	}

    int32_t tid = one.core.tid;
    int l_qseq = one.core.l_qseq;
    if (tid > 0)
       {
       char *chrom = bamHeader->target_name[tid];
       int start = one.core.pos;
       // Approximate here... can do better if parse cigar.
       int end = start + l_qseq;	
       boolean isRc = (one.core.flag & BAM_FREVERSE);
       char strand = '+';
       if (isRc)
	   {
	   strand = '-';
	   reverseIntRange(&start, &end, bamHeader->target_len[tid]);
	   }
       if (start < 0) start=0;
       fprintf(f, "%s\t%d\t%d\t.\t0\t%c\n", chrom, start, end, strand);
       }
    }
samclose(sf);

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
seed = optionInt("seed", seed);
if (argc != 5)
    usage();
sampleBam(argv[1],argv[2], argv[3], argv[4]);
return 0;
}

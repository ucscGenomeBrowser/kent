/* cdwFastqPolyFilter - Filter out runs of poly-A from a fastq file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fq.h"

int minSize = 4;  // minimum size of read to leave
int minTrim = 16; // demand at least this many in a row to trim

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFastqPolyFilter - Filter out runs of poly-A from a fastq file.\n"
  "usage:\n"
  "   cdwFastqPolyFilter in.fastq out.fastq\n"
  "in.fastq can be gzipped\n"
  "options:\n"
  "   minSize=%d - the minimum size of read after trimming so as not to make zero length reads\n"
  "   minTrim=%d - the minimum size of polyA run to trim.  Short ones may just be part of genome\n"
  , minSize, minTrim
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"minSize", OPTION_INT},
   {"minTrim", OPTION_INT},
   {NULL, 0},
};

void trim(struct fq *fq, int minSize, int minTrim, char target)
/* Trim fq leaving at least minSize bases. */
{
char *dna = fq->dna;
int size = strlen(dna);
int pos;

for (pos = size-1; pos >= minSize; --pos)
    {
     if (dna[pos] != target)
          {
	  break;
	  }
     }
int firstTrimmed = pos+1;
int trimSize = size - firstTrimmed;
if (trimSize > minTrim)
     {
     dna[firstTrimmed] = 0;
     fq->quality[firstTrimmed] = 0;
     uglyf("size %d, firstTrimmed %d, trimSize %d\n", size, firstTrimmed, trimSize);
     }
}

void cdwFastqPolyFilter(char *inName, char *outName)
/* cdwFastqPolyFilter - Filter out runs of poly-A and poly-T from a fastq file.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct fq *fq;

while ((fq = fqReadNext(lf)) != NULL)
    {
    trim(fq, minSize, minTrim, 'A');
    trim(fq, minSize, minTrim, 'T');
    fqWriteNext(fq, f);
    fqFree(&fq);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
minSize = optionInt("minSize", minSize);
minTrim = optionInt("minTrim", minTrim);
cdwFastqPolyFilter(argv[1], argv[2]);
return 0;
}

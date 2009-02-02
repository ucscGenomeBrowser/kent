/* wigToBigWig - Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigWig.h"
#include "bwgInternal.h"

static char const rcsid[] = "$Id: wigToBigWig.c,v 1.3 2009/02/02 06:05:52 kent Exp $";

int blockSize = 1024;
int itemsPerSlot = 512;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigToBigWig - Convert ascii format wig file (in fixedStep, variableStep or bedGraph\n"
  "format) to binary big wig format.\n"
  "usage:\n"
  "   wigToBigWig in.wig chrom.sizes out.bw\n"
  "Where in.wig is in one of the ascii wiggle formats, but not including track lines\n"
  "and chrom.sizes is two column: <chromosome name> <size in bases>\n"
  "and out.bw is the output indexed big wig file.\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {NULL, 0},
};

void wigToBigWig(char *inName, char *chromSizes, char *outName)
/* wigToBigWig - Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format.. */
{
bigWigFileCreate(inName, chromSizes, blockSize, itemsPerSlot, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
if (argc != 4)
    usage();
wigToBigWig(argv[1], argv[2], argv[3]);
return 0;
}

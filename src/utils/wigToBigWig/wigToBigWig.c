/* wigToBigWig - Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format.. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigWig.h"
#include "bwgInternal.h"
#include "zlibFace.h"

static char const rcsid[] = "$Id: wigToBigWig.c,v 1.9 2009/11/16 18:12:04 kent Exp $";

static int blockSize = 256;
static int itemsPerSlot = 1024;
static boolean clipDontDie = FALSE;
static boolean doCompress = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wigToBigWig v %d - Convert ascii format wig file (in fixedStep, variableStep\n"
  "or bedGraph format) to binary big wig format.\n"
  "usage:\n"
  "   wigToBigWig in.wig chrom.sizes out.bw\n"
  "Where in.wig is in one of the ascii wiggle formats, but not including track lines\n"
  "and chrom.sizes is two column: <chromosome name> <size in bases>\n"
  "and out.bw is the output indexed big wig file.\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  "   -clip - If set just issue warning messages rather than dying if wig\n"
  "                  file contains items off end of chromosome.\n"
  "   -unc - If set, do not use compression."
  , bbiCurrentVersion, blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {"clip", OPTION_BOOLEAN},
   {"unc", OPTION_BOOLEAN},
   {NULL, 0},
};

void wigToBigWig(char *inName, char *chromSizes, char *outName)
/* wigToBigWig - Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format.. */
{
bigWigFileCreate(inName, chromSizes, blockSize, itemsPerSlot, clipDontDie, doCompress, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
clipDontDie = optionExists("clip");
doCompress = !optionExists("unc");
if (argc != 4)
    usage();
wigToBigWig(argv[1], argv[2], argv[3]);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}

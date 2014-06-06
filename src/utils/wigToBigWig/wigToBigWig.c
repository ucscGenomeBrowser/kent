/* wigToBigWig - Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigWig.h"
#include "bwgInternal.h"
#include "zlibFace.h"


static int blockSize = 256;
static int itemsPerSlot = 1024;
static boolean clipDontDie = FALSE;
static boolean doCompress = FALSE;
static boolean fixedSummaries = FALSE;
static boolean keepAllChromosomes = FALSE;

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
  "Use the script: fetchChromSizes to obtain the actual chrom.sizes information\n"
  "from UCSC, please do not make up a chrom sizes from your own information.\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  "   -clip - If set just issue warning messages rather than dying if wig\n"
  "                  file contains items off end of chromosome.\n"
  "   -unc - If set, do not use compression.\n"
  "   -fixedSummaries - If set, use a predefined sequence of summary levels.\n"
  "   -keepAllChromosomes - If set, store all chromosomes in b-tree."
  , bbiCurrentVersion, blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {"clip", OPTION_BOOLEAN},
   {"unc", OPTION_BOOLEAN},
   {"fixedSummaries", OPTION_BOOLEAN},
   {"keepAllChromosomes", OPTION_BOOLEAN},
   {NULL, 0},
};

void wigToBigWig(char *inName, char *chromSizes, char *outName)
/* wigToBigWig - Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format.. */
{
bigWigFileCreateEx(inName, chromSizes, blockSize, itemsPerSlot, clipDontDie, doCompress, keepAllChromosomes, fixedSummaries, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
clipDontDie = optionExists("clip");
doCompress = !optionExists("unc");
keepAllChromosomes = optionExists("keepAllChromosomes");
fixedSummaries = optionExists("fixedSummaries");
if (argc != 4)
    usage();
wigToBigWig(argv[1], argv[2], argv[3]);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}

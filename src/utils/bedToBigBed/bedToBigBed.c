/* bedToBigBed - Convert bed file to bigBed.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigBed.h"

static char const rcsid[] = "$Id: bedToBigBed.c,v 1.2 2009/02/10 22:11:00 kent Exp $";

int blockSize = 1024;
int itemsPerSlot = 64;
int bedFields = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedToBigBed - Convert bed file to bigBed.\n"
  "usage:\n"
  "   bedToBigBed in.bed chrom.sizes out.bb\n"
  "Where in.bed is in one of the ascii bed formats, but not including track lines\n"
  "and chrom.sizes is two column: <chromosome name> <size in bases>\n"
  "and out.bb is the output indexed big bed file.\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  "   -bedFields=N - Number of fields that fit standard bed definition.  If undefined\n"
  "                  assumes all fields in bed are defined.\n"
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {"bedFields", OPTION_INT},
   {NULL, 0},
};

void bedToBigBed(char *inName, char *chromSizes, char *outName)
/* bedToBigBed - Convert bed file to bigBed.. */
{
bigBedFileCreate(inName, chromSizes, blockSize, itemsPerSlot, bedFields, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
bedFields = optionInt("bedFields", bedFields);
if (argc != 4)
    usage();
bedToBigBed(argv[1], argv[2], argv[3]);
return 0;
}

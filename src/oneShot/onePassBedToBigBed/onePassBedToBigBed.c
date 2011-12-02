/* bedToBigBed - Convert bed file to bigBed.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigBed.h"


int blockSize = 1024;
int itemsPerSlot = 256;
int bedFields = 0;
char *as = NULL;

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
  "If the input file is not sorted then this will sort the file in memory,\n"
  "so it will run slightly faster if the input file is already sorted.\n"
  "\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  "   -bedFields=N - Number of fields that fit standard bed definition.  If undefined\n"
  "                  assumes all fields in bed are defined.\n"
  "   -as=fields.as - If have non-standard fields, it's great to put a definition of\n"
  "                   each field in a row in AutoSql format here.\n"
  "   -clip - If set just issue warning messages rather than dying if wig\n"
  "                  file contains items off end of chromosome."
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {"bedFields", OPTION_INT},
   {"as", OPTION_STRING},
   {"clip", OPTION_BOOLEAN},
   {NULL, 0},
};

void bedToBigBed(char *inName, char *chromSizes, char *outName)
/* bedToBigBed - Convert bed file to bigBed.. */
{
bigBedFileCreate(inName, chromSizes, blockSize, itemsPerSlot, bedFields, as, 
	optionExists("clip"), outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
bedFields = optionInt("bedFields", bedFields);
as = optionVal("as", as);
if (argc != 4)
    usage();
bedToBigBed(argv[1], argv[2], argv[3]);
return 0;
}

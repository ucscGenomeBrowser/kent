/* bedToBigBed - Convert bed file to bigBed.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigBed.h"

static char const rcsid[] = "$Id: bedToBigBed.c,v 1.4 2009/04/20 23:17:26 mikep Exp $";

int blockSize = 1024;
int itemsPerSlot = 64;
int bedFields = 0;
char *as = NULL;
boolean sorted = FALSE;

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
  "   -as=fields.as - If have non-standard fields, it's great to put a definition of\n"
  "                   each field in a row in AutoSql format here.\n"
  "   -sorted       - Input is already sorted (quicker for very large files)\n"
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {"bedFields", OPTION_INT},
   {"as", OPTION_STRING},
   {"sorted", OPTION_BOOLEAN},
   {NULL, 0},
};

void bedToBigBed(char *inName, char *chromSizes, char *outName)
/* bedToBigBed - Convert bed file to bigBed.. */
{
bigBedFileCreateDetailed(inName, sorted, chromSizes, blockSize, itemsPerSlot, bedFields, as, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
bedFields = optionInt("bedFields", bedFields);
sorted = optionExists("sorted");
as = optionVal("as", as);
if (argc != 4)
    usage();
bedToBigBed(argv[1], argv[2], argv[3]);
return 0;
}

/* hicInfo - Retrieve and display header information for a .hic file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hic.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hicInfo - Retrieve and display header information for a .hic file.  Uses UDC for remote files.\n"
  "usage:\n"
  "   hicInfo file.hic\n"
  "options:\n"
  "   -attrs - write out the attribute dictionary for the file (might be large)\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"attrs", OPTION_BOOLEAN},
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void hicInfo(char *filename)
/* hicInfo - Retrieve header information for a .hic file. */
{
int i;
struct hicMeta *fileInfo = NULL;
char *errMsg = hicLoadHeader(filename, &fileInfo, NULL);
if(errMsg != NULL)
    errAbort("%s", errMsg);
printf("File: %s\n", filename);
printf("File Assembly (according to the file): %s\n", fileInfo->fileAssembly);
if (optionExists("attrs"))
    {
    printf("\nAttribute count: %d\n", fileInfo->nAttributes/2);
    printf("Attribute key/value pairs:\n");
    for (i=0; i<fileInfo->nAttributes-1; i+=2)
        {
        printf("Key #%d: %s\n", i/2+1, fileInfo->attributes[i]);
        printf("Value #%d: %s\n", i/2+1, fileInfo->attributes[i+1]);
        }
    }
// Skip the first chromosome in this listing, as it's always a composite
// called "All" that's just used for low-resolution display of the entire data set.
printf("\nChromosome count: %d\n", fileInfo->nChroms-1);
printf("Chromosome names/sizes:\n");
for (i=1; i<fileInfo->nChroms; i++)
    {
    printf("%s\t%d\n", fileInfo->chromNames[i], fileInfo->chromSizes[i]);
    }
printf("\nBP Resolution count: %d\n", fileInfo->nRes);
printf("BP Resolutions:\n");
for (i=0; i<fileInfo->nRes; i++)
    {
    printf("%s\n", fileInfo->resolutions[i]);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
hicInfo(argv[1]);
return 0;
}

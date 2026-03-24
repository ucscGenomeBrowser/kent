/* bigMafToMaf - convert bigMaf to maf file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "bigBed.h"
#include "basicBed.h"
#include "options.h"
#include "udc.h"
#include "hdb.h"
#include "bigBedCmdSupport.h"
#include <limits.h>

char *clChrom = NULL;
long long clStart = 0;
long long clEnd = 0;
char *clBed = NULL;
char *clPos = NULL;
struct slName *clRange = NULL;
struct hash *chromHash = NULL;
boolean skipChromCheck = FALSE;

struct lm *lm = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigMafToMaf - convert bigMaf to maf file\n"
  "usage:\n"
  "   bigMafToMaf file.bigMaf file.maf\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  "   -start=N - if set, restrict output to only that over start 0 based coordinate\n"
  "   -end=N - if set, restrict output to only that under end\n"
  "   -range - restrict output to the given genomic range.\n"
  "            Two formats are accepted:\n"
  "            -range=\"chrom start end\"  - 0-based half-open coordinates (BED format)\n"
  "            -range=chrom:start-end  - 1-based start position (colon/hyphen-separated)\n"
  "            May be specified multiple times for multiple ranges.\n"
  "            Do not combine -range with -chrom, -start, or -end.\n"
  "   -bed=in.bed - restrict output to all regions in a BED file\n"
  "   -positions=in.pos - restrict output to all regions in a position file with 1-based start\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  "   -skipChromCheck - skip chrom name validation and coordinate check for chrom size.\n"
  );
}

/* chunking keeps memory down */
static int chunkSizeBases = 1048576;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_LONG_LONG},
   {"end", OPTION_LONG_LONG},
   {"bed", OPTION_STRING},
   {"range", OPTION_STRING|OPTION_MULTI},
   {"positions", OPTION_STRING},
   {"udcDir", OPTION_STRING},
   {"skipChromCheck", OPTION_BOOLEAN},
   {NULL, 0},
};

static void writeMafAli(char *ptr, FILE *f)
/* output one block, change ';'  back to newline */
{
for(; *ptr; ptr++)
    {
    if (*ptr == ';')
        fputc('\n', f);
    else
        fputc(*ptr, f);
    }
fputc('\n', f);

}

static void processChromChunk(struct bbiFile *bbi, char *chrom,
                              uint start, uint end, char *bedName, FILE *f)
/* Output one chunk.  Only blocks where start is in the range will be written
 * to avoid outputting a block multiple times.  */
{
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom,
                                                    start, end, 0, lm);
for(; bbList; bbList = bbList->next)
    {
    if ((start <= bbList->start) && (bbList->start < end))
        writeMafAli(bbList->rest, f);
    }
}

void processChrom(struct bbiFile *bbi, struct bbiChromInfo *chrom,
                         FILE *f)
/* output MAF blocks from one chrom */
{
uint start = 0, end = chrom->size;
if (optionExists("start"))
    {
    start = clStart;
    if (!skipChromCheck)
	{
	if (start > chrom->size)
	    errAbort("invalid start=%u > chromSize=%u", start, chrom->size);
	}
    }
if (optionExists("end"))
    {
    end = clEnd;
    if (!skipChromCheck)
	{
	if (end > chrom->size)
	    errAbort("invalid end=%u > chromSize=%u", end, chrom->size);
	}
    }
if (start > end)
    errAbort("invalid range, start=%u > end=%u", start, end);
while (start < end)
    {
    uint chunkEnd = min(start + chunkSizeBases, end);
    processChromChunk(bbi, chrom->name, start, chunkEnd, NULL, f);
    start = chunkEnd;
    }
}

static void bigMafToMaf(char *bigBed, char *mafFile)
/* bigMafToMaf - convert bigMaf to maf file. */
{
struct bbiFile *bbi = bigBedFileOpen(bigBed);
FILE *f = mustOpen(mafFile, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
if (!skipChromCheck)
    chromHash = makeChromHash(chromList);
fprintf(f, "##maf version=1\n");

if (clBed != NULL)
    {
    genericBigToNonBigFromBed(bbi, chromHash, clBed, f, &processChromChunk);
    }
else if (clPos != NULL)
    {
    genericBigToNonBigFromPos(bbi, chromHash, clPos, f, &processChromChunk);
    }
else if (clRange != NULL)
    {
    genericBigToNonBigFromRange(bbi, chromHash, f, clRange, &processChromChunk);
    }
else
    {
    boolean chromFound = FALSE;
    for (chrom = chromList; chrom != NULL; chrom = chrom->next)
	{
	if (clChrom != NULL && !sameString(clChrom, chrom->name))
	    continue;
	chromFound = TRUE;
	processChrom(bbi, chrom, f);
	}
    if (clChrom && !chromFound && !skipChromCheck)
	errAbort("specified chrom %s not found in bigMaf", clChrom);
    }
bbiChromInfoFreeList(&chromList);
carefulClose(&f);
bbiFileClose(&bbi);
}

int main(int argc, char *argv[])
/* Process command line. */
{
lm = lmInit(0);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
clChrom = optionVal("chrom", clChrom);
clStart = optionLongLong("start", clStart);
if (clStart > UINT_MAX)
    errAbort("-start option too big. Should not exceed %u - the size of unsigned integer.", UINT_MAX);
if (clStart < 0)
    errAbort("-start option should not be less than 0.");

clEnd = optionLongLong("end", clEnd);
if (clEnd > UINT_MAX)
    errAbort("-end option too big. Should not exceed %u - the size of an unsigned integer.", UINT_MAX);
if (clEnd < 0)
    errAbort("-end option should not be less than 0.");

clBed = optionVal("bed", clBed);
clRange = optionMultiVal("range", clRange);
clPos = optionVal("positions", clPos);
skipChromCheck = optionExists("skipChromCheck");

if ((clBed || clPos || clRange) && (clChrom || optionExists("start") || optionExists("end")))
    errAbort("-bed or -positions or -range can not be used with -chrom -start or -end options");
if ((clBed && clPos) || (clBed && clRange) || (clPos && clRange))
    errAbort("-bed, -positions, and -range can not be used together");

bigMafToMaf(argv[1], argv[2]);

lmCleanup(&lm);
return 0;
}

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

char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;
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
  "   -range=\"chrom start end\" - if set, restrict output to only that within range from start to end. \n"
  "          This range start is a half-open 0-based coordinate like used in BED files. \n"
  "   -range=chrom:start-end - if set, restrict output to only that within range from start to end. \n"
  "          This range start is a 1-based start position. \n"
  "    Do not use range with chrom, start, and/or end options. \n"
  "   -range may be specified multiple times for multiple ranges. \n"
  "   -bed=in.bed - restrict output to all regions in a BED file\n"
  "   -positions=in.pos - restrict output to all regions in a position file with 1-based start\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  "   -skipChromCheck - skip checking chrom name.\n"
  );
}

/* chunking keeps memory down */
static int chunkSizeBases = 1048576;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
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
                              int start, int end, char *bedName, FILE *f)
/* Output one chunk.  Only blocks where start is in the range will be written
 * to avoid outputting a block multiple tines.  */
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
int start = 0, end = chrom->size;
if (clStart >= 0)
    {
    start = clStart;
    }
if (clEnd >= 0)
    {
    end = clEnd;
    if (end > chrom->size)
	end = chrom->size;
    }
if (start > end)
    errAbort("invalid range, start=%d > end=%d", start, end);
while (start < end)
    {
    int chunkEnd = min(start + chunkSizeBases, end);
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
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
clBed = optionVal("bed", clBed);
clRange =optionMultiVal("range", clRange);
clPos = optionVal("positions", clPos);
skipChromCheck = optionExists("skipChromCheck");

if ((clBed || clPos || clRange) && (clChrom || (clStart >= 0) || (clEnd >= 0)))
    errAbort("-bed or -positions or -range can not be used with -chrom -start or -end options");
if ((clBed && clPos) || (clBed && clRange) || (clPos && clRange))
    errAbort("-bed, -positions, and -range can not be used together");

bigMafToMaf(argv[1], argv[2]);

lmCleanup(&lm);
return 0;
}

/* bigMafToMaf - convert bigMaf to maf file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "bigBed.h"
#include "basicBed.h"
#include "options.h"
#include "udc.h"
#include "hdb.h"

char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;
char *clBed = NULL;
char *clRange = NULL;
char *clPos = NULL;

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
  "   -range=start-end - if set, restrict output to only that within range from start to end. \n"
  "          (NOTE the range start is a half-open 0-based coordinate like used in BED files). \n"
  "          (Do not use range with chrom, start, and/or end options). \n"
  "   -bed=in.bed - restrict output to all regions in a BED file\n"
  "   -positions=in.pos - restrict output to all regions in a position file with 1-based start\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
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
   {"positions", OPTION_STRING},
   {"udcDir", OPTION_STRING},
   {"range", OPTION_STRING},
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
                              int start, int end, FILE *f)
/* Output one chunk.  Only blocks where start is in the range will be written
 * to avoid outputting a block multiple tines.  */
{
struct lm *lm = lmInit(0);
struct bigBedInterval *bbList = bigBedIntervalQuery(bbi, chrom,
                                                    start, end, 0, lm);
for(; bbList; bbList = bbList->next)
    {
    if ((start <= bbList->start) && (bbList->start < end)) 
        writeMafAli(bbList->rest, f);
    }
lmCleanup(&lm);
}

static void bigMafToMafFromBed(struct bbiFile *bbi, char *bedFileName, FILE *outFile)
{
struct bed *bed, *bedList = bedLoadNAll(bedFileName, 3);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->chromStart > bed->chromEnd)
	errAbort("invalid range, start=%u > end=%u", bed->chromStart, bed->chromEnd);
    processChromChunk(bbi, bed->chrom, bed->chromStart, bed->chromEnd, outFile);
    }
}

static void bigMafToMafFromPos(struct bbiFile *bbi, char *posFileName, FILE *outFile)
{
/* Use  positions from file (chrom:start-end). */
struct lineFile *lf = lineFileOpen(posFileName, TRUE);
char *line;
char *chrom;
uint start, end;

/* Convert input to bed file */
while (lineFileNextReal(lf, &line))
    {
    if (parsePosition(line, &chrom, &start, &end))
	{
	if (start > end)
	    errAbort("invalid range, (start - 1)=%u > end=%u", start, end);
	processChromChunk(bbi, chrom, start, end, outFile);
        }
    else
	{
	errAbort("line %s has invalid position", line);
	}
    }
lineFileClose(&lf);
}


static void processChrom(struct bbiFile *bbi, struct bbiChromInfo *chrom,
                         FILE *f)
/* output MAF blocks from one chrom */
{
int start = 0, end = chrom->size;
if (clStart >= 0)
    {
    start = clStart;
    if (start < 0)
	start = 0;
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
    processChromChunk(bbi, chrom->name, start, chunkEnd, f);
    start = chunkEnd;
    }
}

static void bigMafToMaf(char *bigBed, char *mafFile)
/* bigMafToMaf - convert bigMaf to maf file. */
{
struct bbiFile *bbi = bigBedFileOpen(bigBed);
FILE *f = mustOpen(mafFile, "w");
fprintf(f, "##maf version=1\n");

if (clBed != NULL)
    {
    bigMafToMafFromBed(bbi, clBed, f);
    return;
    }
if (clPos != NULL)
    {
    bigMafToMafFromPos(bbi, clPos, f);
    return;
    }
if (clRange)
    {
    mustParseRange(clRange, &clChrom, &clStart, &clEnd);
    }
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
boolean chromFound = FALSE;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (clChrom != NULL && !sameString(clChrom, chrom->name))
        continue;
    chromFound = TRUE;
    processChrom(bbi, chrom, f);
    }
if (clChrom && !chromFound)
    warn("specified chrom %s not found in maf", clChrom);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
clChrom = optionVal("chrom", clChrom);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
clRange = optionVal("range", clRange);
if (clRange && (optionExists("chrom") || optionExists("start") || optionExists("end")))
    usage();
clBed = optionVal("bed", clBed);
clPos = optionVal("positions", clPos);
bigMafToMaf(argv[1], argv[2]);
return 0;
}

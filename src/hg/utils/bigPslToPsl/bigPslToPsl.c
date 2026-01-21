/* bigPslToPsl - convert bigPsl file to psl. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "bigBed.h"
#include "psl.h"
#include "bigPsl.h"
#include "basicBed.h"
#include "bigBedCmdSupport.h"

char *clChrom = NULL;
int clStart = -1;
int clEnd = -1;
char *clBed = NULL;
char *clPos = NULL;

int seqTypeField = 0;

struct lm *lm = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigPslToPsl - convert bigPsl file to psl\n"
  "usage:\n"
  "   bigPslToPsl bigPsl.bb output.psl\n"
  "options:\n"
  "   -chrom=chr1 - if set restrict output to given chromosome\n"
  "   -start=N - if set, restrict output to only that over start\n"
  "   -end=N - if set, restict output to only that under end\n"
  "   -bed=in.bed - restrict output to all regions in a BED file\n"
  "   -positions=in.pos - restrict output to all regions in a position file with 1-based start\n"
  "   -collapseStrand   if target strand is '+', don't output it\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

boolean collapseStrand = FALSE;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"bed", OPTION_STRING},
   {"positions", OPTION_STRING},
   {"collapseStrand", OPTION_BOOLEAN},
   {"udcDir", OPTION_STRING},
    {NULL, 0},
};

static void processChromChunk(struct bbiFile *bbi, char *chrom,
                              int start, int end, char *bedName, FILE *f)
/* Output one chunk.  Only blocks where start is in the range will be written
 * to avoid outputting a block multiple tines.  */
{
int itemsLeft = 0;
char *chromName = chrom;

struct bigBedInterval  *bbList = bigBedIntervalQuery(bbi, chromName,
	start, end, itemsLeft, lm);

for(; bbList; bbList = bbList->next)
    {
    struct psl *psl, *pslList = pslFromBigPsl(chromName, bbList, seqTypeField, NULL, NULL);

    for(psl=pslList; psl; psl = psl->next)
	{
	if (collapseStrand && (psl->strand[1] == '+'))
	    psl->strand[1] = 0;
	pslTabOut(psl, f);
	}
    }

}


void bigPslToPsl(char *bigPslName, char *outputName)
/* bigPslToPsl - convert bigPsl file to psle. */
{
struct bbiFile *bbi = bigBedFileOpen(bigPslName);
FILE *f = mustOpen(outputName, "w");
seqTypeField =  bbExtraFieldIndex(bbi, "seqType");
if (clBed != NULL)
    {
    genericBigToNonBigFromBed(bbi, clBed, f, &processChromChunk);
    return;
    }
if (clPos != NULL)
    {
    genericBigToNonBigFromPos(bbi, clPos, f, &processChromChunk);
    return;
    }
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {

    if (clChrom != NULL && !sameString(clChrom, chrom->name))
        continue;

    int start = 0, end = chrom->size;
    if (clStart >= 0)
        start = clStart;
    if (clEnd >= 0)
	{
        end = clEnd;
	if (end > chrom->size)
	    end = chrom->size;
	}
    if (start > end)
	errAbort("invalid range, start=%d > end=%d", start, end);

    processChromChunk(bbi, chrom->name, start, end, NULL, f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
lm = lmInit(0);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clChrom = optionVal("chrom", clChrom);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
clBed = optionVal("bed", clBed);
clPos = optionVal("positions", clPos);
collapseStrand = optionExists("collapseStrand");
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

if (argc != 3)
    usage();

if ((clBed || clPos) && (clChrom || (clStart >= 0) || (clEnd >= 0)))
    errAbort("-bed or -positions can not be used with -chrom -start or -end options");
if (clBed && clPos)
    errAbort("-bed and -positions can not be used together");

bigPslToPsl(argv[1], argv[2]);

lmCleanup(&lm);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}

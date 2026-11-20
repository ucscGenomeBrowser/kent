/* bbiZoomCapTest - read the first zoom level of bigBed and bigWig files whose zoom
 * index has a bad size for its last block.  bedToBigBed and bedGraphToBigWig wrote such
 * files when they were over 2 GB.  bbiSummariesInRegion() must cap the block at the
 * start of the zoom index and return the same summaries as for the undamaged file.
 * The input files are described in input/bbiZoomCap/README.  refs #38819 */

#include "common.h"
#include "errCatch.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "bigWig.h"

static void readZoomOne(char *fileName)
/* Print the first zoom level's summaries over the whole first chromosome, or the error. */
{
struct bbiFile *bbi = (endsWith(fileName, ".bb") ? bigBedFileOpen(fileName)
                                                 : bigWigFileOpen(fileName));
struct bbiChromInfo *chrom = bbiChromList(bbi);
struct bbiZoomLevel *zoom = bbi->levelList;
printf("%s zoom %d:", strrchr(fileName, '/') + 1, zoom->reductionLevel);
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    struct bbiSummary *sumList = bbiSummariesInRegion(zoom, bbi, chrom->id, 0, chrom->size);
    struct bbiSummary *sum;
    bits64 validCount = 0;
    double sumData = 0;
    for (sum = sumList; sum != NULL; sum = sum->next)
        {
        validCount += sum->validCount;
        sumData += sum->sumData;
        }
    printf(" %d summaries, last ends %u, validCount %llu, sumData %g\n",
        slCount(sumList), (sumList ? ((struct bbiSummary *)slLastEl(sumList))->end : 0),
        validCount, sumData);
    slFreeList(&sumList);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    printf(" error: %s\n", trimSpaces(errCatch->message->string));
errCatchFree(&errCatch);
bbiChromInfoFreeList(&chrom);
bbiFileClose(&bbi);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int i;
if (argc < 2)
    errAbort("usage: bbiZoomCapTest file.bb|file.bw ...");
for (i = 1; i < argc; ++i)
    readZoomOne(argv[i]);
return 0;
}

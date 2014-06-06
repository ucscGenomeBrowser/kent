/* edwReplicatedPeaks - Given two peak input files in narrowPeak or broadPeak like format make 
 * an output consisting only of peaks that replicate.  Scores are averaged between replicates as 
 * are peak boundaries. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bigBed.h"
#include "genomeRangeTree.h"
#include "encode/encodePeak.h"

/* This program relies on the genomeRangeTree to build up clumps of overlapping peaks.
 * Most of the time the clumps will be one (for unreplicated) or two (for simply replicated).
 * However there are cases where the two replicates overlap in such a way as to make the
 * clumps larger.   In ascii art below is something that would resolve as a clump of 5:
 *      11111    11111   1111111
 *        2222222222 2222222
 * Note that since these are peak calls there will be no overlap of peaks within a replicate.
 * The program depends on this and verifies it.
 *
 * The logic of the program depends on the size of the clump.   For clumps of 1, it just
 * ignores them.  For clumps of 2 and 3 it outputs a single peak containing the average
 * values of the components, with coordinates chosen to keep the center and the
 * total size of the output peak the same as the average center and average total size of
 * peaks involved in the overlap in both replicates. 
 *
 * For bigger clumps it will procede from left to right outputting the average of the next
 * two peaks until it gets down to just 2 or 3 peaks left in the clump.  It then processes
 * the last 2 or three together.   The above example would thus lead to 2 output peaks composed
 * from the sub-clumps
 *      --x--
 *      11111    
 *        2222222222
 *        ----x-----
 * and
 *               -------x-------
 *               11111   1111111
 *                   2222222
 *                   ---x---
 * These in turn would resolve to (using P for pooled) the following peaks
 *       PPPPPPP    PPPPPPPPP
 * I'm not 100% sure this handles all cases, but it handles the common cases.  The one I
 * worry about is:
 *     11111111111111111111111111111111111111111
 *       22  22  22   22  22  22  22 22  22  22
 * Which is going to lead to something like
 *      PPPPPPPPPPPPPPPPPPPPPP
 *             PP      PP       PP       PP
 * by this logic, which among other things overlaps.... 
 * At the moment because of this we run bedRemoveOverlaps if need be on the result.
 * This would throw out the two smaller overlapping peaks, so you'd have
 *      PPPPPPPPPPPPPPPPPPPPPP
 *                              PP       PP
 * for that merger, which may not be perfect in any sense, but which is probably good enough
 * for a corner case. */

/* Version history:
 * 1 - Initial version
 * 2 - Polished case where two peaks in one replicate overlap one peak in the other
 *     going from taking average start and average end of all 3 peaks to a scheme that
 *     produces one peak with average mass center of mass of the two replicates. */

FILE *bigMerge = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwReplicatedPeaks v2 - Given two peak input files in narrowPeak or broadPeak like format make\n"
  "an output consisting only of peaks that replicate.  Scores are averaged between replicates\n"
  "as are peak boundaries\n"
  "usage:\n"
  "   edwReplicatedPeaks 1.bigBed 2.bigBed output\n"
  "Output will be either narrowPeak or broadPeak depending on the input."
  "You'll still need to run bedToBigBed on output\n"
  "options:\n"
  "   -bigMerge=fileName - print information about cases where more than 2 peaks intersect\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bigMerge", OPTION_STRING},
   {NULL, 0},
};

enum encodePeakType checkIsBbPeak(struct bbiFile *bbi)
/* Check that file is narrowPeak or broadPeak, and abort if not.  Return type of peak. */
{
struct asObject *as = bigBedAs(bbi);
if (as == NULL)
    errAbort("%s has no associated .as file", bbi->fileName);
enum encodePeakType type = invalid;
if (sameString(as->name, "narrowPeak"))
    type = narrowPeak;
else if (sameString(as->name, "broadPeak"))
    type = broadPeak;
else
    errAbort("%s has as %s, expecting broadPeak or narrowPeak", bbi->fileName, as->name);
asObjectFreeList(&as);
return type;
}

boolean bigBedAnyInternalOverlap(struct bbiFile *bbi)
/* Return TRUE if any items in bbi overlap */
{
boolean anyOverlap = FALSE;
struct bbiChromInfo *chrom, *chromList = bbiChromList(bbi);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct lm *lm = lmInit(0);
    struct bigBedInterval *ivList = bigBedIntervalQuery(bbi, chrom->name, 0, chrom->size, 0, lm);
    if (ivList != NULL)
        {
	/* Since ivList comes to us sorted, enough to check no overlap between adjacent items */
	struct bigBedInterval *lastIv = ivList, *iv;
	for (iv = lastIv->next; iv != NULL; iv = iv->next)
	    {
	    if (lastIv->end >= iv->start)
	        {
		anyOverlap = TRUE;
		break;
		}
	    lastIv = iv;
	    }
	}
    lmCleanup(&lm);
    if (anyOverlap)
        break;
    }
bbiChromInfoFreeList(&chromList);
return anyOverlap;
}

void bigBedCheckNoInternalOverlap(struct bbiFile *bbi)
/* Abort if any overlap within file. */
{
if (bigBedAnyInternalOverlap(bbi))
    errAbort("%s has internal overlaps, can't process it", bbi->fileName);
    
}

void grtAddRep(struct genomeRangeTree *grt, struct bbiFile *bb, struct lm *lm)
/* Add items in bb representing given replicate to grt */
{
struct bbiChromInfo *chrom, *chromList = bbiChromList(bb);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct bigBedInterval *ivList = bigBedIntervalQuery(bb, chrom->name, 0, chrom->size, 0, lm);
    struct bigBedInterval *iv, *nextIv;
    for (iv = ivList; iv != NULL; iv = nextIv)
        {
	nextIv = iv->next;
	iv->next = NULL;
	genomeRangeTreeAddValList(grt, chrom->name, iv->start, iv->end, iv);
	}
    }
}

int bigBedIntervalCmpStart(const void *va, const void *vb)
/* Compare two intervals purely based on chromosome start */
{
const struct bigBedInterval *a = *((struct bigBedInterval **)va);
const struct bigBedInterval *b = *((struct bigBedInterval **)vb);
return a->start - b->start;
}


void outputSmallMergedList(FILE *f, char *chrom, struct bigBedInterval *ivList, int count,
    enum encodePeakType type)
/* Average together next count in ivList and output it to f in format specified by type. 
 * This will put zeros in the rest field of ivList as a side effect.  */
{
long long startSum = 0, endSum = 0, sumScore = 0, peakSum = 0;
double sumSignal = 0, sumP = 0, sumQ = 0;
int i = 0;
char *strand = NULL;
char *name = NULL;
struct bigBedInterval *iv;
for (iv = ivList; iv != NULL && i < count; iv = iv->next, ++i)
    {
    /* Sum up starts and ends */
    startSum += iv->start;
    endSum += iv->end;

    /* Parse out rest of fields and check all fields we need are there */
    char *rest = iv->rest;
    name = nextWord(&rest);
    char *scoreString = nextWord(&rest);
    strand = nextWord(&rest);
    char *signalString = nextWord(&rest);
    char *pString = nextWord(&rest);
    char *qString = nextWord(&rest);
    char *peakString = nextWord(&rest);
    if (type == narrowPeak)
        {
	if (peakString == NULL)
	    internalErr();
	}
    else
	{
	if (qString == NULL)
	     internalErr();
	}

    /* Sum up other fields */
    sumScore += sqlSigned(scoreString);
    sumSignal += sqlDouble(signalString);
    sumP += sqlDouble(pString);
    sumQ += sqlDouble(qString);
    if (type == narrowPeak)
        peakSum += sqlSigned(peakString) + iv->start;
    }

/* And now output averaged values in appropriate peak format. */
long long aveSize = (endSum - startSum)/2;
long long midPoint = (startSum + endSum)/(2*count);
long long start = midPoint - aveSize/2;
long long end = start + aveSize;
fprintf(f, "%s\t%lld\t%lld\t", chrom, start, end);
fprintf(f, "%s\t%lld\t%s\t", name, sumScore/count, strand);
double invCount = 1.0/count;
fprintf(f, "%g\t%g\t%g", invCount*sumSignal, invCount * sumP, invCount * sumQ);
if (type == narrowPeak)
     fprintf(f, "\t%lld",  (peakSum - startSum)/count);
fprintf(f, "\n");
}

void outputMergedPeaks(FILE *f, char *chrom, struct bigBedInterval *ivList, 
    enum encodePeakType type)
/* Output replicted merged peaks from pirList. Assumes ivList is sorted by start. */
{
/* Check count.  If not at least two no replicates, so no output */
int count = slCount(ivList);
if (count < 2)
    return;
    
if (bigMerge != NULL && count > 2)
    {
    struct bigBedInterval *iv;
    fprintf(bigMerge, "# %d in clump\n", count);
    for (iv = ivList; iv != NULL; iv = iv->next)
        {
	fprintf(bigMerge, "%s\t%d\t%d\n", chrom, iv->start, iv->end);
	}
    }

while (count > 0)
    {
    int thisCount = count;
    if (thisCount > 3)
        thisCount = 2;
    outputSmallMergedList(f, chrom, ivList, thisCount, type);
    count -= thisCount;
    while (--thisCount >= 0)
        ivList = ivList->next;
    }

}

void edwReplicatedPeaks(char *rep1File, char *rep2File, char *outFile)
/* edwReplicatedPeaks - Given two peak input files in narrowPeak or broadPeak like format make 
 * an output consisting only of peaks that replicate.  Scores are averaged between replicates 
 * as are peak boundaries. */
{
/* Open input and make sure they are peak files of same type. */
struct bbiFile *bb1 = bigBedFileOpen(rep1File);
struct bbiFile *bb2 = bigBedFileOpen(rep2File);
enum encodePeakType type1 = checkIsBbPeak(bb1);
enum encodePeakType type2 = checkIsBbPeak(bb2);
if (type1 != type2)
     errAbort("One file is broadPeak, the other narrowPeak.  They need to be the same.");
verbose(2, "Both files are good bigBed peak files.\n");

/* Make sure neither one has internal overlap. */
bigBedCheckNoInternalOverlap(bb1);
bigBedCheckNoInternalOverlap(bb2);
verbose(2, "No internal overlap, good.\n");

/* Make genomeRangeTree to help with merging and fold in both beds*/
struct genomeRangeTree *grt = genomeRangeTreeNew();
struct lm *lm = grt->lm;
grtAddRep(grt, bb1, lm);
grtAddRep(grt, bb2, lm);
verbose(2, "Made genomeRangeTree ok\n");

/* Go through genomeRangeTree which now contains a bunch of lists of overlapping items. 
 * Do output based on these. */
FILE *f = mustOpen(outFile, "w");
struct bbiChromInfo *chrom, *chromList = bbiChromList(bb1);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    verbose(2, "Doing merge on chrom %s\n", chrom->name);
    struct range *range, *rangeList = genomeRangeTreeList(grt, chrom->name);
    for (range = rangeList; range != NULL; range = range->next)
        {
	slSort(&range->val, bigBedIntervalCmpStart); 
	outputMergedPeaks(f, chrom->name, range->val, type1);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
if (optionExists("bigMerge"))
    bigMerge = mustOpen(optionVal("bigMerge",NULL), "w");
edwReplicatedPeaks(argv[1], argv[2], argv[3]);
carefulClose(&bigMerge);
return 0;
}

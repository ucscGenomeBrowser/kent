/* crisprKmers - find and annotate crispr sequences. */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

/*  Theory of operation:
  a. scan given sequence (2bit or fa or fa.gz file)
  b. record all quide sequences, both positive and negative strands,
     on a linked list structure, 2bit encoding of the A C G T bases,
     with PAM sequence, strand and start coordinates, one linked list
     for each chromosome name.
  c. if a 'ranges' bed3 file is given, then divide up the linked list
     guide sequences into a 'query' list and a 'target' list.
     The 'query' list of guide sequences are those that have any overlap
     with the 'ranges' bed3 items.  The 'target' list is an exclusive
     set of all the other guide sequences.
  d. Without 'ranges', the full list of sequences can be considerd as
     the 'query' sequences.
  e. Convert the linked list structures into memory arrays, get all
     the sequence data and start coordinates into arrays.  This is much
     more efficient to work with the arrays than trying to run through
     the linked lists.  The data happens to become duplicated as it
     is copied, it isn't worth the time to try to free the memory for
     the data from the linked lists.
  f. When working with 'ranges', there are two comparison steps:
     1. compare all the 'query' sequences with all the 'target' sequences,
	recording the off-target information for the 'query' sequences
	in mis-count arrays and writing the off-target information to a
	given file.
     2. compare all 'query' sequences with themselves while avoiding
	direct self to self comparison.  Recording the same off-target
	information as the 'query' vs. 'target' sequences.
  g. Without 'ranges', the complete list, aka 'query' list, is compared
     to itself while avoiding direct self to self comparison.
     Recording same off-target information as when working with 'ranges'
  h. Finish off by printing out a bed9+ file with all the data recorded
     for off-counts for the 'query' sequences.

  Post-processing needs to go through the recorded off-target output,
  to construct final scores for each guide sequence to produce the final
  bed9+ output file.

  It doesn't save time to write out all the scanned guide sequences
  data to be used by a second run.  The original scanning itself is
  faster than reading in all that data.
*/


#include <popcntintrin.h>
#include <pthread.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "portable.h"
#include "basicBed.h"
#include "sqlNum.h"
#include "binRange.h"
#include "obscure.h"
#include "memalloc.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "crisprKmers - annotate crispr guide sequences\n"
  "usage:\n"
  "   crisprKmers <sequence>\n"
  "options:\n"
  "   where <sequence> is a .2bit file or .fa fasta sequence\n"
  "   -verbose=N - for debugging control processing steps with level of verbose:\n"
  "              - verbose < 2 - scan sequence only, no comparisons\n"
  "              - verbose > 1 - run comparisons\n"
  "   -bed=<file> - output results to given bed9+ file\n"
  "   -offTargets=<file> - output off target data to given file\n"
  "   -ranges=<file> - use specified bed3 file to limit which guides are\n"
  "                  - measured, only those with any overlap to these bed items.\n"
  "   -threads=N - N number of threads to run, default: no threading\n"
  "              - use when ku is going to allocate more CPUs for big mem jobs.\n"
  "   -dumpGuides=<file> - after scan of sequence, output guide sequences to file"
  );
}

//  "   -loadKmers=<file> - NOT VALID load kmers from previous scan of sequence from -dumpKmers"

static char *bedFileOut = NULL;	/* set with -bed=<file> argument, kmer output */
static char *offTargets = NULL;	/* write offTargets to <file> */
static FILE *offFile = NULL;	/* file handle to write off targets */
static char *ranges = NULL;	/* use ranges <file> to limit scanning */
static struct hash *rangesHash = NULL;	/* ranges into hash + binkeeper */
static char *dumpGuides = NULL;	/* file name to write out guides from scan */
static char *loadKmers = NULL;	/* file name to read in kmers from previous scan */
static int threads = 0;	/* number of threads to run */
static int threadCount = 1;	/* will be adjusted depending upon vmPeak */

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bed", OPTION_STRING},
   {"offTargets", OPTION_STRING},
   {"ranges", OPTION_STRING},
   {"threads", OPTION_INT},
   {"dumpGuides", OPTION_STRING},
   {"loadKmers", OPTION_STRING},
   {NULL, 0},
};

#define	guideSize	20	// 20 bases
#define	pamSize		3	//  3 bases
#define negativeStrand	0x0000800000000000
#define duplicateGuide	0x0001000000000000

#define fortyEightBits	0xffffffffffff
#define fortySixBits	0x3fffffffffff
#define fortyBits	0xffffffffff
#define high32bits	0xffffffff00000000
#define low32bits	0x00000000ffffffff
#define high16bits	0xffff0000ffff0000
#define low16bits	0x0000ffff0000ffff
#define high8bits	0xff00ff00ff00ff00
#define low8bits	0x00ff00ff00ff00ff
#define high4bits	0xf0f0f0f0f0f0f0f0
#define low4bits	0x0f0f0f0f0f0f0f0f
#define high2bits	0xcccccccccccccccc
#define low2bits	0x3333333333333333

#define pamMask 0x3f

//  0x0000 0000 0000 0000
//    6348 4732 3116 15-0

// sequence word structure:
// bits 5-0 - PAM sequence in 2bit encoding for 3 bases
// bits 45-6 - 20 base sequence in 2bit encoding format
// bit 47 - negative strand indication
// bit 48 - duplicate guide indication
// considering using other bits during processing to mark
// an item for no more consideration

// sizeof(struct crispr): 32
struct crispr
/* one chromosome set of guides */
    {
    struct crispr *next;		/* Next in list. */
    long long sequence;	/* sequence value in 2bit format */
    long long start;		/* chromosome start 0-relative */
    };

// sizeof(struct crisprList): 72
struct crisprList
/* all chromosome sets of guides */
    {
    struct crisprList *next;	/* Next in list. */
    char *chrom;		/* chrom name */
    int size;			/* chrom size */
    struct crispr *chromCrisprs;	/* all the guides on this chrom */
    long long crisprCount;	/* number of guides on this chrom */
    long long *sequence;	/* array of the sequences */
    long long *start;		/* array of the starts */
    int **offBy;		/* offBy[5][n] */
    float *mitSum;		/* accumulating sum of MIT scores */
    };

struct threadControl
/* data passed to thread to control execution */
    {
    int threadId;	/* this thread Id */
    int threadCount;	/* total threads running */
    struct crisprList *query;	/* running query guides against */
    struct crisprList *target;	/* target guides */
    };

struct loopControl
/* calculate index for start to < end processing for multiple threads */
    {
    long long listStart;
    long long listEnd;
    };

/* base values here are different than standard '2bit' format
 * these are in order numerically and alphabetically
 * plus they complement to their complement with an XOR with 0x3
 */
#define A_BASE	0
#define C_BASE	1
#define G_BASE	2
#define T_BASE	3
#define U_BASE	3
#define endsGG	((G_BASE << 2) | G_BASE)
#define endsAG	((A_BASE << 2) | G_BASE)
#define beginsCT	((long long)((C_BASE << 2) | T_BASE) << 42)
#define beginsCC	((long long)((C_BASE << 2) | C_BASE) << 42)

static int orderedNtVal[256];	/* values in alpha order: ACGT 00 01 10 11 */
				/* for easier sorting and complementing */
static char bases[4];  /* for binary to ascii conversion */

static void initOrderedNtVal()
/* initialization of base value lookup arrays */
{
int i;
for (i=0; i<ArraySize(orderedNtVal); i++)
    orderedNtVal[i] = -1;
orderedNtVal['A'] = orderedNtVal['a'] = A_BASE;
orderedNtVal['C'] = orderedNtVal['c'] = C_BASE;
orderedNtVal['G'] = orderedNtVal['g'] = G_BASE;
orderedNtVal['T'] = orderedNtVal['t'] = T_BASE;
orderedNtVal['U'] = orderedNtVal['u'] = T_BASE;
bases[A_BASE] = 'A';
bases[C_BASE] = 'C';
bases[G_BASE] = 'G';
bases[T_BASE] = 'T';
}	//	static void initOrderedNtVal()

static void timingMessage(char *prefix, long long count, char *message,
    long startMs, char *units, char *invUnits)
{
long elapsedMs = clock1000() - startMs;
double perSecond = 0.0;
double inverse = 0.0;
if ((elapsedMs > 0) && (count > 0))
    {
    perSecond = 1000.0 * count / elapsedMs;
    inverse = 1.0 / perSecond;
    }

verbose(1, "# %s: %lld %s @ %ld ms\n#\t%.2f %s == %g %s\n", prefix, count,
    message, elapsedMs, perSecond, units, inverse, invUnits);
}

static void setLoopEnds(struct loopControl *control, long long listSize,
    int partCount, int partNumber)
/* for multiple thread processing, given a list of listSize,
 #    a partCount and a partNumber
 *    return listStart, listEnd indexes in control structure
 */
{
long long partSize = 1 + listSize / partCount;
long long listStart = partSize * partNumber;
long long listEnd = partSize * (partNumber + 1);
if (listEnd > listSize)
    listEnd = listSize;
control->listStart = listStart;
control->listEnd = listEnd;
}

static struct hash *readRanges(char *bedFile)
/* Read bed and return it as a hash keyed by chromName
 * with binKeeper values.  (from: src/hg/bedIntersect/bedIntersec.c) */
{
struct hash *hash = newHash(0);	/* key is chromName, value is binkeeper data */
struct lineFile *lf = lineFileOpen(bedFile, TRUE);
char *row[3];

verbose(1, "# using ranges file: %s\n", bedFile);

while (lineFileNextRow(lf, row, 3))
    {
    struct binKeeper *bk;
    struct bed3 *item;
    struct hashEl *hel = hashLookup(hash, row[0]);
    if (hel == NULL)
       {
       bk = binKeeperNew(0, 1024*1024*1024);
       hel = hashAdd(hash, row[0], bk);
       }
    bk = hel->val;
    AllocVar (item);
    item->chrom = hel->name;
    item->chromStart = lineFileNeedNum(lf, row, 1);
    item->chromEnd = lineFileNeedNum(lf, row, 2);
    binKeeperAdd(bk, item->chromStart, item->chromEnd, item);
    }
lineFileClose(&lf);
return hash;
}	//	static struct hash *readRanges(char *bedFile)

#ifdef NOT
// Do not need this, slReverse does this job, perhaps a bit more efficiently
static int slCmpStart(const void *va, const void *vb)
/* Compare slPairs on start value */
{
const struct crispr *a = *((struct crispr **)va);
const struct crispr *b = *((struct crispr **)vb);
long long aVal = a->start;
long long bVal = b->start;
if (aVal < bVal)
    return -1;
else if (aVal > bVal)
    return 1;
else
    return 0;
}
#endif

/* the kmerPAMString and kmerValToString functions could be reduced to
 * one single function, and a lookup table might make this faster.
 */
static void kmerPAMString(char *stringReturn, long long val)
/* return the ASCII string for last three bases in then binary sequence value */
{
long long twoBitMask = 0x30;
int shiftCount = 4;
int baseCount = 0;

while (twoBitMask)
    {
    int base = (val & twoBitMask) >> shiftCount;
    stringReturn[baseCount++] = bases[base];
    twoBitMask >>= 2;
    shiftCount -= 2;
    }
stringReturn[baseCount] = 0;
}	//	static char *kmerPAMString(struct crispr *c, int trim)

static void kmerValToString(char *stringReturn, long long val, int trim)
/* return ASCII string for binary sequence value */
{
long long twoBitMask = 0x300000000000;
int shiftCount = 44;
int baseCount = 0;

while (twoBitMask && (shiftCount >= (2*trim)))
    {
    int base = (val & twoBitMask) >> shiftCount;
    stringReturn[baseCount++] = bases[base];
    twoBitMask >>= 2;
    shiftCount -= 2;
    }
stringReturn[baseCount] = 0;
}	//	static void kmerValToString(char *stringReturn, long long val, int trim)

/* this revComp is only used in one place, perfectly fine as an inline
 * function, plus, the val does *not* have the negativeStrand bit set yet
 */
static inline long long revComp(long long val)
/* reverse complement the 2-bit numerical value kmer */
{
/* complement bases and add 18 0 bits
 *    because this divide and conquer works on 64 bits, not 46,
 *      hence the addition of the 18 zero bits which fall out
 * The 'val ^ fortySixBits' does the complement, the shifting and
 *     masking does the reversing.
 */
register long long v = (val ^ fortySixBits) << 18;
v = ((v & high32bits) >> 32) | ((v & low32bits) << 32);
v = ((v & high16bits) >> 16) | ((v & low16bits) << 16);
v = ((v & high8bits) >> 8) | ((v & low8bits) << 8);
v = ((v & high4bits) >> 4) | ((v & low4bits) << 4);
v = ((v & high2bits) >> 2) | ((v & low2bits) << 2);
return v;
}	//	static long long revComp(long long val)

static void copyToArray(struct crisprList *list)
/* copy the crispr list data into arrays */
{
long startTime = clock1000();
struct crisprList *cl;
long long itemsCopied = 0;
long long chromsCopied = 0;

for (cl = list; cl; cl = cl->next)
    {
    ++chromsCopied;
    size_t memSize = cl->crisprCount * sizeof(long long);
    cl->sequence = (long long *)needLargeMem(memSize);
    cl->start = (long long *)needLargeMem(memSize);
    memSize = 5 * sizeof(int *);
    cl->offBy = (int **)needLargeMem(memSize);
    memSize = cl->crisprCount * sizeof(int);
    int r;
    for (r = 0; r < 5; ++r)
        cl->offBy[r] = (int *)needLargeZeroedMem(memSize);
    memSize = cl->crisprCount * sizeof(float);
    cl->mitSum = (float *)needLargeMem(memSize);

    long long i = 0;
    struct crispr *c;
    for (c = cl->chromCrisprs; c; c = c->next)
        {
	++itemsCopied;
        cl->sequence[i] = c->sequence;
        cl->start[i] = c->start;
        cl->mitSum[i] = 0.0;
	++i;
        }
    }

verbose(1, "# copyToArray: copied %lld chromosome lists\n", chromsCopied);
timingMessage("copyToArray", itemsCopied, "items copied", startTime,
    "items/sec", "seconds/item");

}	//	static void copyToArray(struct crisprList *list)	*/

static struct crisprList *generateKmers(struct dnaSeq *seq)
{
struct crispr *crisprSet = NULL;
struct crisprList *returnList = NULL;
AllocVar(returnList);
returnList->chrom = cloneString(seq->name);
returnList->size = seq->size;
int i;
DNA *dna;
long long chromPosition = 0;
long long startGap = 0;
long long gapCount = 0;
int kmerLength = 0;
long long kmerVal = 0;
long long reverseMask = (long long)0xf << 42;

dna=seq->dna;
for (i = 0; i < seq->size; ++i)
    {
    int val = orderedNtVal[(int)dna[i]];
    if (val >= 0)
	{
        kmerVal = fortySixBits & ((kmerVal << 2) | val);
        ++kmerLength;
	if (kmerLength > 22)
	    {
	    if ((endsAG == (kmerVal & 0xf)) || (endsGG == (kmerVal & 0xf)))
                {	/* have match for positive strand */
                struct crispr *oneCrispr = NULL;
                AllocVar(oneCrispr);
                oneCrispr->start = chromPosition - 22;
                oneCrispr->sequence = kmerVal;
                slAddHead(&crisprSet, oneCrispr);
                }
	    if ((beginsCT == (kmerVal & reverseMask))
		|| (beginsCC == (kmerVal & reverseMask)))
                {	/* have match for negative strand */
                struct crispr *oneCrispr = NULL;
                AllocVar(oneCrispr);
                oneCrispr->start = chromPosition - 22;
                oneCrispr->sequence = revComp(kmerVal);
                oneCrispr->sequence |= negativeStrand;
                slAddHead(&crisprSet, oneCrispr);
                }
	    }
        }	// if (val >= 0)
        else
            {
            ++gapCount;
            startGap = chromPosition;
            /* skip all N's == any value not = 0,1,2,3 */
            while ( ((i+1) < seq->size) && (orderedNtVal[(int)dna[i+1]] < 0))
                {
                ++chromPosition;
                ++i;
                }
            if (startGap != chromPosition)
                verbose(4, "#GAP %s\t%lld\t%lld\t%lld\t%lld\t%s\n", seq->name, startGap, chromPosition, gapCount, chromPosition-startGap, "+");
            else
                verbose(4, "#GAP %s\t%lld\t%lld\t%lld\t%lld\t%s\n", seq->name, startGap, chromPosition+1, gapCount, 1+chromPosition-startGap, "+");
            kmerLength = 0;  /* reset, start over */
            kmerVal = 0;
            }	// else if (val >= 0)
    ++chromPosition;
    }	// for (i = 0; i < seq->size; ++i)
// slReverse(&crisprSet);	// save time, order not important at this time
returnList->chromCrisprs = crisprSet;
returnList->crisprCount = slCount(crisprSet);
return returnList;
}	// static struct crisprList *generateKmers(struct dnaSeq *seq)

// for future reference, note the Cpf1 system:
//	https://benchling.com/pub/cpf1

static char *scoreToColor(long mitScore)
/* following scheme from Max's python scripts */
{
static char *green = "50,205,50";	/* #32cd32 */
static char *yellow = "255,255,0";	/* #ffff00 */
static char *black = "0,0,0";
static char *red = "170,1,20";		/* #aa0114 */
if (mitScore > 50)
    return green;
else if (mitScore > 20)
    return yellow;
else if (mitScore == -1)
    return black;
else
    return red;
}

static void countsOutput(struct crisprList *all, FILE *bedFH)
/* all done scanning and counting, print out all the guide data */
{
long startTime = clock1000();
long long itemsOutput = 0;
long long duplicatesMarked = 0;

struct crisprList *list;
long long totalOut = 0;
for (list = all; list; list = list->next)
    {
    long long c;
    for (c = 0; c < list->crisprCount; ++c)
        {
	++itemsOutput;
	if (list->sequence[c] & duplicateGuide)
	    ++duplicatesMarked;
	int negativeOffset = 0;
        char strand = '+';
        if (negativeStrand & list->sequence[c])
	    {
            strand = '-';
	    negativeOffset = pamSize;
	    }
	long long txStart = list->start[c] + negativeOffset;
	long long txEnd = txStart + guideSize;

        int mitScoreCount = + list->offBy[1][c] +
	    list->offBy[2][c] + list->offBy[3][c] + list->offBy[4][c];

        char kmerString[33];
        kmerValToString(kmerString, list->sequence[c], pamSize);
        char pamString[33];
        kmerPAMString(pamString, list->sequence[c]);

	if (bedFH)
	    {
	    long mitScore = 1.0;
            if (mitScoreCount > 0)  /* note: interger <- float conversion  */
	    {
		mitScore = roundf((list->mitSum[c] / (float) mitScoreCount));
	    }
	    else if (list->sequence[c] & duplicateGuide)
	    {
		mitScore = 0.0;
	    }
            char *color = scoreToColor(mitScore);
	    /* the end zero will be filled in by post-processing, it will be
	     *   the _offset into the crisprDetails.tab file
	     */
	    fprintf(bedFH, "%s\t%lld\t%lld\t\t%ld\t%c\t%lld\t%lld\t%s\t%s\t%s\t%d,%d,%d,%d,%d\t0\n", list->chrom, list->start[c], list->start[c]+pamSize+guideSize, mitScore, strand, txStart, txEnd, color, kmerString, pamString, list->offBy[0][c], list->offBy[1][c], list->offBy[2][c], list->offBy[3][c], list->offBy[4][c]);
	    }
	++totalOut;
	}
    }
if (bedFH)
    carefulClose(&bedFH);

verbose(1, "# countsOutput: %lld guides marked as duplicate sequence\n",
    duplicatesMarked);
timingMessage("countsOutput:", itemsOutput, "items output", startTime,
    "items/sec", "seconds/item");

}	//	static void countsOutput(struct crisprList *all, FILE *bedFH)

static struct crisprList *scanSequence(char *inFile)
/* scan the given file, return list of guides */
{
verbose(1, "# scanning sequence file: %s\n", inFile);
dnaUtilOpen();
struct dnaLoad *dl = dnaLoadOpen(inFile);
struct dnaSeq *seq;
struct crisprList *listReturn = NULL;

long startTime = clock1000();
long long totalCrisprs = 0;

/* scanning all sequences, setting up guides on the listReturn */
while ((seq = dnaLoadNext(dl)) != NULL)
    {
    if (startsWithNoCase("chrUn", seq->name) ||
         rStringIn("hap", seq->name) || rStringIn("_alt", seq->name) )
	{
	verbose(4, "# skip chrom: %s\n", seq->name);
	continue;
	}

    long startTime = clock1000();
    struct crisprList *oneList = generateKmers(seq);
    slAddHead(&listReturn, oneList);
    totalCrisprs += oneList->crisprCount;
    if (verboseLevel() > 3)
	timingMessage(seq->name, oneList->crisprCount, "guides", startTime,
	    "guides/sec", "seconds/guide");
    }

timingMessage("scanSequence", totalCrisprs, "total guides", startTime,
    "guides/sec", "seconds/guide");

return listReturn;
}	/*	static crisprList *scanSequence(char *inFile)	*/

static struct crisprList *rangeExtraction(struct crisprList **allReference)
/* given ranges in global rangesHash, construct new list of guides that
 *     have any type of overlap with the ranges, also extract those items from
 *         the all list.  Returns new list.
 */
{
struct crisprList *all = *allReference;
struct crisprList *listReturn = NULL;
struct crisprList *list;
int inputChromCount = slCount(all);
long long returnListCrisprCount = 0;
long long examinedCrisprCount = 0;
struct crisprList *prevChromList = NULL;

long startTime = clock1000();

struct crisprList *nextList = NULL;
for (list = all; list; list = nextList)
    {
    nextList = list->next;	// remember before perhaps lost
    long long beforeCrisprCount = list->crisprCount;
    examinedCrisprCount += list->crisprCount;
    struct binKeeper *bk = hashFindVal(rangesHash, list->chrom);
    struct crispr *newCrispr = NULL;
    if (bk != NULL)
        {
	struct crispr *prevCrispr = NULL;
	struct crispr *next = NULL;
	struct crispr *c;
        for (c = list->chromCrisprs; c; c = next)
            {
	    next = c->next;	// remember before perhaps lost
	    if (endsAG == (c->sequence & 0xf))
		{
		prevCrispr = c;	// remains on 'all' list
		continue;	// only check guides endsGG
		}
            struct binElement *hitList = NULL;
	    // select any guide that is at least half contained in any range
	    int midPoint = c->start + ((pamSize + guideSize) >> 1);
//            if (negativeStrand & c->sequence)
//                start += 2;
//            int end = midPoint + 1;
            hitList = binKeeperFind(bk, midPoint, midPoint + 1);
            if (hitList)
		{
		if (prevCrispr)	// remove this one from the 'all' list
		    prevCrispr->next = next;
                else
                    list->chromCrisprs = next;	// removing the first one
		c->next = NULL;			// new item for new list
		slAddHead(&newCrispr, c);	// constructing new list
		}
	    else
		prevCrispr = c;	// remains on 'all' list
	    slFreeList(&hitList);
            }
	}
    if (newCrispr)
	{
	struct crisprList *newItem = NULL;
	AllocVar(newItem);
	newItem->chrom = cloneString(list->chrom);
	newItem->size = list->size;
	newItem->chromCrisprs = newCrispr;
	newItem->crisprCount = slCount(newCrispr);
	returnListCrisprCount += newItem->crisprCount;
	slAddHead(&listReturn, newItem);
        if (NULL == list->chromCrisprs)	// all have been removed for this chrom
	    {
	    if (prevChromList)	// remove it from the chrom list
		prevChromList->next = nextList;
	    else
                all = nextList;	// removing the first one
	    }
	else
	    {
	    list->crisprCount = slCount(list->chromCrisprs);
            long long removedCrisprs = beforeCrisprCount - list->crisprCount;
	    verbose(1, "# range scan chrom %s had %lld guides, removed %lld leaving %lld target\n",
		list->chrom, beforeCrisprCount, removedCrisprs, list->crisprCount);
	    }
	}
    prevChromList = list;
    }	//	for (list = all; list; list = list->next)

verbose(1, "# range scanning %d chroms, return %d selected chroms, leaving %d chroms\n",
    inputChromCount, slCount(listReturn), slCount(all));

long long targetCrisprCount = examinedCrisprCount - returnListCrisprCount;
timingMessage("range scan", examinedCrisprCount, "examined guides",
    startTime, "guides/sec", "seconds/guide");
timingMessage("range scan", targetCrisprCount, "remaining target guides",
    startTime, "guides/sec", "seconds/guide");
timingMessage("range scan", returnListCrisprCount, "returned query guides",
    startTime, "guides/sec", "seconds/guide");

if (NULL == all)
    {
    allReference = NULL;	// they have all been removed
    verbose(1, "# range scan, every single chrom has been removed\n");
    }
else if (*allReference != all)
    {
    verbose(1, "# range scan, first chrom list has been moved from %p to %p\n", (void *)*allReference, (void *)all);
    *allReference = all;
    }
return listReturn;
}	//	static crisprList *rangeExtraction(crisprList *all)

static float hitScoreM[20] =
{
    0,0,0.014,0,0,
    0.395,0.317,0,0.389,0.079,
    0.445,0.508,0.613,0.851,0.732,
    0.828,0.615,0.804,0.685,0.583
};

static float calcHitScore(long long sequence1, long long sequence2)
/* calcHitScore - from Max's crispor.py script and paper:
 *    https://www.ncbi.nlm.nih.gov/pubmed/27380939 */
{
float score1 = 1.0;
int mmCount = 0;
int lastMmPos = -1;
/* the XOR determines differences in two sequences, the shift
 * right 6 removes the PAM sequence and the 'fortyBits &' eliminates
 * the negativeStrand and duplicateGuide bits
 */
long long misMatch = fortyBits & ((sequence1 ^ sequence2) >> 6);
int distCount = 0;
int distSum = 0;
int pos;
for (pos = 0; pos < guideSize; ++pos)
    {
    int diff = misMatch & 0x3;
    if (diff)
	{
	++mmCount;
        if (lastMmPos != -1)
	    {
	    distSum += pos-lastMmPos;
	    ++distCount;
	    }
	score1 *= 1 - hitScoreM[pos];
	lastMmPos = pos;
	}
    misMatch >>= 2;
    }

float score2 = 1.0;
if (distCount > 1)
    {
    float avgDist = (float)distSum / distCount;
    score2 = 1.0 / (((19-avgDist)/19.0) * 4 + 1);
    }
float score3 = 1.0;
if (mmCount > 0)
    score3 = 1.0 / (mmCount * mmCount);

return (score1 * score2 * score3 * 100);
} //	static float calcHitScore(long long sequence1, long long sequence2)

static float pamScores[4][4] = {
    { 0.0, 0.0, 0.25925926, 0.0 },	/* A[ACGT] */
    { 0.0, 0.0, 0.107142857, 0.0 },	/* C[ACGT] */
    { 0.069444444, 0.022222222, 1.0, 0.016129032 },	/* G[ACGT] */
    { 0.0, 0.0, 0.03896104, 0.0 }	/* T[ACGT] */
};

/* the [20] index is the position in the string
 * the first [4] index is the query base at that position
 * the second [4] index is the target base at that position
 */
static float cfdMmScores[20][4][4] =
{
    {	/* 0 */
	{ -1, 0.857142857, 1.0, 1.0 },
	{ 1.0, -1, 0.913043478, 1.0 },
	{ 0.9, 0.714285714, -1, 1.0 },
	{ 1.0, 0.857142857, 0.956521739, -1 },
    },
    {	/* 1 */
	{ -1, 0.785714286, 0.8, 0.727272727 },
	{ 0.727272727, -1, 0.695652174, 0.909090909 },
	{ 0.846153846, 0.692307692, -1, 0.636363636 },
	{ 0.846153846, 0.857142857, 0.84, -1 },
    },
    {	/* 2 */
	{ -1, 0.428571429, 0.611111111, 0.705882353 },
	{ 0.866666667, -1, 0.5, 0.6875 },
	{ 0.75, 0.384615385, -1, 0.5 },
	{ 0.714285714, 0.428571429, 0.5, -1 },
    },
    {	/* 3 */
	{ -1, 0.352941176, 0.625, 0.636363636 },
	{ 0.842105263, -1, 0.5, 0.8 },
	{ 0.9, 0.529411765, -1, 0.363636364 },
	{ 0.476190476, 0.647058824, 0.625, -1 },
    },
    {	/* 4 */
	{ -1, 0.5, 0.72, 0.363636364 },
	{ 0.571428571, -1, 0.6, 0.636363636 },
	{ 0.866666667, 0.785714286, -1, 0.3 },
	{ 0.5, 1.0, 0.64, -1 },
    },
    {	/* 5 */
	{ -1, 0.454545455, 0.714285714, 0.714285714 },
	{ 0.928571429, -1, 0.5, 0.928571429 },
	{ 1.0, 0.681818182, -1, 0.666666667 },
	{ 0.866666667, 0.909090909, 0.571428571, -1 },
    },
    {	/* 6 */
	{ -1, 0.4375, 0.705882353, 0.4375 },
	{ 0.75, -1, 0.470588235, 0.8125 },
	{ 1.0, 0.6875, -1, 0.571428571 },
	{ 0.875, 0.6875, 0.588235294, -1 },
    },
    {	/* 7 */
	{ -1, 0.428571429, 0.733333333, 0.428571429 },
	{ 0.65, -1, 0.642857143, 0.875 },
	{ 1.0, 0.615384615, -1, 0.625 },
	{ 0.8, 1.0, 0.733333333, -1 },
    },
    {	/* 8 */
	{ -1, 0.571428571, 0.666666667, 0.6 },
	{ 0.857142857, -1, 0.619047619, 0.875 },
	{ 0.642857143, 0.538461538, -1, 0.533333333 },
	{ 0.928571429, 0.923076923, 0.619047619, -1 },
    },
    {	/* 9 */
	{ -1, 0.333333333, 0.555555556, 0.882352941 },
	{ 0.866666667, -1, 0.388888889, 0.941176471 },
	{ 0.933333333, 0.4, -1, 0.8125 },
	{ 0.857142857, 0.533333333, 0.5, -1 },
    },
    {	/* 10 */
	{ -1, 0.4, 0.65, 0.307692308 },
	{ 0.75, -1, 0.25, 0.307692308 },
	{ 1.0, 0.428571429, -1, 0.384615385 },
	{ 0.75, 0.666666667, 0.4, -1 },
    },
    {	/* 11 */
	{ -1, 0.263157895, 0.722222222, 0.333333333 },
	{ 0.714285714, -1, 0.444444444, 0.538461538 },
	{ 0.933333333, 0.529411765, -1, 0.384615385 },
	{ 0.8, 0.947368421, 0.5, -1 },
    },
    {	/* 12 */
	{ -1, 0.210526316, 0.652173913, 0.3 },
	{ 0.384615385, -1, 0.136363636, 0.7 },
	{ 0.923076923, 0.421052632, -1, 0.3 },
	{ 0.692307692, 0.789473684, 0.260869565, -1 },
    },
    {	/* 13 */
	{ -1, 0.214285714, 0.466666667, 0.533333333 },
	{ 0.35, -1, 0.0, 0.733333333 },
	{ 0.75, 0.428571429, -1, 0.266666667 },
	{ 0.619047619, 0.285714286, 0.0, -1 },
    },
    {	/* 14 */
	{ -1, 0.272727273, 0.65, 0.2 },
	{ 0.222222222, -1, 0.05, 0.066666667 },
	{ 0.941176471, 0.272727273, -1, 0.142857143 },
	{ 0.578947368, 0.272727273, 0.05, -1 },
    },
    {	/* 15 */
	{ -1, 0.0, 0.192307692, 0.0 },
	{ 1.0, -1, 0.153846154, 0.307692308 },
	{ 1.0, 0.0, -1, 0.0 },
	{ 0.909090909, 0.666666667, 0.346153846, -1 },
    },
    {	/* 16 */
	{ -1, 0.176470588, 0.176470588, 0.133333333 },
	{ 0.466666667, -1, 0.058823529, 0.466666667 },
	{ 0.933333333, 0.235294118, -1, 0.25 },
	{ 0.533333333, 0.705882353, 0.117647059, -1 },
    },
    {	/* 17 */
	{ -1, 0.19047619, 0.4, 0.5 },
	{ 0.538461538, -1, 0.133333333, 0.642857143 },
	{ 0.692307692, 0.476190476, -1, 0.666666667 },
	{ 0.666666667, 0.428571429, 0.333333333, -1 },
    },
    {	/* 18 */
	{ -1, 0.206896552, 0.375, 0.538461538 },
	{ 0.428571429, -1, 0.125, 0.461538462 },
	{ 0.714285714, 0.448275862, -1, 0.666666667 },
	{ 0.285714286, 0.275862069, 0.25, -1 },
    },
    {	/* 19 */
	{ -1, 0.227272727, 0.764705882, 0.6 },
	{ 0.5, -1, 0.058823529, 0.3 },
	{ 0.9375, 0.428571429, -1, 0.7 },
	{ 0.5625, 0.090909091, 0.176470588, -1 },
    },
};

/* Cutting Frequency Determination */
static float calcCfdScore(long long sequence1, long long sequence2)
/* calcCfdScore - from cfd_score_calculator.py script and paper:
 *    https://www.ncbi.nlm.nih.gov/pubmed/27380939 */
{
float score = 1.0;
/* the XOR determine differences in two sequences, the
 * shift right 6 removes the PAM sequence and
 * the 'fortyBits &' eliminates the negativeStrand and duplicateGuide bits
 */
long long misMatch = fortyBits & ((sequence1 ^ sequence2) >> 6);
long long misMatchBitMask = 0xc000000000;
long long twoBitMask = 0x300000000000;
int shiftRight = 44;	/* to move the 2bits to bits 1,0 */
int index = 0;

while (misMatchBitMask)
    {
    if (misMatchBitMask & misMatch)
	{
        int queryIndex = (sequence1 & twoBitMask) >> shiftRight;
        int targetIndex = (sequence2 & twoBitMask) >> shiftRight;
	score *= cfdMmScores[index][queryIndex][targetIndex];
	}
    twoBitMask >>= 2;
    misMatchBitMask >>= 2;
    shiftRight -= 2;
    ++index;
    }
int pam1 = (sequence2 & 0xc) >> 2;
int pam2 = (sequence2 & 0x3);
score *= pamScores[pam1][pam2];

return score;
}	// static float calcCfdScore(long long sequence1, long long sequence2)

static void misMatchString(char *stringReturn, long long misMatch)
/* return ascii string ...*.....*.*.....*.. to indicate mis matches */
{
int i = 0;
long long twoBitMask = 0xc000000000;
while (twoBitMask)
    {
    stringReturn[i] = '.';
    if (twoBitMask & misMatch)
	stringReturn[i] = '*';
    ++i;
    twoBitMask >>= 2;
    }
stringReturn[i] = 0;
}	// static void misMatchString(char *returnString, long long misMatch)

static void recordOffTargets(struct crisprList *query,
    struct crisprList *target, int bitsOn, long long qIndex,
	long long tIndex, long long twoBitMisMatch)
/* bitsOn is from 1 to 4, record this match when less than 1000 total */
{
float mitScore =
    calcHitScore(query->sequence[qIndex], target->sequence[tIndex]);
float cfdScore =
    calcCfdScore(query->sequence[qIndex], target->sequence[tIndex]);
/* note: interger cfdInt <- cfdScore float conversion  */
int cfdInt = round(cfdScore * 1000);

/* Note from Max's script:
 *	this is a heuristic based on the guideSeq data where alternative
 *	PAMs represent only ~10% of all cleaveage events.
 *	We divide the MIT score by 5 to make sure that these off-targets
 *	are not ranked among the top but still appear in the list somewhat
 */
if ( (endsGG == (query->sequence[qIndex] & 0xf)) &&
	(endsGG != (target->sequence[tIndex] & 0xf) ) )
    mitScore *= 0.2;

query->mitSum[qIndex] += mitScore;

if (query->offBy[0][qIndex] ) // no need to accumulate if 0 mismatch > 0
    return;

if (offFile)
    {
    char queryString[33];	/* local storage for string */
    char targetString[33];	/* local storage for string */
    char queryPAM[33];	/* local storage for string */
    char targetPAM[33];	/* local storage for string */
    char misMatch[33];	/* ...*.....*.*... ascii string represent misMatch */

    int i;
    int offTargetCount = 0;
    for (i = 1; i < 5; ++i)
        offTargetCount += query->offBy[i][qIndex];

    if (offTargetCount < 1000)	// could be command line option limit
        {
        misMatchString(misMatch, twoBitMisMatch);
        kmerValToString(queryString, query->sequence[qIndex], pamSize);
        kmerValToString(targetString, target->sequence[tIndex], pamSize);
	kmerPAMString(queryPAM, query->sequence[qIndex]);
	kmerPAMString(targetPAM, target->sequence[tIndex]);
        fprintf(offFile, "%s:%lld %c %s %s %s %s %s;%lld%c;%d %s %d %.8f %.8f %s:%lld %c\n",
	    query->chrom, query->start[qIndex],
		negativeStrand & query->sequence[qIndex] ? '-' : '+',
                queryString, queryPAM, targetString, targetPAM,
                target->chrom, target->start[tIndex],
		negativeStrand & target->sequence[tIndex] ? '-' : '+', cfdInt,
		misMatch, bitsOn, mitScore, cfdScore, target->chrom,
		target->start[tIndex],
		negativeStrand & target->sequence[tIndex] ? '-' : '+');
        }
    }
}	//	static void recordOffTargets(struct crisprList *query,
	//	    struct crisprList *target, int bitsOn, long long qIndex,
	//		long long tIndex)


/* this queryVsTarget can be used by threads, appears to be safe */
static void queryVsTarget(struct crisprList *query, struct crisprList *target,
    int threadCount, int threadId)
/* run the query guides list against the target list in the array structures */
{
struct crisprList *qList;
long long totalCrisprsQuery = 0;
long long totalCrisprsTarget = 0;
long long totalCompares = 0;
struct loopControl *control = NULL;
AllocVar(control);

long startTime = clock1000();
long long duplicatesMarked = 0;

for (qList = query; qList; qList = qList->next)
    {
    totalCrisprsQuery += qList->crisprCount;
    if (threadCount > 1)
	{
	setLoopEnds(control, qList->crisprCount, threadCount, threadId);
	if (control->listStart >= qList->crisprCount)
	    continue;	/* next chrom, no part to be done for this thread */
	}
    else
	{
	control->listStart = 0;
	control->listEnd = qList->crisprCount;
	}
    verbose(1, "# thread %d of %d running %s %lld items [ %lld : %lld )\n",
	1 + threadId, threadCount, qList->chrom,
	    control->listEnd - control->listStart, control->listStart,
		control->listEnd);
    totalCrisprsTarget += control->listEnd - control->listStart;
    long long qCount;
    for (qCount = control->listStart; qCount < control->listEnd; ++qCount)
	{
        if (qList->sequence[qCount] & duplicateGuide)
	    continue;	/* already marked as duplicate */
        struct crisprList *tList;
        for (tList = target; tList; tList = tList->next)
            {
            long long tCount;
            totalCompares += tList->crisprCount;
            for (tCount = 0; tCount < tList->crisprCount; ++tCount)
                {
                /* the XOR determine differences in two sequences, the
                 * shift right 6 removes the PAM sequence and
                 * the 'fortyBits &' eliminates the negativeStrand and
		 * duplicateGuide bits
                 */
                long long misMatch = fortyBits &
                    ((qList->sequence[qCount] ^ tList->sequence[tCount]) >> 6);
                if (misMatch)
                    {
                    /* possible misMatch bit values: 01 10 11
                     *  turn those three values into just: 01
                     */
                    misMatch = (misMatch | (misMatch >> 1)) & 0x5555555555;
                    int bitsOn = _mm_popcnt_u64(misMatch);
                    if (bitsOn < 5)
                        {
                        recordOffTargets(qList, tList, bitsOn, qCount,
			    tCount, misMatch);
                        qList->offBy[bitsOn][qCount] += 1;
//			tList->offBy[bitsOn][tCount] += 1; not needed
                        }
                    }
                else
                    { 	/* no misMatch, identical guides */
                    qList->offBy[0][qCount] += 1;
                    qList->sequence[qCount] |= duplicateGuide;
		    ++duplicatesMarked;
//                  tList->offBy[0][tCount] += 1;	not needed
                    }
                } // for (tCount = 0; tCount < tList->crisprCount; ++tCount)
            }	//	for (tList = target; tList; tList = tList->next)
	}	//	for (qCount = 0; qCount < qList->crisprCount; ++qCount)
    }	//	for (qList = query; qList; qList = qList->next)

verbose(1, "# queryVsTarget: an additional %lld duplicates marked\n",
    duplicatesMarked);
timingMessage("queryVsTarget", totalCrisprsQuery, "query guides processed",
    startTime, "guides/sec", "seconds/guide");
timingMessage("queryVsTarget", totalCrisprsTarget, "vs target guides",
    startTime, "guides/sec", "seconds/guide");
timingMessage("queryVsTarget", totalCompares, "total comparisons",
    startTime, "compares/sec", "seconds/compare");

}	/* static struct crisprList *queryVsTarget(struct crisprList *query,
	    struct crisprList *target) */

static void queryVsSelf(struct crisprList *all)
/* run this 'all' list vs. itself avoiding self to self comparisons */
{
struct crisprList *qList;
long long totalCrisprsQuery = 0;
long long totalCrisprsCompare = 0;

long startTime = clock1000();

long long duplicatesMarked = 0;

/* query runs through all chroms */
for (qList = all; qList; qList = qList->next)
    {
    long long qCount;
    totalCrisprsQuery += qList->crisprCount;
    verbose(1, "# queryVsSelf %lld query guides on chrom %s\n", qList->crisprCount, qList->chrom);
    for (qCount = 0; qCount < qList->crisprCount; ++qCount)
	{
	/* target starts on same chrom as query, and
	   at next kmer after query for this first chrom */
        long long tStart = qCount+1;
        struct crisprList *tList;
        if (qList->sequence[qCount] & duplicateGuide)
	    continue;	/* already marked as duplicate */
        for (tList = qList; tList; tList = tList->next)
            {
            long long tCount;
	    totalCrisprsCompare += tList->crisprCount - tStart;
            for (tCount = tStart; tCount < tList->crisprCount; ++tCount)
                {
		/* the XOR determine differences in two sequences, the
		 * shift right 6 removes the PAM sequence and
		 * the 'fortyBits &' eliminates the negativeStrand and
		 * duplicateGuide bits
		 */
                long long misMatch = fortyBits &
                    ((qList->sequence[qCount] ^ tList->sequence[tCount]) >> 6);
                if (misMatch)
                    {
                    /* possible misMatch bit values: 01 10 11
                     *  turn those three values into just: 01
                     */
                    misMatch = (misMatch | (misMatch >> 1)) & 0x5555555555;
                    int bitsOn = _mm_popcnt_u64(misMatch);
		    if (bitsOn < 5)
			{
			recordOffTargets(qList, tList, bitsOn, qCount, tCount, misMatch);
			qList->offBy[bitsOn][qCount] += 1;
			tList->offBy[bitsOn][tCount] += 1;
			}
                    }
                else
                    { 	/* no misMatch, identical guides */
                    qList->sequence[qCount] |= duplicateGuide;
                    tList->sequence[tCount] |= duplicateGuide;
                    qList->offBy[0][qCount] += 1;
                    tList->offBy[0][tCount] += 1;
		    ++duplicatesMarked;
                    }
                } // for (tCount = 0; tCount < tList->crisprCount; ++tCount)
                tStart = 0;	/* following chroms run through all */
            }	//	for (tList = target; tList; tList = tList->next)
	}	//	for (qCount = 0; qCount < qList->crisprCount; ++qCount)
    }	//	for (qList = query; qList; qList = qList->next)

verbose(1, "# queryVsSelf: counted %lld duplicate guides\n", duplicatesMarked);
timingMessage("queryVsSelf", totalCrisprsQuery, "guides processed",
    startTime, "guides/sec", "seconds/guide");
timingMessage("queryVsSelf", totalCrisprsCompare, "total comparisons",
    startTime, "compares/sec", "seconds/compare");

}	/* static void queryVsSelf(struct crisprList *all) */

static struct crisprList *readKmers(char *fileIn)
/* read in kmer list from 'fileIn', return list structure */
{
errAbort("# XXX readKmers function not implemented\n");
verbose(1, "# reading guides from: %s\n", fileIn);
struct crisprList *listReturn = NULL;
struct lineFile *lf = lineFileOpen(fileIn, TRUE);
char *row[10];
int wordCount = 0;
long long crisprsInput = 0;

long startTime = clock1000();

while (0 < (wordCount = lineFileChopNextTab(lf, row, ArraySize(row))) )
    {
    if (3 != wordCount)
	errAbort("expecing three words at line %d in file %s, found: %d",
	    lf->lineIx, fileIn, wordCount);
    struct crisprList *newItem = NULL;
    AllocVar(newItem);
    newItem->chrom = cloneString(row[0]);
    newItem->crisprCount = sqlLongLong(row[1]);
    newItem->size = sqlLongLong(row[2]);
    newItem->chromCrisprs = NULL;
    slAddHead(&listReturn, newItem);
    long long verifyCount = 0;
    while ( (verifyCount < newItem->crisprCount) &&  (0 < (wordCount = lineFileChopNextTab(lf, row, ArraySize(row)))) )
	{
        if (3 != wordCount)
	    errAbort("expecing three words at line %d in file %s, found: %d",
		lf->lineIx, fileIn, wordCount);
        ++verifyCount;
        struct crispr *oneCrispr = NULL;
        AllocVar(oneCrispr);
        oneCrispr->sequence = sqlLongLong(row[0]);
        oneCrispr->start = sqlLongLong(row[1]);
        slAddHead(&newItem->chromCrisprs, oneCrispr);
	}
    if (verifyCount != newItem->crisprCount)
	errAbort("expecting %lld kmer items at line %d in file %s, found: %lld",
	    newItem->crisprCount, lf->lineIx, fileIn, verifyCount);
    crisprsInput += verifyCount;
    }

lineFileClose(&lf);

timingMessage("readKmers", crisprsInput, "guides read in", startTime,
    "guides/sec", "seconds/guide");

return listReturn;
}	//	static struct crisprList *readKmers(char *fileIn)

static void writeGuides(struct crisprList *all, char *fileOut)
/* write guide list 'all' to 'fileOut' */
{
FILE *fh = mustOpen(fileOut, "w");
struct crisprList *list;
long long crisprsWritten = 0;
char kmerString[33];
char pamString[33];

long startTime = clock1000();

/* slReverse(&all);   * can not do this here, destroy's caller's list,
                      * caller's value of all doesn't change */

for (list = all; list; list = list->next)
    {
    fprintf(fh, "# chrom\tcrisprCount\tchromSize\n");
    fprintf(fh, "# %s\t%lld\t%d\n", list->chrom, list->crisprCount, list->size);
    struct crispr *c;
    for (c = list->chromCrisprs; c; c = c->next)
	{
        kmerValToString(kmerString, c->sequence, pamSize);
        kmerPAMString(pamString, c->sequence);
	fprintf(fh, "%s\t%s\t%lld\t%c\n", kmerString, pamString,
	    c->start, negativeStrand & c->sequence ? '-' : '+');
	++crisprsWritten;
	}
    }
carefulClose(&fh);

timingMessage("writeGuides", crisprsWritten, "guides written", startTime,
    "guides/sec", "seconds/guide");
}	//	static void writeGuides(struct crisprList *all, char *fileOut)

static void *threadFunction(void *id)
/* thread entry */
{
struct threadControl *tId = (struct threadControl *)id;

queryVsTarget(tId->query, tId->target, tId->threadCount, tId->threadId);

return NULL;
}

static void runThreads(int threadCount, struct crisprList *query,
    struct crisprList *target)
{
struct threadControl *threadIds = NULL;
AllocArray(threadIds, threadCount);

pthread_t *threads = NULL;
AllocArray(threads, threadCount);

int pt;
for (pt = 0; pt < threadCount; ++pt)
    {
    struct threadControl *threadData;
    AllocVar(threadData);
    threadData->threadId = pt;
    threadData->threadCount = threadCount;
    threadData->query = query;
    threadData->target = target;
    threadIds[pt] = *threadData;
    int rc = pthread_create(&threads[pt], NULL, threadFunction, &threadIds[pt]);
    if (rc)
        {
        errAbort("Unexpected error %d from pthread_create(): %s",rc,strerror(rc));
        }
    }

/* Wait for threads to finish */
for (pt = 0; pt < threadCount; ++pt)
    pthread_join(threads[pt], NULL);
}

static void crisprKmers(char *sequence, FILE *bedFH)
/* crisprKmers - find and annotate crispr sequences. */
{
struct crisprList *queryGuides = NULL;
struct crisprList *allGuides = NULL;

if (loadKmers)
    allGuides = readKmers(loadKmers);
else
    allGuides = scanSequence(sequence);

// if (dumpGuides)
//     {
//     writeGuides(allGuides, dumpGuides);
//     return;
//     }

long long vmPeak = currentVmPeak();
verbose(1, "# vmPeak after scanSequence: %lld kB\n", vmPeak);

/* processing those guides */
if (verboseLevel() > 1)
    {
    if (offTargets)
	offFile = mustOpen(offTargets, "w");
    /* if ranges have been specified, construct queryList of kmers to measure */
    if (rangesHash)
        {
	/* result here is two exclusive sets: query, and allGuides
	 *    the query guides have been extracted from the allGuides */
        queryGuides = rangeExtraction(& allGuides);
	if (dumpGuides)
	    {
	    writeGuides(queryGuides, dumpGuides);
	    return;
	    }
        }
    if (queryGuides)
        copyToArray(queryGuides);
    if (allGuides)
	copyToArray(allGuides);
    vmPeak = currentVmPeak();
    verbose(1, "# vmPeak after copyToArray: %lld kB\n", vmPeak);
    /* larger example: 62646196 kB */
    if (threads > 1)
	{
	int gB = vmPeak >> 20;	/* convert kB to Gb */
	threadCount = threads;
	verbose(1, "# at %d Gb (%lld kB), running %d threads\n", gB, vmPeak, threadCount);
	}
    if (queryGuides)	// when range selected some query sequences
	{
	/* the query vs. itself avoiding self vs. self and marking dups */
        queryVsSelf(queryGuides);
	if (allGuides) // if there are any left on the all list
	    {
	    if (threadCount > 1)
		runThreads(threadCount, queryGuides, allGuides);
	    else
		queryVsTarget(queryGuides, allGuides, 0, 0);
	    }
        countsOutput(queryGuides, bedFH);
        }
    else
        {
	queryVsSelf(allGuides); /* run up all vs. all avoiding self vs. self */
	countsOutput(allGuides, bedFH);
        }

    carefulClose(&offFile);
    }
}	// static void crisprKmers(char *sequence, FILE *bedFH)

int main(int argc, char *argv[])
/* Process command line, initialize translation arrays and call the process */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

verbose(0, "# running verboseLevel: %d\n", verboseLevel());

bedFileOut = optionVal("bed", bedFileOut);
dumpGuides = optionVal("dumpGuides", dumpGuides);
loadKmers = optionVal("loadKmers", loadKmers);
offTargets = optionVal("offTargets", offTargets);
threads = optionInt("threads", threads);
if (threads < 0)
    errAbort("specified threads is less than 0: %d\n", threads);
if (threads > 0)
    verbose(1, "# requesting threads: %d\n", threads);
ranges = optionVal("ranges", ranges);
if (ranges)
    rangesHash = readRanges(ranges);

FILE *bedFH = NULL;
if (bedFileOut)
    {
    bedFH = mustOpen(bedFileOut, "w");
    verbose(1, "# output to bed file: %s\n", bedFileOut);
    }

initOrderedNtVal();	/* set up orderedNtVal[] */
crisprKmers(argv[1], bedFH);

if (verboseLevel() > 1)
    printVmPeak();
return 0;
}

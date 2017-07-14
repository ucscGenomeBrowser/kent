/* crisprKmers - find and annotate crispr sequences. */

/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

/*  Theory of operation:
  a. scan given sequence (2bit of fa or fa.gz file)
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
  "   -bed=<file> - output results to given bed9+ file\n"
  "   -offTargets=<file> - output off target data to given file\n"
  "   -ranges=<file> - use specified bed3 file to limit which guides are\n"
  "                  - measured, only those with any overlap to these bed items.\n"
  "   -memLimit=N - N number of gigabytes for each thread, default: no threading\n"
  "               when memory size goes beyond N gB program will thread gB/N times\n"
  "   -dumpKmers=<file> - NOT VALID after scan of sequence, output kmers to file\n"
  "                     - process will exit after this, use -loadKmers to continue\n"
  "   -loadKmers=<file> - NOT VALID load kmers from previous scan of sequence from -dumpKmers"
  );
}

static char *bedFileOut = NULL;	/* set with -bed=<file> argument, kmer output */
static char *offTargets = NULL;	/* write offTargets to <file> */
static FILE *offFile = NULL;	/* file handle to write off targets */
static char *ranges = NULL;	/* use ranges <file> to limit scanning */
static struct hash *rangesHash = NULL;	/* ranges into hash + binkeeper */
static char *dumpKmers = NULL;	/* file name to write out kmers from scan */
static char *loadKmers = NULL;	/* file name to read in kmers from previous scan */
static int memLimit = 0;	/* gB limit before going into thread mode */
static int threadCount = 1;	/* will be adjusted depending upon vmPeak */

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bed", OPTION_STRING},
   {"offTargets", OPTION_STRING},
   {"ranges", OPTION_STRING},
   {"memLimit", OPTION_INT},
   {"dumpKmers", OPTION_STRING},
   {"loadKmers", OPTION_STRING},
   {NULL, 0},
};

#define negativeStrand	0x0000800000000000

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
// considering using other bits during processing to mark
// an item for no more consideration

// sizeof(struct crispr): 32
struct crispr
/* one chromosome set of crisprs */
    {
    struct crispr *next;		/* Next in list. */
    long long sequence;	/* sequence value in 2bit format */
    long long start;		/* chromosome start 0-relative */
    };

// sizeof(struct crisprList): 72
struct crisprList
/* all chromosome sets of crisprs */
    {
    struct crisprList *next;	/* Next in list. */
    char *chrom;		/* chrom name */
    int size;			/* chrom size */
    struct crispr *chromCrisprs;	/* all the crisprs on this chrom */
    long long crisprCount;	/* number of crisprs on this chrom */
    long long *sequence;	/* array of the sequences */
    long long *start;		/* array of the starts */
    int **offBy;		/* offBy[5][n] */
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
    long ms, char *units, char *invUnits)
{
double perSecond = 0.0;
double inverse = 0.0;
if ((ms > 0) && (count > 0))
    {
    perSecond = 1000.0 * count / ms;
    inverse = 1.0 / perSecond;
    }

verbose(1, "# %s: %lld %s @ %ld ms -> %.2f %s == %g %s\n", prefix, count, message, ms, perSecond, units, inverse, invUnits);
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
struct crisprList *cl = NULL;
long long itemsCopied = 0;

for (cl = list; cl; cl = cl->next)
    {
    size_t memSize = cl->crisprCount * sizeof(long long);
    cl->sequence = (long long *)needLargeMem(memSize);
    cl->start = (long long *)needLargeMem(memSize);
    memSize = 5 * sizeof(int *);
    cl->offBy = (int **)needLargeMem(memSize);
    memSize = cl->crisprCount * sizeof(int);
    int r = 0;
    for (r = 0; r < 5; ++r)
        cl->offBy[r] = (int *)needLargeZeroedMem(memSize);

    long long i = 0;
    struct crispr *c = NULL;
    for (c = cl->chromCrisprs; c; c = c->next)
        {
	++itemsCopied;
        cl->sequence[i] = c->sequence;
        cl->start[i] = c->start;
	++i;
        }
    }
long elapsedMs = clock1000() - startTime;
timingMessage("copyToArray", itemsCopied, "items copied", elapsedMs, "items/sec", "seconds/item");
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
long long endsAG = (A_BASE << 2) | G_BASE;
long long endsGG = (G_BASE << 2) | G_BASE;
long long beginsCT = (long long)((C_BASE << 2) | T_BASE) << 42;
long long beginsCC = (long long)((C_BASE << 2) | C_BASE) << 42;
long long reverseMask = (long long)0xf << 42;
verbose(4, "#   endsAG: %032llx\n", endsAG);
verbose(4, "#   endsGG: %032llx\n", endsGG);
verbose(4, "# beginsCT: %032llx\n", beginsCT);
verbose(4, "# beginsCC: %032llx\n", beginsCC);
verbose(4, "#  46 bits: %032llx\n", (long long) fortySixBits);

dna=seq->dna;
for (i=0; i < seq->size; ++i)
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
	    if ((beginsCT == (kmerVal & reverseMask)) || (beginsCC == (kmerVal & reverseMask)))
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
    }	// for (i=0; i < seq->size; ++i)
// slReverse(&crisprSet);	// save time, order not important at this time
returnList->chromCrisprs = crisprSet;
returnList->crisprCount = slCount(crisprSet);
return returnList;
}	// static struct crisprList *generateKmers(struct dnaSeq *seq)

static char *itemColor(int **offBy, long long index)
/* temporary itemColor, post processing will do this correctly */
{
static char *notUnique = "150,150,150";
// static char *notUniqueAlt = "150,150,150";
static char *twoMany = "120,120,120";
// static char *twoManyAlt = "120,120,120";
static char *highScore = "0,255,0";	// > 70
static char *mediumScore = "128,128,0";	// > 50
static char *lowScore = "255,0,0";	// >= 50
// static char *lowestScore = "80,80,80";	// < 50
// static char *lowestScoreAlt = "80,80,80";	// < 50
if (offBy[0][index])
    return notUnique;
else
    {
    int offSum = offBy[1][index] + offBy[2][index] + offBy[3][index] +
		offBy[4][index];
    if (offSum < 100)
       return highScore;
    else if (offSum < 150)
       return mediumScore;
    else if (offSum < 250)
       return lowScore;
    else
       return twoMany;
    }
}

static void countsOutput(struct crisprList *all, FILE *bedFH)
/* everything has been scanned and counted, print out all the data from arrays*/
{
long startTime = clock1000();
long long itemsOutput = 0;

struct crisprList *list;
long long totalOut = 0;
for (list = all; list; list = list->next)
    {
    long long c = 0;
    for (c = 0; c < list->crisprCount; ++c)
        {
	++itemsOutput;
	int negativeOffset = 0;
        char strand = '+';
        if (negativeStrand & list->sequence[c])
	    {
            strand = '-';
	    negativeOffset = 3;
	    }
	long long txStart = list->start[c] + negativeOffset;
	long long txEnd = txStart + 20;

        int totalOffs = list->offBy[0][c] + list->offBy[1][c] +
	    list->offBy[2][c] + list->offBy[3][c] + list->offBy[4][c];

        char *color = itemColor(list->offBy, c);
        char kmerString[33];
        kmerValToString(kmerString, list->sequence[c], 3);
        char pamString[33];
        kmerPAMString(pamString, list->sequence[c]);

        if (0 == totalOffs)
verbose(1, "# PERFECT score %s:%lld %c\t%s\n", list->chrom, list->start[c], strand, kmerString);

	if (bedFH)
	    fprintf(bedFH, "%s\t%lld\t%lld\t%d,%d,%d,%d,%d\t%d\t%c\t%lld\t%lld\t%s\t%s\t%s\t%s\t%s\t%d\t%d\t%d\t%d\n", list->chrom, list->start[c], list->start[c]+23, list->offBy[0][c], list->offBy[1][c], list->offBy[2][c], list->offBy[3][c], list->offBy[4][c],  list->offBy[0][c], strand, txStart, txEnd, color, color, color, kmerString, pamString, list->offBy[1][c], list->offBy[2][c], list->offBy[3][c], list->offBy[4][c]);

        if (list->offBy[0][c])
           verbose(3, "# array identical: %d %s:%lld %c\t%s\n", list->offBy[0][c], list->chrom, list->start[c], strand, kmerString);
	++totalOut;
	}
    }
if (bedFH)
    carefulClose(&bedFH);

long elapsedMs = clock1000() - startTime;
timingMessage("copyToArray:", itemsOutput, "items output", elapsedMs, "items/sec", "seconds/item");
}	//	static void countsOutput(struct crisprList *all, FILE *bedFH)

static struct crisprList *scanSequence(char *inFile)
/* scan the given file, return list of crisprs */
{
verbose(1, "#\tscanning sequence file: %s\n", inFile);
dnaUtilOpen();
struct dnaLoad *dl = dnaLoadOpen(inFile);
struct dnaSeq *seq;
struct crisprList *listReturn = NULL;

long elapsedMs = 0;
long scanStart = clock1000();
long long totalCrisprs = 0;

/* scanning all sequences, setting up crisprs on the listReturn */
while ((seq = dnaLoadNext(dl)) != NULL)
    {
    if (startsWithNoCase("chrUn", seq->name) ||
         rStringIn("hap", seq->name) || rStringIn("_alt", seq->name) )
	{
	verbose(1, "# skip chrom: %s\n", seq->name);
	continue;
	}

    long startTime = clock1000();
    struct crisprList *oneList = generateKmers(seq);
    slAddHead(&listReturn, oneList);
    totalCrisprs += oneList->crisprCount;
    elapsedMs = clock1000() - startTime;
    timingMessage(seq->name, oneList->crisprCount, "crisprs", elapsedMs, "crisprs/sec", "seconds/crispr");
    }

elapsedMs = clock1000() - scanStart;
timingMessage("scanSequence", totalCrisprs, "total crisprs", elapsedMs, "crisprs/sec", "seconds/crispr");
return listReturn;
}	/*	static crisprList *scanSequence(char *inFile)	*/

static struct crisprList *rangeExtraction(struct crisprList **allReference)
/* given ranges in global rangesHash, construct new list of crisprs that
 *     have any type of overlap with the ranges, also extract those items from
 *         the all list.  Returns new list.
 */
{
struct crisprList *all = *allReference;
struct crisprList *listReturn = NULL;
struct crisprList *list = NULL;
int inputChromCount = slCount(all);
long long returnListCrisprCount = 0;
long long examinedCrisprCount = 0;
struct crisprList *prevChromList = NULL;

long elapsedMs = 0;
long scanStart = clock1000();

struct crisprList *nextList = NULL;
for (list = all; list; list = nextList)
    {
    nextList = list->next;	// remember before perhaps lost
    long long beforeCrisprCount = list->crisprCount;
    examinedCrisprCount += list->crisprCount;
    struct crispr *c = NULL;
    struct binKeeper *bk = hashFindVal(rangesHash, list->chrom);
    struct crispr *newCrispr = NULL;
    if (bk != NULL)
        {
	struct crispr *prevCrispr = NULL;
	struct crispr *next = NULL;
        for (c = list->chromCrisprs; c; c = next)
            {
            struct binElement *hitList = NULL;
	    next = c->next;	// remember before perhaps lost
            int start = c->start;
            if (negativeStrand & c->sequence)
                start += 2;
            int end = start + 20;
            hitList = binKeeperFind(bk, start, end);
            if (hitList)
		{
		if (prevCrispr)	// remove this one from the all list
		    prevCrispr->next = next;
                else
                    list->chromCrisprs = next;	// removing the first one
		c->next = NULL;
		slAddHead(&newCrispr, c);	// constructing new list
		}
	    else
		prevCrispr = c;	// remains on all list
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
	    verbose(1, "# range scan chrom %s had %lld crisprs, removed %lld leaving %lld target\n",
		list->chrom, beforeCrisprCount, removedCrisprs, list->crisprCount);
	    }
	}
    prevChromList = list;
    }	//	for (list = all; list; list = list->next)

elapsedMs = clock1000() - scanStart;
verbose(1, "# range scanning %d chroms, return %d selected chroms, leaving %d chroms\n",
    inputChromCount, slCount(listReturn), slCount(all));
long long targetCrisprCount = examinedCrisprCount - returnListCrisprCount;
timingMessage("range scan", examinedCrisprCount, "examined crisprs", elapsedMs, "crisprs/sec", "seconds/crispr");
timingMessage("range scan", targetCrisprCount, "remaining target crisprs", elapsedMs, "crisprs/sec", "seconds/crispr");
timingMessage("range scan", returnListCrisprCount, "returned query crisprs", elapsedMs, "crisprs/sec", "seconds/crispr");
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

static double hitScoreM[20] = {0,0,0.014,0,0,0.395,0.317,0,0.389,0.079,0.445,0.508,0.613,0.851,0.732,0.828,0.615,0.804,0.685,0.583};

static double calcHitScore(long long sequence1, long long sequence2)
/* calcHitScore - from Max' crispor.py script and paper:
 *    https://www.ncbi.nlm.nih.gov/pubmed/27380939 */
{
double score1 = 1.0;
int mmCount = 0;
int lastMmPos = -1;
/* the XOR determines differences in two sequences, the shift
 * right 6 removes the PAM sequence and the 'fortyBits &' eliminates
 * the negativeStrand bit
 */
long long misMatch = fortyBits & ((sequence1 ^ sequence2) >> 6);
int distCount = 0;
int distSum = 0;
int pos = 0;
for (pos = 0; pos < 20; ++pos)
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

double score2 = 1.0;
if (distCount > 1)
    {
    double avgDist = (double)distSum / distCount;
    score2 = 1.0 / (((19-avgDist)/19.0) * 4 + 1);
    }
double score3 = 1.0;
if (mmCount > 0)
    score3 = 1.0 / (mmCount * mmCount);

return (score1 * score2 * score3 * 100);
} //	static double calcHitScore(long long sequence1, long long sequence2)

#ifdef NOT
static double calcCfdScore(long long sequence1, long long sequence2)
/* calcCfdScore - from Max' crispor.py script and paper:
 *    https://www.ncbi.nlm.nih.gov/pubmed/27380939 */
{
return 0.0;
}	// static double calcCfdScore(long long sequence1, long long sequence2)
#endif

static void misMatchString(char *stringReturn, long long misMatch)
/* return ascii string ...*.....*.*.....*.. to indicate mis matches */
{
int i = 0;
long long bitMask = 0xc000000000;
while (bitMask)
    {
    stringReturn[i] = '.';
    if (bitMask & misMatch)
	stringReturn[i] = '*';
    ++i;
    bitMask >>= 2;
    }
stringReturn[i] = 0;
}	// static void misMatchString(char *returnString, long long misMatch)

static void recordOffTargets(struct crisprList *query,
    struct crisprList *target, int bitsOn, long long qIndex,
	long long tIndex, long long twoBitMisMatch)
/* bitsOn is from 1 to 4, record this match when less than 1000 total */
{
char queryString[33];	/* local storage for string */
char targetString[33];	/* local storage for string */
char queryPAM[33];	/* local storage for string */
char targetPAM[33];	/* local storage for string */
char misMatch[33];	/* ...*.....*.*... ascii string represent misMatch */

if (query->offBy[0][qIndex] ) // no need to accumulate if 0-mismatch > 0
    return;

if (offFile)
    {
    int i = 0;
    int bitsOnSum = 0;
    for (i = 1; i < 5; ++i)
        bitsOnSum += query->offBy[i][qIndex];

    misMatchString(misMatch, twoBitMisMatch);

    if (bitsOnSum < 1000)	// could be command line option limit
        {
        double hitScore =
		calcHitScore(query->sequence[qIndex], target->sequence[tIndex]);
//        double cfdScore =
//		calcCfdScore(query->sequence[qIndex], target->sequence[tIndex]);
        kmerValToString(queryString, query->sequence[qIndex], 3);
        kmerValToString(targetString, target->sequence[tIndex], 3);
	kmerPAMString(queryPAM, query->sequence[qIndex]);
	kmerPAMString(targetPAM, target->sequence[tIndex]);
        fprintf(offFile, "%s:%lld %c %s%s %s%s %s %d %.8f\t%s:%lld %c\n",
	    query->chrom, query->start[qIndex],
		negativeStrand & query->sequence[qIndex] ? '-' : '+',
                queryString, queryPAM, targetString, targetPAM,
                misMatch, bitsOn, hitScore, target->chrom,
		target->start[tIndex],
		negativeStrand & target->sequence[tIndex] ? '-' : '+');
        }
    }
}	//	static void recordOffTargets(struct crisprList *query,
	//	    struct crisprList *target, int bitsOn, long long qIndex,
	//		long long tIndex)


/* this queryVsTarget can be used by threads, it should be safe,
 * remains to be proven.
 */
static void queryVsTarget(struct crisprList *query, struct crisprList *target,
    int threadCount, int threadId)
/* run the query crisprs list against the target list in the array structures */
{
struct crisprList *qList = NULL;
long long totalCrisprsQuery = 0;
long long totalCrisprsTarget = 0;
long long totalCompares = 0;
struct loopControl *control = NULL;
AllocVar(control);

long processStart = clock1000();
long elapsedMs = 0;

for (qList = query; qList; qList = qList->next)
    {
    long long qCount = 0;
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
    for (qCount = control->listStart; qCount < control->listEnd; ++qCount)
	{
        struct crisprList *tList = NULL;
        for (tList = target; tList; tList = tList->next)
            {
            long long tCount = 0;
	    totalCompares += tList->crisprCount;
            for (tCount = 0; tCount < tList->crisprCount; ++tCount)
                {
		/* the XOR determine differences in two sequences, the
		 * shift right 6 removes the PAM sequence and
		 * the 'fortyBits &' eliminates the negativeStrand bit
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
//			tList->offBy[bitsOn][tCount] += 1; not needed
			}
                    }
                else
                    { 	/* no misMatch, identical crisprs */
                    qList->offBy[0][qCount] += 1;
//                  tList->offBy[0][tCount] += 1;	not needed
                    }
                } // for (tCount = 0; tCount < tList->crisprCount; ++tCount)
            }	//	for (tList = target; tList; tList = tList->next)
	}	//	for (qCount = 0; qCount < qList->crisprCount; ++qCount)
    }	//	for (qList = query; qList; qList = qList->next)
verbose(1, "# done with scanning, check timing\n");
elapsedMs = clock1000() - processStart;
timingMessage("queryVsTarget", totalCrisprsQuery, "query crisprs processed", elapsedMs, "crisprs/sec", "seconds/crispr");
timingMessage("queryVsTarget", totalCrisprsTarget, "vs target crisprs", elapsedMs, "crisprs/sec", "seconds/crispr");
timingMessage("queryVsTarget", totalCompares, "total comparisons", elapsedMs, "compares/sec", "seconds/compare");
}	/* static struct crisprList *queryVsTarget(struct crisprList *query,
	    struct crisprList *target) */

static void queryVsSelf(struct crisprList *all)
/* run this 'all' list vs. itself avoiding self to self comparisons */
{
struct crisprList *qList = NULL;
long long totalCrisprsQuery = 0;
long long totalCrisprsCompare = 0;

long processStart = clock1000();
long elapsedMs = 0;

/* query runs through all chroms */
for (qList = all; qList; qList = qList->next)
    {
    long long qCount = 0;
    totalCrisprsQuery += qList->crisprCount;
    verbose(1, "# queryVsSelf %lld query crisprs on chrom %s\n", qList->crisprCount, qList->chrom);
    for (qCount = 0; qCount < qList->crisprCount; ++qCount)
	{
	/* target starts on same chrom as query, and
	   at next kmer after query for this first chrom */
        long long tStart = qCount+1;
        struct crisprList *tList = NULL;
        for (tList = qList; tList; tList = tList->next)
            {
            long long tCount = tStart;
	    totalCrisprsCompare += tList->crisprCount - tStart;
            for (tCount = tStart; tCount < tList->crisprCount; ++tCount)
                {
		/* the XOR determine differences in two sequences, the
		 * shift right 6 removes the PAM sequence and
		 * the 'fortyBits &' eliminates the negativeStrand bit
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
                    { 	/* no misMatch, identical crisprs */
                    qList->offBy[0][qCount] += 1;
                    tList->offBy[0][tCount] += 1;
                    }
                } // for (tCount = 0; tCount < tList->crisprCount; ++tCount)
                tStart = 0;	/* following chroms run through all */
            }	//	for (tList = target; tList; tList = tList->next)
	}	//	for (qCount = 0; qCount < qList->crisprCount; ++qCount)
    }	//	for (qList = query; qList; qList = qList->next)
elapsedMs = clock1000() - processStart;
timingMessage("queryVsSelf", totalCrisprsQuery, "crisprs processed", elapsedMs, "crisprs/sec", "seconds/crispr");
timingMessage("queryVsSelf", totalCrisprsCompare, "total comparisons", elapsedMs, "compares/sec", "seconds/compare");
}	/* static void queryVsSelf(struct crisprList *all) */

static struct crisprList *readKmers(char *fileIn)
/* read in kmer list from 'fileIn', return list structure */
{
errAbort("# XXX readKmers function not implemented\n");
verbose(1, "# reading crisprs from: %s\n", fileIn);
struct crisprList *listReturn = NULL;
struct lineFile *lf = lineFileOpen(fileIn, TRUE);
char *row[10];
int wordCount = 0;
long long crisprsInput = 0;

long startMs = clock1000();

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

long elapsedMs = clock1000() - startMs;
timingMessage("readKmers", crisprsInput, "crisprs read in", elapsedMs, "crisprs/sec", "seconds/crispr");

return listReturn;
}	//	static struct crisprList *readKmers(char *fileIn)

static void writeKmers(struct crisprList *all, char *fileOut)
/* write kmer list 'all' to 'fileOut' */
{
errAbort("# XXX writeKmers function not implemented\n");
FILE *fh = mustOpen(fileOut, "w");
struct crisprList *list = NULL;
long long crisprsWritten = 0;
char kmerString[33];
char pamString[33];

long startMs = clock1000();

slReverse(&all);
for (list = all; list; list = list->next)
    {
    fprintf(fh, "%s\t%lld\t%d\n", list->chrom, list->crisprCount, list->size);
    struct crispr *c = NULL;
    slReverse(&list->chromCrisprs);
    for (c = list->chromCrisprs; c; c = c->next)
	{
        kmerValToString(kmerString, c->sequence, 3);
        kmerPAMString(pamString, c->sequence);
	fprintf(fh, "%s\t%s\t%lld\t%c\n", kmerString, pamString,
	    c->start, negativeStrand & c->sequence ? '-' : '+');
	++crisprsWritten;
	}
    }
carefulClose(&fh);

long elapsedMs = clock1000() - startMs;
timingMessage("writeKmers", crisprsWritten, "crisprs written", elapsedMs, "crisprs/sec", "seconds/crispr");
}	//	static void writeKmers(struct crisprList *all, char *fileOut)

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

int pt = 0;
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

if (dumpKmers)
    {
    writeKmers(allGuides, dumpKmers);
    return;
    }

long long vmPeak = currentVmPeak();
verbose(1, "# vmPeak after scanSequence: %lld kB\n", vmPeak);

/* processing those crisprs */
if (verboseLevel() > 1)
    {
    if (offTargets)
	offFile = mustOpen(offTargets, "w");
    /* if ranges have been specified, construct queryList of kmers to measure */
    if (rangesHash)
        {
	/* result here is two exclusive sets: query, and allGuides
	 *    the query crisprs have been extracted from the allGuides */
        queryGuides = rangeExtraction(& allGuides);
        }
    if (queryGuides)
        copyToArray(queryGuides);
    if (allGuides)
	copyToArray(allGuides);
    vmPeak = currentVmPeak();
    verbose(1, "# vmPeak after copyToArray: %lld kB\n", vmPeak);
    /* larger example: 62646196 kB */
    if ((memLimit > 0) && ((vmPeak >> 20) > memLimit))	// the >> 20 converts kB to gB
	{
	threadCount = 1 + ((vmPeak >> 20) / memLimit);
	verbose(1, "# over %d Gb at %lld kB, threadCount: %d\n", memLimit, vmPeak, threadCount);
	}
    if (queryGuides)	// when range selected some query sequences
	{
	if (allGuides) // if there are any left on the all list
	    {
	    if (threadCount > 1)
		runThreads(threadCount, queryGuides, allGuides);
	    else
		queryVsTarget(queryGuides, allGuides, 1, 0);
	    }
	/* then run up the query vs. itself avoiding self vs. self */
        queryVsSelf(queryGuides);
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
dumpKmers = optionVal("dumpKmers", dumpKmers);
loadKmers = optionVal("loadKmers", loadKmers);
offTargets = optionVal("offTargets", offTargets);
memLimit = optionInt("memLimit", memLimit);
ranges = optionVal("ranges", ranges);
if (ranges)
    rangesHash = readRanges(ranges);

FILE *bedFH = NULL;
if (bedFileOut)
    bedFH = mustOpen(bedFileOut, "w");

initOrderedNtVal();	/* set up orderedNtVal[] */
crisprKmers(argv[1], bedFH);

if (verboseLevel() > 1)
    printVmPeak();
return 0;
}

/* crisprKmers - find and annotate crispr sequences. */
#include <popcntintrin.h>
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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "crisprKmers - annotate crispr sequences\n"
  "usage:\n"
  "   crisprKmers <sequence>\n"
  "options:\n"
  "   where <sequence> is a .2bit file or .fa fasta sequence\n"
  "   -verbose=N - control processing steps with level of verbose:\n"
  "                default N=1 - only find crisprs, set up listings\n"
  "                N=2 - empty process crispr list into new list\n"
  "                N=3 - only count identicals during processing\n"
  "                N=4 - attempt measure all off targets and\n"
  "                      print out all crisprs as bed format\n"
  "   -bed=<file> - output kmers to given bed9+ file\n"
  "   -ranges=<file> - use specified bed3 file to limit which crisprs are\n"
  "                  - measured, only those within bed items."
  );
}

static char *bedFileOut = NULL;	/* set with -bed=<file> argument */
static char *ranges = NULL;	/* set with -ranges=<file> argument */
static struct hash *rangesHash = NULL;	/* ranges into hash + binkeeper */

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bed", OPTION_STRING},
   {"ranges", OPTION_STRING},
   {NULL, 0},
};

struct crispr
/* one chromosome set of crisprs */
    {
    struct crispr *next;		/* Next in list. */
    long long sequence;	/* sequence value in 2bit format */
    unsigned long long start;		/* chromosome start 0-relative */
    char strand;			/* strand: + or - */
    int offBy0;		/* counting number of off targets, 0 mismatch */
    int offBy1;		/* counting number of off targets, 1 mismatch */
    int offBy2;		/* counting number of off targets, 2 mismatch */
    int offBy3;		/* counting number of off targets, 3 mismatch */
    int offBy4;		/* counting number of off targets, 4 mismatch */
    };

struct crisprList
/* all chromosome sets of crisprs */
    {
    struct crisprList *next;	/* Next in list. */
    char *chrom;		/* chrom name */
    int size;			/* chrom size */
    struct crispr *chromCrisprs;	/* all the crisprs on this chrom */
    unsigned long long crisprCount;	/* number of crisprs on this chrom */
    };

#define A_BASE	0
#define C_BASE	1
#define G_BASE	2
#define T_BASE	3
#define U_BASE	3
static int orderedNtVal[256];	/* values in alpha order: ACGT 00 01 10 11 */
				/* for easier sorting and complementing */
static char bases[4];  /* for binary to ascii conversion */

#define fortySixBits	0x3fffffffffff
#define fortyEixhtBits	0xffffffffffff
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

static struct hash *readRanges(char *bedFile)
/* Read bed and return it as a hash keyed by chromName
 * with binKeeper values.  (from: src/hg/bedIntersect/bedIntersec.c) */
{
struct hash *hash = newHash(0);	/* key is chromName, value is binkeeper data */
struct lineFile *lf = lineFileOpen(bedFile, TRUE);
char *row[3];

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

static char *kmerPAMString(struct crispr *c)
/* return the ASCII string for last three bases in then binary sequence value */
{
long long val = c->sequence;
static char pamString[32];
long long twoBitMask = 0x30;
int shiftCount = 4;
int baseCount = 0;

while (twoBitMask)
    {
    int base = (val & twoBitMask) >> shiftCount;
    pamString[baseCount++] = bases[base];
    twoBitMask >>= 2;
    shiftCount -= 2;
    }
pamString[baseCount] = 0;
return pamString;
}	//	static char *kmerValToString(struct crispr *c, int trim)

static char *kmerValToString(struct crispr *c, int trim)
/* print out ASCII string for binary sequence value */
{
long long val = c->sequence;
static char kmerString[32];
long long twoBitMask = 0x300000000000;
int shiftCount = 44;
int baseCount = 0;

while (twoBitMask && (shiftCount >= (2*trim)))
    {
    int base = (val & twoBitMask) >> shiftCount;
    kmerString[baseCount++] = bases[base];
    twoBitMask >>= 2;
    shiftCount -= 2;
    }
kmerString[baseCount] = 0;
return kmerString;
}	//	static char *kmerValToString(struct crispr *c, int trim)
 
static long long revComp(long long val)
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

static struct crisprList *generateKmers(struct dnaSeq *seq)
{
struct crispr *crisprSet = NULL;
struct crisprList *returnList = NULL;
AllocVar(returnList);
returnList->chrom = cloneString(seq->name);
returnList->size = seq->size;
int i;
DNA *dna;
unsigned long long chromPosition = 0;
unsigned long long startGap = 0;
unsigned long long gapCount = 0;
int kmerLength = 0;
long long kmerVal = 0;
long long endsGG = (G_BASE << 2) | G_BASE;
long long beginsCC = (long long)((C_BASE << 2) | C_BASE) << 42;
long long reverseMask = (long long)0xf << 42;
verbose(4, "#   endsGG: %032llx\n", endsGG);
verbose(4, "# beginsCC: %032llx\n", beginsCC);
verbose(4, "#  46 bits: %032llx\n", (unsigned long long) fortySixBits);

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
	    if (endsGG == (kmerVal & 0xf))	/* check positive strand */
                {
                struct crispr *oneCrispr = NULL;
                AllocVar(oneCrispr);
                oneCrispr->start = chromPosition - 22;
                oneCrispr->strand = '+';
                oneCrispr->sequence = kmerVal;
                slAddHead(&crisprSet, oneCrispr);
                }
	    if (beginsCC == (kmerVal & reverseMask))	/* check neg strand */
                {
                struct crispr *oneCrispr = NULL;
                AllocVar(oneCrispr);
                oneCrispr->start = chromPosition - 22;
                oneCrispr->strand = '-';
                oneCrispr->sequence = revComp(kmerVal);
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
                verbose(4, "#GAP %s\t%llu\t%llu\t%llu\t%llu\t%s\n", seq->name, startGap, chromPosition, gapCount, chromPosition-startGap, "+");
            else
                verbose(4, "#GAP %s\t%llu\t%llu\t%llu\t%llu\t%s\n", seq->name, startGap, chromPosition+1, gapCount, 1+chromPosition-startGap, "+");
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

static void showCounts(struct crisprList *all)
/* everything has been scanned and counted, print out all the data */
{
FILE *bedFH = NULL;
if (bedFileOut)
    bedFH = mustOpen(bedFileOut, "w");

verbose(1, "#\tprint out all data\n");
struct crisprList *list;
unsigned long long totalChecked = 0;
verbose(1, "# %d number of chromosomes to display\n", slCount(all));
for (list = all; list; list = list->next)
    {
    struct crispr *c = NULL;
    struct crispr *next = NULL;
    verbose(1, "# crisprs count %llu on chrom %s\n", list->crisprCount, list->chrom);
    for (c = list->chromCrisprs; c; c = next)
	{
	int negativeOffset = 0;
        if (c->strand == '-')
	    negativeOffset = 3;
	unsigned long long txStart = c->start + negativeOffset;
	unsigned long long txEnd = c->start+20 + negativeOffset;
	if (bedFH)
	    fprintf(bedFH, "%s\t%llu\t%llu\t%s\t%d\t%c\t%llu\t%llu\t%s\t%d\t%d\t%d\t%d\n", list->chrom, c->start, c->start+23, kmerValToString(c, 3), c->offBy0, c->strand, txStart, txEnd, kmerPAMString(c), c->offBy1, c->offBy2, c->offBy3, c->offBy4);

	verbose(4, "%s\t%llu\t%llu\t%s\t%d\t%c\t%llu\t%llu\t%d\t%d\t%d\t%d\n", list->chrom, c->start, c->start+23, kmerValToString(c, 3), c->offBy0, c->strand, txStart, txEnd, c->offBy1, c->offBy2, c->offBy3, c->offBy4);

        if (c->offBy0)
           verbose(3, "# identical: %d %s:%llu %c\t%s\n", c->offBy0, list->chrom, c->start, c->strand, kmerValToString(c, 3));
	next = c->next;
	++totalChecked;
	}
    }
verbose(1, "# total crisprs displayed: %llu\n", totalChecked);
}	// static void showCounts(struct crisprList *all)

static unsigned long long countOffTargets(char *chrom, struct crispr *c, struct crisprList *all)
/* given one crispr c, scan against all others to find off targets */
{
struct crisprList *listPtr;
unsigned long long totalCompares = 0;
for (listPtr = all; listPtr; listPtr = listPtr->next)
    {
    struct crispr *crisprPtr = NULL;
    struct crispr *next = NULL;
    for (crisprPtr = listPtr->chromCrisprs; crisprPtr; crisprPtr = next)
	{
	/* the XOR will determine differences in two sequences
	 *  the shift right 6 removes the PAM sequence
	 */
        unsigned long long misMatch = (c->sequence ^ crisprPtr->sequence) >> 6;
        if (! misMatch)	/* no misMatch, identical crisprs */
	    {
            c->offBy0 += 1;
            crisprPtr->offBy0 += 1;
//            verbose(4, "# identical: %d %s:%llu %c == %s:%llu %c\t%s == %s\n", c->offBy0, chrom, c->start, c->strand, listPtr->chrom, crisprPtr->start, crisprPtr->strand, kmerValToString(c, 0), kmerValToString(crisprPtr, 0));
	    }
        else
	    {
            if (verboseLevel() > 3)
		{
                /* possible misMatch bit values: 01 10 11
		 *  turn those three values into just: 01
		 */
		misMatch = (misMatch | (misMatch > 1)) & 0x5555555555;
		int bitsOn = _mm_popcnt_u64(misMatch);
                switch(bitsOn)
                    {
                    case 1:
                        c->offBy1 += 1;
                        crisprPtr->offBy1 += 1;
                        break;
                    case 2:
                        c->offBy2 += 1;
                        crisprPtr->offBy2 += 1;
                        break;
                    case 3:
                        c->offBy3 += 1;
                        crisprPtr->offBy3 += 1;
                        break;
                    case 4:
                        c->offBy4 += 1;
                        crisprPtr->offBy4 += 1;
                        break;
                    default: break;
                    }
                }
	    }
        ++totalCompares;
	next = crisprPtr->next;
	}
    }
// verbose(3,"#\ttotal comparisons %llu\n", totalCompares);
return totalCompares;
}	// static void countOffTargets( . . . )

static struct crisprList *scanSequence(char *inFile)
/* scan the given file, return list of crisprs */
{
verbose(1, "#\tscanning sequence file: %s\n", inFile);
dnaUtilOpen();
struct dnaLoad *dl = dnaLoadOpen(inFile);
struct dnaSeq *seq;
struct crisprList *listReturn = NULL;

double perSecond = 0.0;
long elapsedMs = 0;
long scanStart = clock1000();
unsigned long long totalCrisprs = 0;

/* scanning all sequences, setting up crisprs on the listReturn */
while ((seq = dnaLoadNext(dl)) != NULL)
    {
    long startTime = clock1000();
    struct crisprList *oneList = generateKmers(seq);
    slAddHead(&listReturn, oneList);
    elapsedMs = clock1000() - startTime;
    totalCrisprs += oneList->crisprCount;
    perSecond = 0.0;
    if (elapsedMs > 0)
	perSecond = 1000.0 * oneList->crisprCount / elapsedMs;
    verbose(1, "# %s, %d bases, %llu crisprs @ %ld ms -> %.2f crisprs/sec\n",
	seq->name, seq->size, oneList->crisprCount, elapsedMs, perSecond);
    }

elapsedMs = clock1000() - scanStart;
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * totalCrisprs / elapsedMs;
verbose(1, "# %llu total crisprs @ %ld ms -> %.2f crisprs/sec\n", totalCrisprs, elapsedMs, perSecond);

return listReturn;
}	/*	static crisprList *scanSequence(char *inFile)	*/

static struct crisprList *rangeExtraction(struct crisprList *all)
/* given ranges in global rangesHash, construct new list of crisprs that
 *     have any type of overlap with the ranges, also extract those items from
 *         the all list.  Returns new list.
 */
{
struct crisprList *listReturn = NULL;
struct crisprList *listPtr = NULL;
int inputChromCount = slCount(all);
unsigned long long returnListCrisprCount = 0;
unsigned long long examinedCrisprCount = 0;

for (listPtr = all; listPtr; listPtr = listPtr->next)
    {
    struct crispr *c = NULL;
    struct binKeeper *bk = hashFindVal(rangesHash, listPtr->chrom);
    struct crispr *newCrispr = NULL;
    if (bk != NULL)
        {
	struct crispr *prevCrispr = NULL;
	struct crispr *next = NULL;
        for (c = listPtr->chromCrisprs; c; c = next)
            {
	    ++examinedCrisprCount;
            struct binElement *hitList = NULL;
	    next = c->next;	// remember before perhaps lost
            int start = c->start;
            if (c->strand == '-')
                start += 2;
            int end = start + 20;
            hitList = binKeeperFind(bk, start, end);
            if (hitList)
		{
		++returnListCrisprCount;
		if (prevCrispr)	// remove this one from the all list
		    prevCrispr->next = next;
		c->next = NULL;
		slAddHead(&newCrispr, c);	// constructing new list
		prevCrispr = prevCrispr;
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
	newItem->chrom = cloneString(listPtr->chrom);
	newItem->size = listPtr->size;
	newItem->chromCrisprs = newCrispr;
	newItem->crisprCount = slCount(newCrispr);
	slAddHead(&listReturn, newItem);
	}
    }	//	for (listPtr = all; listPtr; listPtr = listPtr->next)

verbose(1, "# range limits, input %d chromosomes, return %d chromosomes\n",
    inputChromCount, slCount(listReturn));
verbose(1, "# range limits, input %llu crisprs, return %llu crisprs\n",
    examinedCrisprCount, returnListCrisprCount);
return listReturn;
}	//	static crisprList *rangeExtraction(crisprList *all)

static struct crisprList *queryVsAll(struct crisprList *query,
    struct crisprList *target)
/* run the query crisprs list against the target list */
{
struct crisprList *listReturn = NULL;

unsigned long long totalCrisprsQuery = 0;
unsigned long long totalCompares = 0;
int targetChrCount = slCount(target);
int queryChrCount = slCount(query);
verbose(1, "# queryVsAll: target %d chromosomes vs. query %d chromosomes\n",
    targetChrCount, queryChrCount);

struct crisprList *listPtr = NULL;
long processStart = clock1000();
long elapsedMs = 0;
double perSecond = 0.0;

for (listPtr = query; listPtr; listPtr = listPtr->next)
    {
    struct crispr *c = NULL;
    totalCrisprsQuery += listPtr->crisprCount;
    verbose(1, "# queryVsAll: crispr count: %llu on %s\n", listPtr->crisprCount, listPtr->chrom);
    int crisprsDone = 0;
    for (c = listPtr->chromCrisprs; c; c = c->next)
        {
        verbose(4, "%s\t%s\t%llu\t%c\n", kmerValToString(c, 0), listPtr->chrom, c->start, c->strand);
        totalCompares += countOffTargets(listPtr->chrom, c, target);
        ++crisprsDone;
elapsedMs = clock1000() - processStart;
perSecond = 0.0;
if (elapsedMs > 0)
perSecond = 1000.0 * crisprsDone / elapsedMs;
verbose(1, "# queryVsAll: processed %d crisprs @ %ld ms -> %.2f crisprs/sec\n", crisprsDone, elapsedMs, perSecond);
verbose(1, "# %d %s:%llu %c %s, offBy0: %d\n", crisprsDone, listPtr->chrom, c->start, c->strand, kmerValToString(c, 0), c->offBy0);
perSecond = 0.0;
if (elapsedMs > 0)
perSecond = 1000.0 * totalCompares / elapsedMs;
verbose(1, "# queryVsAll: processed %d crisprs total compares %llu @ %ld ms -> %.2f crisprs/sec\n", crisprsDone, totalCompares, elapsedMs, perSecond);
        }
    }	// for (listPtr = target; listPtr; listPtr = nextChr)

elapsedMs = clock1000() - processStart;
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * totalCrisprsQuery / elapsedMs;
verbose(1, "# queryVsAll: %llu total crisprs processed @ %ld ms -> %.2f crisprs/sec\n", totalCrisprsQuery, elapsedMs, perSecond);
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * totalCompares / elapsedMs;
verbose(1, "# queryVsAll: %llu total comparisons @ %ld ms -> %.2f crisprs/sec\n", totalCompares, elapsedMs, perSecond);

return listReturn;
}

static struct crisprList *allVsAll(struct crisprList *all,
    struct crisprList *addTo)
/* run the all list against itself, add to 'addTo' if given */
{
struct crisprList *listReturn = NULL;
if (addTo)
    listReturn = addTo;

unsigned long long totalCrisprsIn = 0;
unsigned long long totalCrisprsOut = 0;
unsigned long long totalCompares = 0;
int chrCount = slCount(all);
verbose(1, "# allVsAll: crispr list, scanning %d chromosomes, now processing . . .\n", chrCount);
struct crisprList *listPtr = NULL;
struct crisprList *nextChr = NULL;
long processStart = clock1000();
double perSecond = 0.0;
long elapsedMs = 0;

for (listPtr = all; listPtr; listPtr = nextChr)
    {
    struct crispr *newCrispr = NULL;
    struct crispr *c = NULL;
    totalCrisprsIn += listPtr->crisprCount;	// to verify same in and out
    verbose(1, "# allVsAll: crispr count: %llu on %s\n", listPtr->crisprCount, listPtr->chrom);
    struct crispr *next;
    int crisprsDone = 0;
    for (c = listPtr->chromCrisprs; c; c = next)
        {
        verbose(4, "%s\t%s\t%llu\t%c\n", kmerValToString(c, 0), listPtr->chrom, c->start, c->strand);
        next = c->next;	// remember before lost
        c->next = NULL;	// taking this one out of list
        listPtr->chromCrisprs = next; // this first item removed from list
        if ((verboseLevel() > 2) && next)
            totalCompares += countOffTargets(listPtr->chrom, c, all);
        slAddHead(&newCrispr, c);	// constructing new list
        ++crisprsDone;
elapsedMs = clock1000() - processStart;
perSecond = 0.0;
if (elapsedMs > 0)
perSecond = 1000.0 * crisprsDone / elapsedMs;
verbose(1, "# allVsAll: processed %d crisprs @ %ld ms -> %.2f crisprs/sec\n", crisprsDone, elapsedMs, perSecond);
verbose(1, "# %d %s:%llu %c %s, offBy0: %d\n", crisprsDone, listPtr->chrom, c->start, c->strand, kmerValToString(c, 0), c->offBy0);
perSecond = 0.0;
if (elapsedMs > 0)
perSecond = 1000.0 * totalCompares / elapsedMs;
verbose(1, "# allVsAll: processed %d crisprs total compares %llu @ %ld ms -> %.2f crisprs/sec\n", crisprsDone, totalCompares, elapsedMs, perSecond);
        }
    nextChr = listPtr->next;	// remember before lost
    listPtr->next = NULL;		// taking out of list
    listPtr->chromCrisprs = newCrispr;	// the new crispr list
    listPtr->crisprCount = slCount(newCrispr);	// the new crispr list
    totalCrisprsOut += listPtr->crisprCount;	// to verify correct
    all = nextChr;		// removes this one from list
    slAddHead(&listReturn, listPtr);
    }	// for (listPtr = all; listPtr; listPtr = nextChr)

if (slCount(listReturn) != chrCount)
    verbose(1, "#\tERROR: transferred crispr list chrom count %d != beginning count %d\n", slCount(listReturn), chrCount);
if (totalCrisprsIn != totalCrisprsOut)
    verbose(1, "#\tERROR: initial crispr list count %llu != output count %llu\n", totalCrisprsIn, totalCrisprsOut);
elapsedMs = clock1000() - processStart;
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * totalCrisprsIn / elapsedMs;
verbose(1, "# %llu total crisprs processed @ %ld ms -> %.2f crisprs/sec\n", totalCrisprsIn, elapsedMs, perSecond);
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * totalCompares / elapsedMs;
verbose(1, "# %llu total comparisons @ %ld ms -> %.2f crisprs/sec\n", totalCompares, elapsedMs, perSecond);

return listReturn;
}

static void crisprKmers(char *sequence)
/* crisprKmers - find and annotate crispr sequences. */
{
struct crisprList *allCrisprs = scanSequence(sequence);
struct crisprList *queryCrisprs = NULL;
struct crisprList *countedCrisprs = NULL;

/* processing those crisprs */
if (verboseLevel() > 1)
    {
    /* if ranges have been specified, construct queryList of kmers to measure */
    if (rangesHash)
        {
        queryCrisprs = rangeExtraction(allCrisprs);
        /* first run up query vs. all, no list mucking about */
        countedCrisprs = queryVsAll(queryCrisprs, allCrisprs);
        /* then run up query vs. itself, add to countedCrisprs */
        countedCrisprs = allVsAll(queryCrisprs, countedCrisprs);
        }
    else
        countedCrisprs = allVsAll(allCrisprs, NULL);

    showCounts(countedCrisprs);
    }
}	// static void crisprKmers(char *sequence)

int main(int argc, char *argv[])
/* Process command line, initialize translation arrays and call the process */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

bedFileOut = optionVal("bed", bedFileOut);
ranges = optionVal("ranges", ranges);
if (ranges)
    rangesHash = readRanges(ranges);

initOrderedNtVal();	/* set up orderedNtVal[] */
crisprKmers(argv[1]);
return 0;
}

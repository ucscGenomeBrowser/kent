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
#include "obscure.h"
#include "memalloc.h"

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
  "                  - measured, only those with any overlap to these bed items.\n"
  "   -dumpKmers=<file> - after scan of sequence, output kmers to file\n"
  "                     - process will exit after this, use -loadKmers to continue\n"
  "   -loadKmers=<file> - load kmers from previous scan of sequence from -dumpKmers\n"
  "   NOTE: It is faster to simply scan the sequence to get the system ready\n"
  "         to go.  Reading in the kmer file takes longer than scanning."
  );
}

static char *bedFileOut = NULL;	/* set with -bed=<file> argument */
static char *ranges = NULL;	/* set with -ranges=<file> argument */
static struct hash *rangesHash = NULL;	/* ranges into hash + binkeeper */
static char *dumpKmers = NULL;	/* file name to write out kmers from scan */
static char *loadKmers = NULL;	/* file name to read in kmers from previous scan */

/* Command line validation table. */
static struct optionSpec options[] = {
   {"bed", OPTION_STRING},
   {"ranges", OPTION_STRING},
   {"dumpKmers", OPTION_STRING},
   {"loadKmers", OPTION_STRING},
   {NULL, 0},
};

struct crispr
/* one chromosome set of crisprs */
    {
    struct crispr *next;		/* Next in list. */
    long long sequence;	/* sequence value in 2bit format */
    long long start;		/* chromosome start 0-relative */
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
    long long crisprCount;	/* number of crisprs on this chrom */
    long long *sequence;	/* array of the sequences */
    long long *start;		/* array of the starts */
    char *strand;		/* array of the strand characters */
    int **offBy;		/* offBy[5][n] */
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

static void timingMessage(char *prefix, long long count, char *message,
    long ms, char *units)
{
double perSecond = 0.0;
if (ms > 0)
    perSecond = 1000.0 * count / ms;

verbose(1, "# %s: %lld %s @ %ld ms -> %.2f %s\n", prefix, count, message, ms, perSecond, units);
}


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

static char *kmerPAMStringArray(long long val)
/* return the ASCII string for last three bases in then binary sequence value */
{
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
}	//	static char *kmerValToStringArray(struct crispr *c, int trim)

#ifdef NOT
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
}	//	static char *kmerPAMString(struct crispr *c)
#endif

static char *kmerValToStringArray(long long val, int trim)
/* return ASCII string for binary sequence value */
{
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
}	//	static char *kmerValToStringArray(long long val, int trim)

#ifdef NOT
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
#endif
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

static void copyToArray(struct crisprList *list)
/* copy the crispr list data into arrays */
{
verbose(1, "# copyToArray(%p)\n", (void *)list);
struct crisprList *cl = NULL;
for (cl = list; cl; cl = cl->next)
    {
    verbose(1, "# copy %p %lld crisprs for chrom %s\n", (void *)cl, cl->crisprCount, cl->chrom);
    size_t memSize = cl->crisprCount * sizeof(long long);
    cl->sequence = (long long *)needLargeMem(memSize);
    cl->start = (long long *)needLargeMem(memSize);
    memSize = cl->crisprCount * sizeof(char);
    cl->strand = (char *)needLargeMem(memSize);
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
    #ifdef NOT
    if (2 > i) {
    verbose(1, "# list: %#llx\n", (unsigned long long)cl);
    verbose(1, "# list->sequence: %#llx\n", (unsigned long long)cl->sequence);
    verbose(1, "# list->sequence[%lld]: %#llx\n", i, (unsigned long long)&cl->sequence[i]);
    verbose(1, "# list->strand: %#llx\n", (unsigned long long)cl->strand);
    verbose(1, "# list->strand[%lld]: %#llx\n", i, (unsigned long long)&cl->strand[i]);
    }
    #endif
        cl->sequence[i] = c->sequence;
        cl->start[i] = c->start;
        cl->strand[i++] = c->strand;
        }
    }
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
long long endsGG = (G_BASE << 2) | G_BASE;
long long beginsCC = (long long)((C_BASE << 2) | C_BASE) << 42;
long long reverseMask = (long long)0xf << 42;
verbose(4, "#   endsGG: %032llx\n", endsGG);
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

static void showCountsArray(struct crisprList *all)
/* everything has been scanned and counted, print out all the data from arrays*/
{
FILE *bedFH = NULL;
if (bedFileOut)
    bedFH = mustOpen(bedFileOut, "w");

verbose(1, "#\tprint out all data\n");
struct crisprList *list;
long long totalOut = 0;
verbose(1, "# %d number of chromosomes to display\n", slCount(all));
for (list = all; list; list = list->next)
    {
    verbose(1, "# crisprs count %lld on chrom %s\n", list->crisprCount, list->chrom);
    long long c = 0;
    for (c = 0; c < list->crisprCount; ++c)
        {
	int negativeOffset = 0;
        if (list->strand[c] == '-')
	    negativeOffset = 3;
	long long txStart = list->start[c] + negativeOffset;
	long long txEnd = txStart + 20 + negativeOffset;
        int totalOffs = list->offBy[0][c] + list->offBy[1][c] +
	    list->offBy[2][c] + list->offBy[3][c] + list->offBy[4][c];

        if (0 == totalOffs)
verbose(1, "# PERFECT score %s:%lld %c\t%s\n", list->chrom, list->start[c], list->strand[c], kmerValToStringArray(list->sequence[c], 3));

	if (bedFH)
	    fprintf(bedFH, "%s\t%lld\t%lld\t%s\t%d\t%c\t%lld\t%lld\t%s\t%d\t%d\t%d\t%d\n", list->chrom, list->start[c], list->start[c]+23, kmerValToStringArray(list->sequence[c], 3), list->offBy[0][c], list->strand[c], txStart, txEnd, kmerPAMStringArray(list->sequence[c]), list->offBy[1][c], list->offBy[2][c], list->offBy[3][c], list->offBy[4][c]);

        if (list->offBy[0][c])
           verbose(3, "# array identical: %d %s:%lld %c\t%s\n", list->offBy[0][c], list->chrom, list->start[c], list->strand[c], kmerValToStringArray(list->sequence[c], 3));
	++totalOut;
	}
    }
verbose(1, "# total Array crisprs displayed: %lld\n", totalOut);
if (bedFH)
    carefulClose(&bedFH);
}	// static void showCountsArray(struct crisprList *all)

#ifdef NOT
static void showCounts(struct crisprList *all)
/* everything has been scanned and counted, print out all the data */
{
FILE *bedFH = NULL;
if (bedFileOut)
    bedFH = mustOpen(bedFileOut, "w");

verbose(1, "#\tprint out all data\n");
struct crisprList *list;
long long totalChecked = 0;
verbose(1, "# %d number of chromosomes to display\n", slCount(all));
for (list = all; list; list = list->next)
    {
    struct crispr *c = NULL;
    struct crispr *next = NULL;
    verbose(1, "# crisprs count %lld on chrom %s\n", list->crisprCount, list->chrom);
    for (c = list->chromCrisprs; c; c = next)
	{
	int negativeOffset = 0;
        if (c->strand == '-')
	    negativeOffset = 3;
	long long txStart = c->start + negativeOffset;
	long long txEnd = c->start+20 + negativeOffset;
	if (bedFH)
	    fprintf(bedFH, "%s\t%lld\t%lld\t%s\t%d\t%c\t%lld\t%lld\t%s\t%d\t%d\t%d\t%d\n", list->chrom, c->start, c->start+23, kmerValToString(c, 3), c->offBy0, c->strand, txStart, txEnd, kmerPAMString(c), c->offBy1, c->offBy2, c->offBy3, c->offBy4);

        if (c->offBy0)
           verbose(3, "# identical: %d %s:%lld %c\t%s\n", c->offBy0, list->chrom, c->start, c->strand, kmerValToString(c, 3));
	next = c->next;
	++totalChecked;
	}
    }
verbose(1, "# total crisprs displayed: %lld\n", totalChecked);
if (bedFH)
    carefulClose(&bedFH);
}	// static void showCounts(struct crisprList *all)
#endif

#ifdef NOT
static long long countOffTargets(char *chrom, struct crispr *c, struct crisprList *all)
/* given one crispr c, scan against all others to find off targets */
{
struct crisprList *listPtr;
long long totalCompares = 0;
for (listPtr = all; listPtr; listPtr = listPtr->next)
    {
    struct crispr *crisprPtr = NULL;
    struct crispr *next = NULL;
    for (crisprPtr = listPtr->chromCrisprs; crisprPtr; crisprPtr = next)
	{
	/* the XOR will determine differences in two sequences
	 *  the shift right 6 removes the PAM sequence
	 */
        long long misMatch = (c->sequence ^ crisprPtr->sequence) >> 6;
        if (! misMatch)	/* no misMatch, identical crisprs */
	    {
            c->offBy0 += 1;
            crisprPtr->offBy0 += 1;
	    }
        else
	    {
            if (verboseLevel() > 2)
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
return totalCompares;
}	// static void countOffTargets( . . . )
#endif

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
    timingMessage(seq->name, oneList->crisprCount, "crisprs", elapsedMs, "crisprs/sec");
    }

elapsedMs = clock1000() - scanStart;
timingMessage("scanSequence", totalCrisprs, "total crisprs", elapsedMs, "crisprs/sec");
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
verbose(1, "# range scan all %p, list %p, nextList %p\n",
  (void *)all, (void *)list, (void *)nextList);
    struct crispr *c = NULL;
    struct binKeeper *bk = hashFindVal(rangesHash, list->chrom);
    struct crispr *newCrispr = NULL;
    if (bk != NULL)
        {
	struct crispr *prevCrispr = NULL;
	struct crispr *next = NULL;
        for (c = list->chromCrisprs; c; c = next)
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
	slAddHead(&listReturn, newItem);
verbose(1, "# range scan newCrispr: %lld crisprs on chrom %s\n", 
    newItem->crisprCount, newItem->chrom);
        if (NULL == list->chromCrisprs)	// all have been removed for this chrom
	    {
verbose(1, "# all crisprs on chrom %s have been removed, prevChromList: %p nextList %p\n", list->chrom, (void *)prevChromList, (void *)nextList);
	    if (prevChromList)	// remove it from the chrom list
		prevChromList->next = nextList;
	    else
{
verbose(1, "# removing the first chromList, all %p becomes nextList %p\n", (void *)all, (void *)nextList);
                all = nextList;	// removing the first one
}
	    }
	else
	    verbose(1, "# range scan same chrom list still has %lld crisprs on chrom %s\n",
    (long long)slCount(list->chromCrisprs), list->chrom);
	}
    prevChromList = list;
    }	//	for (list = all; list; list = list->next)

elapsedMs = clock1000() - scanStart;
verbose(1, "# range scan, input %d chromosomes, return %d chromosomes\n",
    inputChromCount, slCount(listReturn));
timingMessage("range scan", examinedCrisprCount, "examined crisprs", elapsedMs, "crisprs/sec");
timingMessage("range scan", returnListCrisprCount, "returned crisprs", elapsedMs, "crisprs/sec");
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

static void queryVsAllArray(struct crisprList *query,
    struct crisprList *target)
/* run the query crisprs list against the target list in the array structures */
{
struct crisprList *qList = NULL;
long long totalCrisprsQuery = 0;
long long totalCompares = 0;

long processStart = clock1000();
long elapsedMs = 0;

for (qList = query; qList; qList = qList->next)
    {
    long long qCount = 0;
    totalCrisprsQuery += qList->crisprCount;
verbose(1, "# queryVsAllArray %lld query crisprs on chrom %s\n", qList->crisprCount, qList->chrom);
    for (qCount = 0; qCount < qList->crisprCount; ++qCount)
	{
        struct crisprList *tList = NULL;
        for (tList = target; tList; tList = tList->next)
            {
            long long tCount = 0;
	    totalCompares += tList->crisprCount;
if (0 == qCount)
    verbose(1, "# queryVsAllArray %lld target crisprs on chrom %s\n", tList->crisprCount, tList->chrom);
            for (tCount = 0; tCount < tList->crisprCount; ++tCount)
                {
                /* the XOR will determine differences in two sequences
                 *  the shift right 6 removes the PAM sequence
                 */
                long long misMatch =
                    (qList->sequence[qCount] ^ tList->sequence[tCount]) >> 6;
                if (misMatch)
                    {
                    /* possible misMatch bit values: 01 10 11
                     *  turn those three values into just: 01
                     */
                    misMatch = (misMatch | (misMatch > 1)) & 0x5555555555;
                    int bitsOn = _mm_popcnt_u64(misMatch);
		    if (bitsOn < 5)
			{
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
            }	//	for (tList = target; tList; tList = tList->next)
	}	//	for (qCount = 0; qCount < qList->crisprCount; ++qCount)
    }	//	for (qList = query; qList; qList = qList->next)
verbose(1, "# done with scanning, check timing\n");
elapsedMs = clock1000() - processStart;
timingMessage("queryVsAllArray", totalCrisprsQuery, "crisprs processed", elapsedMs, "crisprs/sec");
timingMessage("queryVsAllArray", totalCompares, "total comparisons", elapsedMs, "compares/sec");
}	/* static struct crisprList *queryVsAllArray(struct crisprList *query,
	    struct crisprList *target) */

static void allVsAllArray(struct crisprList *all)
/* run this 'all' list vs. itself with arrays */
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
verbose(1, "# allVsAllArray %lld query crisprs on chrom %s\n", qList->crisprCount, qList->chrom);
    /* query runs through all kmers on this chrom */
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
                /* the XOR will determine differences in two sequences
                 *  the shift right 6 removes the PAM sequence
                 */
                long long misMatch =
                    (qList->sequence[qCount] ^ tList->sequence[tCount]) >> 6;
                if (misMatch)
                    {
                    /* possible misMatch bit values: 01 10 11
                     *  turn those three values into just: 01
                     */
                    misMatch = (misMatch | (misMatch > 1)) & 0x5555555555;
                    int bitsOn = _mm_popcnt_u64(misMatch);
		    if (bitsOn < 5)
			{
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
timingMessage("allVsAllArray", totalCrisprsQuery, "crisprs processed", elapsedMs, "crisprs/sec");
timingMessage("allVsAllArray", totalCrisprsCompare, "total comparisons", elapsedMs, "compares/sec");
}	/* static struct crisprList *allVsAllArray(struct crisprList *query,
	    struct crisprList *target) */

#ifdef NOT
static struct crisprList *queryVsAll(struct crisprList *query,
    struct crisprList *target)
/* run the query crisprs list against the target list */
{
struct crisprList *listReturn = NULL;

long long totalCrisprsQuery = 0;
long long totalCompares = 0;
int targetChrCount = slCount(target);
int queryChrCount = slCount(query);
verbose(1, "# queryVsAll: target %d chromosomes vs. query %d chromosomes\n",
    targetChrCount, queryChrCount);

struct crisprList *listPtr = NULL;
long processStart = clock1000();
long elapsedMs = 0;

for (listPtr = query; listPtr; listPtr = listPtr->next)
    {
    struct crispr *c = NULL;
    totalCrisprsQuery += listPtr->crisprCount;
    verbose(1, "# queryVsAll: crispr count: %lld on %s\n", listPtr->crisprCount, listPtr->chrom);
    long long crisprsDone = 0;
    for (c = listPtr->chromCrisprs; c; c = c->next)
        {
        verbose(4, "%s\t%s\t%lld\t%c\n", kmerValToString(c, 0), listPtr->chrom, c->start, c->strand);
        totalCompares += countOffTargets(listPtr->chrom, c, target);
        ++crisprsDone;
	if (crisprsDone < 9)
	    {
	    elapsedMs = clock1000() - processStart;
	    timingMessage("queryVsAll", totalCompares, "total comparisons", elapsedMs, "compares/sec");
	    }
        }
    }	// for (listPtr = target; listPtr; listPtr = nextChr)

elapsedMs = clock1000() - processStart;
timingMessage("queryVsAll", totalCrisprsQuery, "crisprs processed", elapsedMs, "crisprs/sec");
timingMessage("queryVsAll", totalCompares, "total comparisons", elapsedMs, "compares/sec");

return listReturn;
}	/* static struct crisprList *queryVsAll(struct crisprList *query,
	    struct crisprList *target) */


static struct crisprList *allVsAll(struct crisprList *all,
    struct crisprList *addTo)
/* run the all list against itself, add to 'addTo' if given */
{
struct crisprList *listReturn = NULL;
if (addTo)
    listReturn = addTo;

long long totalCrisprsIn = 0;
long long totalCrisprsOut = 0;
long long totalCompares = 0;
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
    verbose(1, "# allVsAll: crispr count: %lld on %s\n", listPtr->crisprCount, listPtr->chrom);
    struct crispr *next;
    int crisprsDone = 0;
    for (c = listPtr->chromCrisprs; c; c = next)
        {
        verbose(4, "%s\t%s\t%lld\t%c\n", kmerValToString(c, 0), listPtr->chrom, c->start, c->strand);
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
verbose(1, "# %d %s:%lld %c %s, offBy0: %d\n", crisprsDone, listPtr->chrom, c->start, c->strand, kmerValToString(c, 0), c->offBy0);
perSecond = 0.0;
if (elapsedMs > 0)
perSecond = 1000.0 * totalCompares / elapsedMs;
verbose(1, "# allVsAll: processed %d crisprs total compares %lld @ %ld ms -> %.2f crisprs/sec\n", crisprsDone, totalCompares, elapsedMs, perSecond);
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
    verbose(1, "#\tERROR: initial crispr list count %lld != output count %lld\n", totalCrisprsIn, totalCrisprsOut);
elapsedMs = clock1000() - processStart;
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * totalCrisprsIn / elapsedMs;
verbose(1, "# %lld total crisprs processed @ %ld ms -> %.2f crisprs/sec\n", totalCrisprsIn, elapsedMs, perSecond);
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * totalCompares / elapsedMs;
verbose(1, "# %lld total comparisons @ %ld ms -> %.2f crisprs/sec\n", totalCompares, elapsedMs, perSecond);

return listReturn;
}	//	static struct crisprList *allVsAll(struct crisprList *all,
			// struct crisprList *addTo)
#endif

static struct crisprList *readKmers(char *fileIn)
/* read in kmer list from 'fileIn', return list structure */
{
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
        oneCrispr->strand = row[2][0];
        oneCrispr->offBy0 = 0;
        oneCrispr->offBy1 = 0;
        oneCrispr->offBy2 = 0;
        oneCrispr->offBy3 = 0;
        oneCrispr->offBy4 = 0;
        slAddHead(&newItem->chromCrisprs, oneCrispr);
	}
    if (verifyCount != newItem->crisprCount)
	errAbort("expecting %lld kmer items at line %d in file %s, found: %lld",
	    newItem->crisprCount, lf->lineIx, fileIn, verifyCount);
    crisprsInput += verifyCount;
    }

lineFileClose(&lf);

long elapsedMs = clock1000() - startMs;
timingMessage("readKmers", crisprsInput, "crisprs read in", elapsedMs, "crisprs/sec");

return listReturn;
}	//	static struct crisprList *readKmers(char *fileIn)

static void writeKmers(struct crisprList *all, char *fileOut)
/* write kmer list 'all' to 'fileOut' */
{
FILE *fh = mustOpen(fileOut, "w");
struct crisprList *list = NULL;
long long crisprsWritten = 0;

long startMs = clock1000();

slReverse(&all);
for (list = all; list; list = list->next)
    {
    fprintf(fh, "%s\t%lld\t%d\n", list->chrom, list->crisprCount, list->size);
    struct crispr *c = NULL;
    slReverse(&list->chromCrisprs);
    for (c = list->chromCrisprs; c; c = c->next)
	{
	fprintf(fh, "%lld\t%lld\t%c\n", c->sequence,
	    c->start, c->strand);
	++crisprsWritten;
	}
    }
carefulClose(&fh);

long elapsedMs = clock1000() - startMs;
timingMessage("writeKmers", crisprsWritten, "crisprs written", elapsedMs, "crisprs/sec");
}	//	static void writeKmers(struct crisprList *all, char *fileOut)

static void crisprKmers(char *sequence)
/* crisprKmers - find and annotate crispr sequences. */
{
struct crisprList *queryCrisprs = NULL;
// struct crisprList *countedCrisprs = NULL;
struct crisprList *allCrisprs = NULL;

if (loadKmers)
    allCrisprs = readKmers(loadKmers);
else
    allCrisprs = scanSequence(sequence);

#ifdef NOT
// timing walking through the linked lists
long startMs = clock1000();
long long kmerCount = 0;
struct crisprList *list = NULL;
for (list = allCrisprs; list; list = list->next)
    kmerCount += slCount(list->chromCrisprs);
long elapsedMs = clock1000() - startMs;
timingMessage("slCount", kmerCount, "kmers counted", elapsedMs, "kmers/sec");
startMs = clock1000();
slReverse(&allCrisprs);
for (list = allCrisprs; list; list = list->next)
    slReverse(&list->chromCrisprs);
elapsedMs = clock1000() - startMs;
timingMessage("slReverse", kmerCount, "kmers reversed", elapsedMs, "kmers/sec");

return;
#endif

if (dumpKmers)
    {
    writeKmers(allCrisprs, dumpKmers);
    return;
    }

/* processing those crisprs */
if (verboseLevel() > 1)
    {
    /* if ranges have been specified, construct queryList of kmers to measure */
    if (rangesHash)
        {
	/* result here is two exclusive sets: query, and allCrisprs
	 *    the query crisprs have been extracted from the allCrisprs */
verbose(1, "# before rangeExtraction, allCrisprs: %p\n", (void *)allCrisprs);
        queryCrisprs = rangeExtraction(& allCrisprs);
verbose(1, "# after rangeExtraction, allCrisprs: %p\n", (void *)allCrisprs);
verbose(1, "# copyToArray(queryCrisprs)\n");
        copyToArray(queryCrisprs);
	if (allCrisprs)	// if there are any left on the all list
	    {
verbose(1, "# copyToArray(allCrisprs)\n");
	    copyToArray(allCrisprs);
            /* first run up query vs. all */
            queryVsAllArray(queryCrisprs, allCrisprs);
	    }
        /* then run up the query vs. itself avoiding self vs. self */
        allVsAllArray(queryCrisprs);
        showCountsArray(queryCrisprs);
verbose(1, "# back from showCountsArray\n");
#ifdef NOT
        countedCrisprs = queryVsAll(queryCrisprs, allCrisprs);
        /* then run up query vs. itself, add to countedCrisprs */
        countedCrisprs = allVsAll(queryCrisprs, countedCrisprs);
#endif
        }
    else
        {
	copyToArray(allCrisprs);
        /* run up all vs. all avoiding self vs. self */
        allVsAllArray(allCrisprs);
        showCountsArray(allCrisprs);
        }
#ifdef NOT
    else
        countedCrisprs = allVsAll(allCrisprs, NULL);
#endif

#ifdef NOT
    showCounts(countedCrisprs);
#endif
    }
verbose(1, "# returning from crisprKmers\n");
}	// static void crisprKmers(char *sequence)

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
ranges = optionVal("ranges", ranges);
if (ranges)
    rangesHash = readRanges(ranges);

initOrderedNtVal();	/* set up orderedNtVal[] */
crisprKmers(argv[1]);
verbose(1, "# returned from crisprKmers(%s}\n", argv[1]);
verbose(1, "# checking heap after crisprKmers\n");
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}

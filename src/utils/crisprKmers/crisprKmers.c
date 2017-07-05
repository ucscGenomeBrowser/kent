/* crisprKmers - annotate crispr sequences. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "portable.h"

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
  "                      print out all crisprs as bed format"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct crispr
/* one chromosome set of crisprs */
    {
    struct crispr *next;		/* Next in list. */
    unsigned long long sequence;	/* sequence value in 2bit format */
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

static char bases[4];  /* for binary to ascii conversion */
static char revBases[4]; /* reverse complement for binary to ascii conversion */
static unsigned long long revValue[4];	/* for reverse complement binary bit values */

#ifdef NOT
// Do not need this, slReverse does this job, perhaps a bit more efficiently
static int slCmpStart(const void *va, const void *vb)
/* Compare slPairs on start value */
{
const struct crispr *a = *((struct crispr **)va);
const struct crispr *b = *((struct crispr **)vb);
unsigned long long aVal = a->start;
unsigned long long bVal = b->start;
if (aVal < bVal)
    return -1;
else if (aVal > bVal)
    return 1;
else
    return 0;
}
#endif

static char *kmerValToString(struct crispr *c, int trim)
/* print out ASCII string for binary sequence value */
{
unsigned long long val = c->sequence;
// char strand = c->strand;
// strand = '+'; // IS STRAND NO LONGER NEEDED ?
static char kmerString[32];
long long twoBitMask = 0x300000000000;
int shiftCount = 44;
int baseCount = 0;

#ifdef NOT
kmerString[baseCount] = 0;
if ('-' == strand)
    {
    twoBitMask = 0x3;
    shiftCount = 0;
    while (shiftCount < 46)
        {
        int base = (val & twoBitMask) >> shiftCount;
        kmerString[baseCount++] = revBases[base];
        twoBitMask <<= 2;
        shiftCount += 2;
        }
    }
else
    {
#endif
    while (twoBitMask && (shiftCount >= (2*trim)))
        {
        int base = (val & twoBitMask) >> shiftCount;
        kmerString[baseCount++] = bases[base];
        twoBitMask >>= 2;
        shiftCount -= 2;
        }
#ifdef NOT
    }
#endif
kmerString[baseCount] = 0;
return kmerString;
}
 
static unsigned long long revComp(unsigned long long val)
/* reverse complement the bit pattern, there is a better way to do this */
{
unsigned long long reverse = 0;
unsigned long long twoBitMask = 0x300000000000;
int leftShift = 0;
while(twoBitMask)
    {
    int base = (val & twoBitMask) >> (44 - leftShift);
    reverse |= revValue[base] << leftShift;
    twoBitMask >>= 2;
    leftShift += 2;
    }
return reverse;
}

static struct crisprList *generateKmers(struct dnaSeq *seq)
{
struct crispr *crisprSet = NULL;
struct crisprList *oneChromList = NULL;
AllocVar(oneChromList);
oneChromList->chrom = cloneString(seq->name);
oneChromList->size = seq->size;
int i;
DNA *dna;
unsigned long long chromPosition = 0;
unsigned long long startGap = 0;
unsigned long long gapCount = 0;
int kmerLength = 0;
long long kmerVal = 0;
long long fortySixBits = 0x3fffffffffff;
long long endsGG = (G_BASE_VAL << 2) | G_BASE_VAL;
long long beginsCC = (long long)((C_BASE_VAL << 2) | C_BASE_VAL) << 42;
long long reverseMask = (long long)0xf << 42;
verbose(4, "#   endsGG: %032llx\n", endsGG);
verbose(4, "# beginsCC: %032llx\n", beginsCC);
verbose(4, "#  46 bits: %032llx\n", fortySixBits);

dna=seq->dna;
for (i=0; i < seq->size; ++i)
    {
    int val;
    val = ntVal[(int)dna[i]];
    switch (val)
	{
	case T_BASE_VAL:	// 0
	case C_BASE_VAL:	// 1
	case A_BASE_VAL:	// 2
	case G_BASE_VAL:	// 3
              kmerVal = fortySixBits & ((kmerVal << 2) | val);
              ++kmerLength;
              if (kmerLength > 22)
                 {
		if (endsGG == (kmerVal & 0xf))
		    {
		    struct crispr *oneCrispr = NULL;
		    AllocVar(oneCrispr);
                    oneCrispr->start = chromPosition - 22;
                    oneCrispr->strand = '+';
                    oneCrispr->sequence = kmerVal;
                    slAddHead(&crisprSet, oneCrispr);
		    }
                 if (beginsCC == (kmerVal & reverseMask))
		    {
		    struct crispr *oneCrispr = NULL;
		    AllocVar(oneCrispr);
                    oneCrispr->start = chromPosition - 22;
                    oneCrispr->strand = '-';
                    oneCrispr->sequence = revComp(kmerVal);
                    slAddHead(&crisprSet, oneCrispr);
		    }
                 }
            break;
        default:
          ++gapCount;
          startGap = chromPosition;
          /* skip all N's == any value not = 0,1,2,3 */
          while ( ((i+1) < seq->size) && (0xfffffffd & ntVal[(int)dna[i+1]]))
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
	}
    ++chromPosition;
    }
// slReverse(&crisprSet);	// save time, order not important at this time
oneChromList->chromCrisprs = crisprSet;
oneChromList->crisprCount = slCount(crisprSet);
return oneChromList;
}	// static struct crisprList *generateKmers(struct dnaSeq *seq)

static void showCounts(struct crisprList *all)
/* everything has been scanned and counted, print out all the data */
{
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
        if (c->strand == '+')
	    verbose(4, "%s\t%llu\t%llu\t%s\t%d\t%c\t%llu\t%llu\t%d\t%d\t%d\t%d\n", list->chrom, c->start, c->start+23, kmerValToString(c, 3), c->offBy0, c->strand, c->start, c->start+20, c->offBy1, c->offBy2, c->offBy3, c->offBy4);
	else
	    verbose(4, "%s\t%llu\t%llu\t%s\t%d\t%c\t%llu\t%llu\t%d\t%d\t%d\t%d\n", list->chrom, c->start, c->start+23, kmerValToString(c, 3), c->offBy0, c->strand, c->start+3, c->start+23, c->offBy1, c->offBy2, c->offBy3, c->offBy4);
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
	/* XOR will determine differences in two sequences */
        unsigned long long misMatch = c->sequence ^ crisprPtr->sequence;
        misMatch &= 0x3fffffffffc0;
        misMatch >>= 6;	/* eliminate *GG from comparison */
        if (! misMatch)	/* no misMatch, identical crisprs */
	    {
            c->offBy0 += 1;
            crisprPtr->offBy0 += 1;
            verbose(4, "# identical: %d %s:%llu %c == %s:%llu %c\t%s == %s\n", c->offBy0, chrom, c->start, c->strand, listPtr->chrom, crisprPtr->start, crisprPtr->strand, kmerValToString(c, 0), kmerValToString(crisprPtr, 0));
	    }
        else
	    {
            if (verboseLevel() > 3)
                {	/* this is not efficient, there are better ways */
                unsigned long long mask = 0x3;
                int count = 0;
                int i;
                for (i = 0; i < 23; ++i)
                    if (misMatch & mask)
                        ++count;
                    mask <<= 2;
                switch(count)
                    {
                    case 1: c->offBy1 += 1; crisprPtr->offBy1 += 1; break;
                    case 2: c->offBy2 += 1; crisprPtr->offBy2 += 1; break;
                    case 3: c->offBy3 += 1; crisprPtr->offBy3 += 1; break;
                    case 4: c->offBy4 += 1; crisprPtr->offBy4 += 1; break;
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

static void crisprKmers(char *sequence)
/* crisprKmers - annotate crispr sequences. */
{
verbose(1, "#\tscanning sequence: %s\n", sequence);
dnaUtilOpen();
struct dnaLoad *dl = dnaLoadOpen(sequence);
struct dnaSeq *seq;
struct crisprList *allCrisprs = NULL;

double perSecond = 0.0;
long elapsedMs = 0;
long scanStart = clock1000();
unsigned long long totalCrisprs = 0;
/* scanning all sequences, setting up crisprs on the allCrisprs list */
while ((seq = dnaLoadNext(dl)) != NULL)
    {
    long startTime = clock1000();
    struct crisprList *oneList = generateKmers(seq);
    slAddHead(&allCrisprs, oneList);
    elapsedMs = clock1000() - startTime;
    totalCrisprs += oneList->crisprCount;
    perSecond = 0.0;
    if (elapsedMs > 0)
	perSecond = 1000.0 * oneList->crisprCount / elapsedMs;
    verbose(1, "# %s, %d bases, %llu crisprs @ %ld ms -> %.2f crisprs/sec\n", seq->name, seq->size, oneList->crisprCount, elapsedMs, perSecond);
    }
elapsedMs = clock1000() - scanStart;
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * totalCrisprs / elapsedMs;
verbose(1, "# %llu total crisprs @ %ld ms -> %.2f crisprs/sec\n", totalCrisprs, elapsedMs, perSecond);

/* processing those crisprs */
if (verboseLevel() > 1)
    {
    struct crisprList *countedCrisprs = NULL;
    unsigned long long totalCrisprsIn = 0;
    unsigned long long totalCrisprsOut = 0;
    unsigned long long totalCompares = 0;
    int chrCount = slCount(allCrisprs);
    verbose(1, "#\tcrispr list, scanning %d chromosomes, now processing . . .\n", chrCount);
    struct crisprList *listPtr;
    struct crisprList *nextChr;
    long processStart = clock1000();
    for (listPtr = allCrisprs; listPtr; listPtr = nextChr)
	{
        struct crispr *newCrispr = NULL;
        struct crispr *c = NULL;
        totalCrisprsIn += listPtr->crisprCount;	// to verify same in and out
        verbose(1, "#\tcrispr count: %llu on %s\n", listPtr->crisprCount, listPtr->chrom);
        struct crispr *next;
        int crisprsDone = 0;
        for (c = listPtr->chromCrisprs; c; c = next)
	    {
	    verbose(4, "%s\t%s\t%llu\t%c\n", kmerValToString(c, 0), listPtr->chrom, c->start, c->strand);
            next = c->next;	// remember before lost
            c->next = NULL;	// taking this one out of list
            listPtr->chromCrisprs = next; // this first item removed from list
            if ((verboseLevel() > 2) && next)
                totalCompares += countOffTargets(listPtr->chrom, c, allCrisprs);
            slAddHead(&newCrispr, c);	// constructing new list
            ++crisprsDone;
elapsedMs = clock1000() - processStart;
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * crisprsDone / elapsedMs;
verbose(1, "# processed %d crisprs @ %ld ms -> %.2f crisprs/sec\n", crisprsDone, elapsedMs, perSecond);
verbose(1, "# %d %s:%llu %c %s, offBy0: %d\n", crisprsDone, listPtr->chrom, c->start, c->strand, kmerValToString(c, 0), c->offBy0);
perSecond = 0.0;
if (elapsedMs > 0)
    perSecond = 1000.0 * totalCompares / elapsedMs;
verbose(1, "# processed %d crisprs total compares %llu @ %ld ms -> %.2f crisprs/sec\n", crisprsDone, totalCompares, elapsedMs, perSecond);
if (crisprsDone > 100)
   exit(255);
	    }
	nextChr = listPtr->next;	// remember before lost
	listPtr->next = NULL;		// taking out of list
	listPtr->chromCrisprs = newCrispr;	// the new crispr list
	listPtr->crisprCount = slCount(newCrispr);	// the new crispr list
        totalCrisprsOut += listPtr->crisprCount;	// to verify correct
        allCrisprs = nextChr;		// removes this one from list
        slAddHead(&countedCrisprs, listPtr);
	}
    if (slCount(countedCrisprs) != chrCount)
	verbose(1, "#\tERROR: transferred crispr list chrom count %d != beginning count %d\n", slCount(countedCrisprs), chrCount);
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
    showCounts(countedCrisprs);
    }
}	// static void crisprKmers(char *sequence)

int main(int argc, char *argv[])
/* Process command line, set up translation arrays and call the process */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
verboseTimeInit();
bases[0] = 'T';
bases[1] = 'C';
bases[2] = 'A';
bases[3] = 'G';
revBases[0] = 'A';
revBases[1] = 'G';
revBases[2] = 'T';
revBases[3] = 'C';
revValue[0] = 2;
revValue[1] = 3;
revValue[2] = 0;
revValue[3] = 1;
crisprKmers(argv[1]);
return 0;
}

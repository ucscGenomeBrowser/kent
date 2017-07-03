/* crisprKmers - annotate crispr sequences. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "crisprKmers - annotate crispr sequences\n"
  "usage:\n"
  "   crisprKmers <sequence>\n"
  "options:\n"
  "   where <sequence> is a .2bit file or .fa fasta sequence\n"
  "   -xxx=XXX\n"
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
slReverse(&crisprSet);	// may not need to do this at this time
oneChromList->chromCrisprs = crisprSet;
return oneChromList;
}	// static struct crisprList *generateKmers(struct dnaSeq *seq)

static void showCounts(struct crisprList *all)
/* everything has been scanned and counted, print out all the data */
{
verbose(1, "# showCounts entered\n");
struct crisprList *list;
unsigned long long totalChecked = 0;
verbose(1, "# chrom count %d\n", slCount(all));
for (list = all; list; list = list->next)
    {
    struct crispr *c = NULL;
    struct crispr *next = NULL;
verbose(1, "# crisprs count %d on chrom %s\n", slCount(list->chromCrisprs), list->chrom);
    for (c = list->chromCrisprs; c; c = next)
	{
        if (c->strand == '+')
	    verbose(1, "%s\t%llu\t%llu\t%s\t%d\t%c\t%llu\t%llu\t%d\t%d\t%d\t%d\n", list->chrom, c->start, c->start+23, kmerValToString(c, 3), c->offBy0, c->strand, c->start, c->start+20, c->offBy1, c->offBy2, c->offBy3, c->offBy4);
	else
	    verbose(1, "%s\t%llu\t%llu\t%s\t%d\t%c\t%llu\t%llu\t%d\t%d\t%d\t%d\n", list->chrom, c->start, c->start+23, kmerValToString(c, 3), c->offBy0, c->strand, c->start+3, c->start+23, c->offBy1, c->offBy2, c->offBy3, c->offBy4);
	++totalChecked;
        if (c->offBy0)
           verbose(1, "# identical: %d %s:%llu %c\t%s\n", c->offBy0, list->chrom, c->start, c->strand, kmerValToString(c, 3));
	next = c->next;
	}
    }
verbose(1, "# total checked: %llu\n", totalChecked);
}	// static void showCounts(struct crisprList *all)

static void countOffTargets(char *chrom, struct crispr *c, struct crisprList *all)
{
struct crisprList *listPtr;
unsigned long long totalCompares = 0;
for (listPtr = all; listPtr; listPtr = listPtr->next)
    {
    struct crispr *crisprPtr = NULL;
    struct crispr *next = NULL;
    for (crisprPtr = listPtr->chromCrisprs; crisprPtr; crisprPtr = next)
	{
        unsigned long long misMatch = c->sequence ^ crisprPtr->sequence;
        misMatch &= 0x3fffffffffc0;
        misMatch >>= 6;
        if (! misMatch)
	    {
            c->offBy0 += 1;
            crisprPtr->offBy0 += 1;
            verbose(1, "# identical: %d %s:%llu %c == %s:%llu %c\t%s == %s\n", c->offBy0, chrom, c->start, c->strand, listPtr->chrom, crisprPtr->start, crisprPtr->strand, kmerValToString(c, 0), kmerValToString(crisprPtr, 0));
	    }
        else
	    {
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
        ++totalCompares;
	next = crisprPtr->next;
	}
    }
}	// static void countOffTargets( . . . )

static void crisprKmers(char *sequence)
/* crisprKmers - annotate crispr sequences. */
{
verbose(1, "#\tscanning sequence: %s\n", sequence);
dnaUtilOpen();
struct dnaLoad *dl = dnaLoadOpen(sequence);
struct dnaSeq *seq;
struct crisprList *allCrisprs = NULL;

while ((seq = dnaLoadNext(dl)) != NULL)
    {
    struct crisprList *oneList = generateKmers(seq);
    slAddHead(&allCrisprs, oneList);
    verboseTime(2, "#\tcrispr kmer scanning on: %s, size: %d\t", seq->name, seq->size);
    }

if (verboseLevel() > 1)
    {
    struct crisprList *countedCrisprs = NULL;
    unsigned long long totalCrisprs = 0;
    int chrCount = slCount(allCrisprs);
    verbose(1, "#\tcrispr list, scanned %d chromosomes, now processing . . .\n", chrCount);
    struct crisprList *listPtr;
    struct crisprList *nextChr;
    for (listPtr = allCrisprs; listPtr; listPtr = nextChr)
	{
        struct crispr *newCrispr = NULL;
        struct crispr *c = NULL;
        int crisprCount = slCount(listPtr->chromCrisprs);
        totalCrisprs += crisprCount;
        verbose(1, "#\tcrispr count: %d on %s\n", crisprCount, listPtr->chrom);
        struct crispr *next;
        for (c = listPtr->chromCrisprs; c; c = next)
	    {
	    verbose (3, "%s\t%s\t%llu\t%c\n", kmerValToString(c, 0), listPtr->chrom, c->start, c->strand);
            next = c->next;	// remember before lost
//            slRemoveEl(&listPtr->chromCrisprs, c);
            c->next = NULL;	// taking out of list
            listPtr->chromCrisprs = next; // first item removed from list
            if (next)
                countOffTargets(listPtr->chrom, c, allCrisprs);
            slAddHead(&newCrispr, c);	// constructing new list
	    }
	nextChr = listPtr->next;	// remember before lost
	listPtr->next = NULL;		// taking out of list
	listPtr->chromCrisprs = newCrispr;	// the new crispr list
        allCrisprs = nextChr;		// removes this one from list
        slAddHead(&countedCrisprs, listPtr);
	}
verbose(1, "#\tcounted chr length: %d\n", slCount(countedCrisprs));
    verboseTime(2, "#\ttotal crispr count: %llu on %d chromosomes\t", totalCrisprs, chrCount);
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

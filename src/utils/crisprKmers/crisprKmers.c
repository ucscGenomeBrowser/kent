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

static char *kmerValToString(long long val, boolean revComp)
{
static char kmerString[32];
long long twoBitMask = 0x300000000000;
int shiftCount = 44;
int baseCount = 0;

kmerString[baseCount] = 0;
if (revComp)
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
    while (twoBitMask)
        {
        int base = (val & twoBitMask) >> shiftCount;;
        kmerString[baseCount++] = bases[base];
        twoBitMask >>= 2;
        shiftCount -= 2;
        }
    }
kmerString[baseCount] = 0;
return kmerString;
}
 
static struct crisprList *scanSeq(struct dnaSeq *seq)
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
//              boolean testA = endsGG == ((long long)kmerVal & endsGG);
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
                    oneCrispr->sequence = kmerVal;
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
oneChromList->chromCrisprs = crisprSet;
return oneChromList;
}

void crisprKmers(char *sequence)
/* crisprKmers - annotate crispr sequences. */
{
verbose(1, "# scanning sequence: %s\n", sequence);
dnaUtilOpen();
struct dnaLoad *dl = dnaLoadOpen(sequence);
struct dnaSeq *seq;
struct crisprList *allCrisprs = NULL;

while ((seq = dnaLoadNext(dl)) != NULL)
    {
    verbose(2, "#\tprocessing: %s, size: %d\n", seq->name, seq->size);
    struct crisprList *oneList = scanSeq(seq);
    slAddHead(&allCrisprs, oneList);
    }
if (verboseLevel() > 2)
    {
    int chrCount = slCount(allCrisprs);
verbose(1, "# printing crispr list, chrCount: %d\n", chrCount);
    struct crisprList *listPtr;
    for (listPtr = allCrisprs; listPtr; listPtr = listPtr->next)
	{
        struct crispr *crisprPtr;
        int crisprCount = slCount(listPtr->chromCrisprs);
        slSort(&listPtr->chromCrisprs, slCmpStart);
verbose(1, "# printing crispr list, crisprCount: %d on %s\n", crisprCount, listPtr->chrom);
        for (crisprPtr = listPtr->chromCrisprs; crisprPtr; crisprPtr = crisprPtr->next)
	    {
            if ('+' == crisprPtr->strand)
		{
	        verbose (3, "%s\t%s\t%llu\t+\n", kmerValToString(crisprPtr->sequence, FALSE), listPtr->chrom, crisprPtr->start);
		}
            else
		{
	        verbose (3, "%s\t%s\t%llu\t-\n", kmerValToString(crisprPtr->sequence, TRUE), listPtr->chrom, crisprPtr->start);
		}
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
bases[0] = 'T';
bases[1] = 'C';
bases[2] = 'A';
bases[3] = 'G';
revBases[0] = 'A';
revBases[1] = 'G';
revBases[2] = 'T';
revBases[3] = 'C';
crisprKmers(argv[1]);
return 0;
}

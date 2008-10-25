/* sufaMake - Make a suffix array file out of input DNA sequences.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "localmem.h"
#include "dnaLoad.h"
#include "dnaseq.h"
#include "sufa.h"

static char const rcsid[] = "$Id: sufaMake.c,v 1.2 2008/10/25 20:45:48 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sufaMake - Make a suffix array file out of input DNA sequences.\n"
  "usage:\n"
  "   sufaMake input1 input2 ... inputN genomeMegs output.sufa\n"
  "where each input is either a fasta file, a nib file, a 2bit file, or a text file\n"
  "containing the names of the above file types one per line, genomeMegs is an estimate\n"
  "of the genome size in megabases (should be an upper bound), and output.sufa is the\n"
  "output file containing the suffix array in a binary format.\n"
  );
}

/* Base values in _alphabetical_ order. Unfortunately the ones in dnautil.h are not.... */
#define A_VAL 0
#define C_VAL 1
#define G_VAL 2
#define T_VAL 3

int baseToVal[256];

static struct optionSpec options[] = {
   {NULL, 0},
};

struct chromInfo
/* Basic info on a chromosome (or contig). */
    {
    struct chromInfo *next;
    char *name;		/* Chromosome/contig name. */
    bits32 size;	/* Chromosome size. */
    bits32 offset;	/* Chromosome offset in total DNA */
    bits32 basesIndexed; /* Bases actually indexed. */
    struct dnaSeq *seq;	/* Chromosome sequence. */
    };

struct sufaOneBaseListy
/* Temporary structure to store information on one base.  This will get compressed to a
 * smaller structure.  The motivation for this is that we can make a list out of these
 * as we go.  When we're done we can compress it into an array. */
    {
    struct sufaOneBaseListy *next;
    bits32 dnaOffset;	/* Offset to start in dna. */
    bits32 nextTwelve;	/* Next twelve bases in binary representation. */
    };

#define sufaSlotCount (1<<(12*2))

bits64 roundUpTo4(bits64 x)
/* Round x up to next multiple of 4 */
{
return (x+3) & (~(3));
}

void zeroPad(FILE *f, int count)
/* Write zeroes to file. */
{
while (--count >= 0)
    fputc(0, f);
}

struct chromInfo *indexChromPass1(struct dnaSeq *seq, bits32 chromOffset, struct lm *lm,
	struct sufaOneBaseListy **listyIndex)
/* Create a sufaOneBaseListy for each base in seq, and hang it in appropriate slot
 * in listyIndex. */
{
int baseIx;
DNA *dna = seq->dna;
int seqSize = seq->size;
int basesIndexed = 0;
int maskTil = 0;

struct chromInfo *chrom;
AllocVar(chrom);
chrom->name = cloneString(seq->name);
chrom->size = seq->size;
chrom->offset = chromOffset;
chrom->seq = seq;

/* Store six bases at a time: two sets of six before and after our middle set of 12 */
bits32 twelve1 = 0, twelve2 = 0;

/* Preload the hex arrays with the first 23 bases. */
for (baseIx=0; baseIx<11; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 12;
    twelve1 <<= 2;
    twelve1 += baseToVal[baseLetter];
    }
for (baseIx=11; baseIx<23; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 12;
    twelve2 <<= 2;
    twelve2 += baseToVal[baseLetter];
    }
    
/* Do the middle part of the sequence where there are no end conditions to consider. */
for (baseIx = 23; baseIx < seqSize; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 12;
    int base = baseToVal[baseLetter];
    twelve2 = (twelve2 << 2) + base;
    twelve1 = (twelve1 << 2) + (twelve2>>12);
    twelve1 &= 0xFFFFF;
    twelve2 &= 0xFFFFF;
    if (baseIx >= maskTil)
	{
	struct sufaOneBaseListy *temp;
	lmAllocVar(lm, temp);
	temp->dnaOffset = baseIx - 23 + chromOffset;
	temp->nextTwelve = twelve2;
	slAddHead(&listyIndex[twelve1], temp);
	++basesIndexed;
	}
    }
chrom->basesIndexed = basesIndexed;
return chrom;
}

char *globalAllDna;	/* Copy of all DNA for sort function. */

static int cmpAfter16(const void *va, const void *vb)
/* Compare to sort, assuming first 16 bases already match. */
{
const struct sufaOneBaseListy *a = *((struct sufaOneBaseListy **)va);
const struct sufaOneBaseListy *b = *((struct sufaOneBaseListy **)vb);
return strcmp(globalAllDna + a->dnaOffset + 16, globalAllDna + b->dnaOffset + 16);
}

void finishAndWriteOneSlot(struct sufaOneBaseListy **listyIndex, int slotIx, DNA *allDna,
	FILE *f)
/* Do additional sorting and write results to file. */
{
struct sufaOneBaseListy *el, *nextEl, *slotList = listyIndex[slotIx];
if (slotList != NULL)
    {
    /* Do in affect a secondary bucket sort on the 13-16th bases. */
    struct sufaOneBaseListy *buckets[256];
    int bucketIx;
    for (bucketIx = 0; bucketIx < ArraySize(buckets); bucketIx += 1)
        buckets[bucketIx] = NULL;
    for (el = slotList; el != NULL; el = nextEl)
        {
	nextEl = el->next;
	bucketIx = (el->nextTwelve >> 16);
	slAddHead(&buckets[bucketIx], el);
	}

    for (bucketIx = 0; bucketIx < ArraySize(buckets); ++bucketIx )
        {
	struct sufaOneBaseListy *bucketList = buckets[bucketIx];
	if (bucketList != NULL)
	    {
	    if (bucketList->next == NULL)
	        writeOne(f, bucketList->dnaOffset);
	    else
	        {
		slSort(&bucketList, cmpAfter16);
		for (el = bucketList; el != NULL; el = el->next)
		    writeOne(f, el->dnaOffset);
		}
	    }
	}
    }
}

void sufaWriteMerged(struct chromInfo *chromList, DNA *allDna,
	struct sufaOneBaseListy **listyIndex, bits64 totalBasesIndexed, char *output)
/* Write out a file that contains a single splix that is the merger of
 * all of the individual splixes in list. */
{
FILE *f = mustOpen(output, "w");

/** Allocate header and fill out easy constant fields. */
struct sufaFileHeader *header;
AllocVar(header);
header->magic = SUFA_MAGIC;
header->majorVersion = SUFA_MAJOR_VERSION;
header->minorVersion = SUFA_MINOR_VERSION;

/* Figure out sizes of names and sequence for each chromosome. */
struct chromInfo *chrom;
bits32 chromNamesSize = 0;
bits64 dnaDiskSize = 1;	/* For initial zero. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
   {
   chromNamesSize += strlen(chrom->name) + 1;
   dnaDiskSize += chrom->size + 1;  /* Include separating zeroes. */
   }

/* Fill in  most of rest of header fields */
header->chromCount = slCount(chromList);
header->chromNamesSize = roundUpTo4(chromNamesSize);
header->basesIndexed = totalBasesIndexed;
header->dnaDiskSize = roundUpTo4(dnaDiskSize);
bits32 chromSizesSize = header->chromCount*sizeof(bits32);

/* Fill out size field last .*/
header->size = sizeof(*header) 			// header
	+ header->chromNamesSize + 		// chromosome names
	+ header->chromCount * sizeof(bits32)	// chromosome sizes
	+ header->dnaDiskSize 			// dna sequence
	+ sizeof(bits32) * totalBasesIndexed;	// suffix array
verbose(1, "%s will be %lld bytes\n", output, header->size);

/* Write header. */
mustWrite(f, header, sizeof(*header));

/* Write chromosome names. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    mustWrite(f, chrom->name, strlen(chrom->name)+1);
zeroPad(f, header->chromNamesSize - chromNamesSize);

/* Write chromosome sizes. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    mustWrite(f, &chrom->size, sizeof(chrom->size));
int chromSizesSizePad = chromSizesSize - header->chromCount * sizeof(bits32);
zeroPad(f, chromSizesSizePad);

/* Write out chromosome DNA and zeros before, between, and after. */
mustWrite(f, allDna, dnaDiskSize);
zeroPad(f, header->dnaDiskSize - dnaDiskSize);
verbose(1, "Wrote %lld bases of DNA in including zero padding\n", header->dnaDiskSize);

/* Calculate and write suffix array. */
int slotCount = sufaSlotCount;
int slotIx;
for (slotIx=0; slotIx < slotCount; ++slotIx)
    finishAndWriteOneSlot(listyIndex, slotIx, allDna, f);
verbose(1, "Wrote %lld suffix array positions\n", totalBasesIndexed);

carefulClose(&f);
}

void sufaMake(int inCount, char *inputs[], char *genomeMegs, char *output)
/* sufaMake - Make a suffix array file out of input DNA sequences.. */
{
/* Make sure that genomeMegs is really a number, and a reasonable number at that. */
bits64 estimatedGenomeSize = sqlUnsigned(genomeMegs)*1024*1024;
bits64 maxGenomeSize = 1024LL*1024*1024*4;
if (estimatedGenomeSize >= maxGenomeSize)
    errAbort("Can only handle genomes up to 4000 meg, sorry.");

/* Fill out baseToVal array - A is already done. */
baseToVal[(int)'C'] = baseToVal[(int)'c'] = C_VAL;
baseToVal[(int)'G'] = baseToVal[(int)'g'] = G_VAL;
baseToVal[(int)'T'] = baseToVal[(int)'t'] = T_VAL;

struct lm *lm = lmInit(0);
struct hash *uniqHash = hashNew(0);
struct chromInfo *chrom, *chromList = NULL;
bits64 chromOffset = 1;	/* Space for zero at beginning. */
DNA *allDna = globalAllDna = needHugeMem(estimatedGenomeSize);
allDna[0] = 0;

struct sufaOneBaseListy **listyIndex;
AllocArray(listyIndex, sufaSlotCount);
bits32 totalBasesIndexed = 0;
int inputIx;
for (inputIx=0; inputIx<inCount; ++inputIx)
    {
    char * input = inputs[inputIx];
    struct dnaLoad *dl = dnaLoadOpen(input);
    struct dnaSeq *seq;
    while ((seq = dnaLoadNext(dl)) != NULL)
	{
	verbose(1, "reading %s with %d bases\n", seq->name, seq->size);
	bits64 currentSize = chromOffset + seq->size + 1;
	if (currentSize > estimatedGenomeSize)
	    errAbort("Too much sequence.  You estimated %lld, comes to %lld\n"
		     "(including one base pad before and after each chromosome.)", 
		     estimatedGenomeSize, currentSize);
	hashAddUnique(uniqHash, seq->name, NULL);
	chrom = indexChromPass1(seq, chromOffset, lm, listyIndex);
	memcpy(allDna + chromOffset, seq->dna, seq->size + 1);
	toUpperN(allDna + chromOffset, seq->size);
	chromOffset = currentSize;
	totalBasesIndexed += chrom->basesIndexed;
	slAddHead(&chromList, chrom);
	dnaSeqFree(&seq);
	}
    dnaLoadClose(&dl);
    }
verbose(1, "Indexed %lld bases\n", (long long)totalBasesIndexed);
slReverse(&chromList);
sufaWriteMerged(chromList, allDna, listyIndex, totalBasesIndexed, output);
lmCleanup(&lm);
freez(&listyIndex);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 4)
    usage();
dnaUtilOpen();
sufaMake(argc-3, argv+1, argv[argc-2], argv[argc-1]);
return 0;
}

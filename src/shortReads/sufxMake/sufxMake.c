/* sufxMake - Make a suffix array file out of input DNA sequences.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "dnaLoad.h"
#include "dnaseq.h"
#include "sufx.h"

static char const rcsid[] = "$Id: sufxMake.c,v 1.3 2008/10/26 20:09:03 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sufxMake - Make a suffix array file out of input DNA sequences.\n"
  "usage:\n"
  "   sufxMake input1 input2 ... inputN genomeMegs output.sufx\n"
  "where each input is either a fasta file, a nib file, a 2bit file, or a text file\n"
  "containing the names of the above file types one per line, genomeMegs is an estimate\n"
  "of the genome size in megabases (should be an upper bound), and output.sufx is the\n"
  "output file containing the suffix array in a binary format.\n"
  );
}

/* Base values in _alphabetical_ order. Unfortunately the ones in dnautil.h are not.... */
#define SUFX_A 0
#define SUFX_C 1
#define SUFX_G 2
#define SUFX_T 3

/* Hex conversions to assist debugging:
 * 0=AA 1=AC 2=AG 3=AT 4=CA 5=CC 6=CG 7=CT
 * 8=GA 9=GC A=GG B=GT C=TA D=TC E=TG F=TT
 */

/* Table to convert letters to one of the above values. */
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


#define sufxSlotCount (1<<(12*2))

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

void dumpDnaPairs(DNA *s, int size, FILE *f)
{
int i;
for (i=0; i<size; i+=2)
    fprintf(f, "%c%c ", s[i], s[i+1]);
}

struct chromInfo *indexChromPass1(struct dnaSeq *seq, bits32 chromOffset, 
	bits32 *offsetArray, bits32 *listArray, bits32 *twelvemerIndex)
/* Create a sufxOneBaseListy for each base in seq, and hang it in appropriate slot
 * in listyIndex. */
{
int baseIx;
DNA *dna = seq->dna;
int seqSize = seq->size;
int maskTil = 0;

struct chromInfo *chrom;
AllocVar(chrom);
chrom->name = cloneString(seq->name);
chrom->size = seq->size;
chrom->offset = chromOffset;
chrom->seq = seq;

/* Preload the twelvemer with the first 11 bases. */
bits32 twelve = 0;
for (baseIx=0; baseIx<11; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 12;
    twelve <<= 2;
    twelve += baseToVal[baseLetter];
    }
    
/* Do the middle part of the sequence where there are no end conditions to consider. */
int freePos = chromOffset;
for (baseIx = 12; baseIx < seqSize; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 12;
    twelve = (twelve << 2) + baseToVal[baseLetter];
    twelve &= 0xFFFFFF;
    if (baseIx >= maskTil)
	{
	offsetArray[freePos] = baseIx - 11 + chromOffset;
	listArray[freePos] = twelvemerIndex[twelve];
	twelvemerIndex[twelve] = freePos;
	++freePos;
	}
    }
chrom->basesIndexed = freePos - chromOffset;
return chrom;
}

char *globalAllDna;	/* Copy of all DNA for sort function. */

static int cmpAfter16(const void *va, const void *vb)
/* Compare to sort, assuming first 16 bases already match. */
{
bits32 a = *((bits32 *)va);
bits32 b = *((bits32 *)vb);
return strcmp(globalAllDna + a + 16, globalAllDna + b + 16);
}

static int binary4(DNA *dna)
/* Return next 4 bases in binary format.  Return -1 if there is an N. */
{
bits32 packedDna = 0;
int i;
for (i=0; i<4; ++i)
    {
    int baseLetter = dna[i];
    if (baseLetter == 'N')
        return -1;
    packedDna <<= 2;
    packedDna += baseToVal[baseLetter];
    }
return packedDna;
}

void sortAndWriteOffsets(bits32 firstIx, bits32 *offsetArray, bits32 *listArray, 
	DNA *allDna, FILE *f)
/* Set up and do a qsort on list that starts at firstIx.  Write result to file. */
{
/* Count up length of list. */
int listSize = 0;
bits32 listIx;
for (listIx = firstIx; listIx != 0; listIx = listArray[listIx])
    ++listSize;

/* Get an array to hold all offsets in list, hopefully a small one on stack,
 * but if need be a big one on heap */
bits32 smallArray[32], *bigArray = NULL, *sortArray;
if (listSize <= ArraySize(smallArray))
    sortArray = smallArray;
else
    AllocArray(sortArray, listSize);

/* Copy list to array for sorting. */
int sortIx;
for (sortIx=0, listIx=firstIx; listIx != 0; ++sortIx, listIx=listArray[listIx])
    sortArray[sortIx] = offsetArray[listIx];

/* Do the qsort.  I hope I got the cmp function right! */
qsort(sortArray, listSize, sizeof(sortArray[0]), cmpAfter16);

#ifdef DEBUG
    {
    uglyf("Sorting %d elements\n", listSize);
    int i;
    int size = min(listSize, 10);
    for (i=0; i<size; ++i)
        {
	int offset = sortArray[i];
	char *dna = allDna + offset;
	mustWrite(uglyOut, dna, 50);
	uglyf("\n");
	}
    }
#endif /* DEBUG */

/* Write out sorted result. */
mustWrite(f, sortArray, listSize * sizeof(bits32));

/* Clean up. */
if (bigArray)
    freeMem(bigArray);
}


void finishAndWriteOneSlot(bits32 *offsetArray, bits32 *listArray, bits32 *twelvemerIndex,
	int slotIx, DNA *allDna, FILE *f)
/* Do additional sorting and write results to file. */
{
bits32 elIx, nextElIx, slotFirstIx = twelvemerIndex[slotIx];
if (slotFirstIx != 0)
    {
    /* Do in affect a secondary bucket sort on the 13-16th bases. */
    bits32 buckets[256];
    int bucketIx;
    for (bucketIx = 0; bucketIx < ArraySize(buckets); bucketIx += 1)
        buckets[bucketIx] = 0;
    for (elIx = slotFirstIx; elIx != 0; elIx = nextElIx)
        {
	nextElIx = listArray[elIx];
	int bucketIx = binary4(allDna + offsetArray[elIx] + 12);
	if (bucketIx >= 0)
	    {
	    listArray[elIx] = buckets[bucketIx];
	    buckets[bucketIx] = elIx;
	    }
	}

    /* Do final sorting within buckets. */
    for (bucketIx = 0; bucketIx < ArraySize(buckets); ++bucketIx )
        {
	bits32 firstIx = buckets[bucketIx];
	if (firstIx != 0)
	    {
	    int secondIx = listArray[firstIx];
	    if (secondIx == 0)
	        {
		/* Special case for size one list, there are lots of these! */
	        writeOne(f, offsetArray[firstIx]);
		}
	    else
	        {
		if (listArray[secondIx] == 0)
		    {
		    bits32 firstOffset = offsetArray[firstIx];
		    bits32 secondOffset = offsetArray[secondIx];
		    /* Special case for size two list.  There are still quite a few of these. */
		    if (strcmp(allDna+firstOffset, allDna+secondOffset) < 0)
		        {
			writeOne(f, secondOffset);
			writeOne(f, firstOffset);
			}
		    else
		        {
			writeOne(f, firstOffset);
			writeOne(f, secondOffset);
			}
		    }
		else
		    {
		    /* Three or more - do it the hard way... */
		    sortAndWriteOffsets(firstIx, offsetArray, listArray, allDna, f);
		    }
		}
#ifdef SOON
	        {
		slSort(&bucketList, cmpAfter16);
		for (el = bucketList; el != NULL; el = el->next)
		    writeOne(f, el->dnaOffset);
		}
#endif /* SOON */
	    }
	}
    }
}

void sufxWriteMerged(struct chromInfo *chromList, DNA *allDna,
	bits32 *offsetArray, bits32 *listArray, bits32 *twelvemerIndex,
	bits64 totalBasesIndexed, char *output)
/* Write out a file that contains a single splix that is the merger of
 * all of the individual splixes in list. */
{
FILE *f = mustOpen(output, "w");

/** Allocate header and fill out easy constant fields. */
struct sufxFileHeader *header;
AllocVar(header);
#define SUFA_MAGIC 0x6727B283	/* Magic number at start of SUFA file */
header->magic = SUFA_MAGIC;	/* TODO - change to SUFX_MAGIC after refactoring. */
header->majorVersion = SUFX_MAJOR_VERSION;
header->minorVersion = SUFX_MINOR_VERSION;

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
header->arraySize = totalBasesIndexed;
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
int slotCount = sufxSlotCount;
int slotIx;
for (slotIx=0; slotIx < slotCount; ++slotIx)
    finishAndWriteOneSlot(offsetArray, listArray, twelvemerIndex, slotIx, allDna, f);
verbose(1, "Wrote %lld suffix array positions\n", totalBasesIndexed);

carefulClose(&f);
}

void sufxMake(int inCount, char *inputs[], char *genomeMegs, char *output)
/* sufxMake - Make a suffix array file out of input DNA sequences.. */
{
/* Make sure that genomeMegs is really a number, and a reasonable number at that. */
bits64 estimatedGenomeSize = sqlUnsigned(genomeMegs)*1024*1024;
bits64 maxGenomeSize = 1024LL*1024*1024*4;
if (estimatedGenomeSize >= maxGenomeSize)
    errAbort("Can only handle genomes up to 4000 meg, sorry.");

/* Fill out baseToVal array - A is already done. */
baseToVal[(int)'C'] = baseToVal[(int)'c'] = SUFX_C;
baseToVal[(int)'G'] = baseToVal[(int)'g'] = SUFX_G;
baseToVal[(int)'T'] = baseToVal[(int)'t'] = SUFX_T;

struct hash *uniqHash = hashNew(0);
struct chromInfo *chrom, *chromList = NULL;
bits64 chromOffset = 1;	/* Space for zero at beginning. */
DNA *allDna = globalAllDna = needHugeMem(estimatedGenomeSize);
allDna[0] = 0;

/* Allocate index array, and offset and list arrays. */
bits32 *twelvemerIndex;
AllocArray(twelvemerIndex, sufxSlotCount);

/* Where normally we'd keep some sort of structure with a next element to form a list
 * of matching positions in each slot of our index,  to conserve memory we'll do this
 * with two parallel arrays.  Because we're such cheapskates in terms of memory we'll
 * (and still using 9*genomeSize bytes of RAM) we'll use these arrays for two different
 * purposes.   
 *     In the first phase they will together be used to form linked lists of
 * offsets, and the twelvemer index will point to the first item in each list.  In this
 * phase the offsetArray contains offsets into the allDna structure, and the listArray
 * contains the next pointers for the list.  After the first phase we write out the
 * suffix array to disk.
 *     In the second phase we read the suffix array back into the offsetArray, and
 * use the listArray for the traverseArray.  We write out the traverse array to finish
 * things up. */

bits32 *offsetArray, *suffixArray;
suffixArray = AllocArray(offsetArray, estimatedGenomeSize);
bits32 *listArray, *traverseArray;
traverseArray = AllocArray(listArray, estimatedGenomeSize);

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
	toUpperN(seq->dna, seq->size);
	bits64 currentSize = chromOffset + seq->size + 1;
	if (currentSize > estimatedGenomeSize)
	    errAbort("Too much sequence.  You estimated %lld, comes to %lld\n"
		     "(including one base pad before and after each chromosome.)", 
		     estimatedGenomeSize, currentSize);
	hashAddUnique(uniqHash, seq->name, NULL);
	chrom = indexChromPass1(seq, chromOffset, offsetArray, listArray, twelvemerIndex);
	memcpy(allDna + chromOffset, seq->dna, seq->size + 1);
	chromOffset = currentSize;
	totalBasesIndexed += chrom->basesIndexed;
	slAddHead(&chromList, chrom);
	dnaSeqFree(&seq);
	}
    dnaLoadClose(&dl);
    }
verbose(1, "Indexed %lld bases\n", (long long)totalBasesIndexed);
slReverse(&chromList);
sufxWriteMerged(chromList, allDna, offsetArray, listArray, twelvemerIndex,
	totalBasesIndexed, output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 4)
    usage();
dnaUtilOpen();
sufxMake(argc-3, argv+1, argv[argc-2], argv[argc-1]);
return 0;
}

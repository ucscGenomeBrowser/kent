/* sufxMake - Make a suffix array file out of input DNA sequences.. */
/* Copyright Jim Kent 2008 all rights reserved. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "dnaLoad.h"
#include "dnaseq.h"
#include "sufx.h"

static char const rcsid[] = "$Id: sufxMake.c,v 1.12 2008/10/29 02:01:00 kent Exp $";

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
bits32 baseIx;
DNA *dna = seq->dna;
bits32 seqSize = seq->size;
bits32 maskTil = 0;

struct chromInfo *chrom;
AllocVar(chrom);
chrom->name = cloneString(seq->name);
chrom->size = seq->size;
chrom->offset = chromOffset;
chrom->seq = seq;

verbose(2, "   start short initial loop\n");
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

verbose(2, "   start main loop\n");
/* Do the middle part of the sequence where there are no end conditions to consider. */
bits32 freePos = chromOffset;
for (baseIx = 11; baseIx < seqSize; ++baseIx)
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
verbose(2, "   end main loop\n");
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

/* Write out sorted result. */
mustWrite(f, sortArray, listSize * sizeof(bits32));

/* Clean up. */
if (bigArray)
    freeMem(bigArray);
}


bits64 finishAndWriteOneSlot(bits32 *offsetArray, bits32 *listArray, bits32 *twelvemerIndex,
	bits32 slotIx, DNA *allDna, FILE *f)
/* Do additional sorting and write results to file.  Return amount actually written. */
{
bits64 basesIndexed = 0;
bits32 elIx, nextElIx, slotFirstIx = twelvemerIndex[slotIx];
if (slotFirstIx != 0)
    {
    /* Do in affect a secondary bucket sort on the 13-16th bases. */
    bits32 buckets[256];
    bits32 bucketIx;
    for (bucketIx = 0; bucketIx < ArraySize(buckets); bucketIx += 1)
        buckets[bucketIx] = 0;
    for (elIx = slotFirstIx; elIx != 0; elIx = nextElIx)
        {
	nextElIx = listArray[elIx];
	int bucketIx = binary4(allDna + offsetArray[elIx] + 12U);
	if (bucketIx >= 0)
	    {
	    listArray[elIx] = buckets[bucketIx];
	    buckets[bucketIx] = elIx;
	    ++basesIndexed;
	    }
	}

    /* Do final sorting within buckets. */
    for (bucketIx = 0; bucketIx < ArraySize(buckets); ++bucketIx )
        {
	bits32 firstIx = buckets[bucketIx];
	if (firstIx != 0)
	    {
	    bits32 secondIx = listArray[firstIx];
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
	    }
	}
    }
return basesIndexed;
}

void sufxFillInTraverseArray(char *dna, bits32 *suffixArray, bits64 arraySize, bits32 *traverseArray)
/* Fill in the bits that will help us traverse the array as if it were a tree. */
{
int depth = 0;
int stackSize = 4*1024;
int *stack;
AllocArray(stack, stackSize);
bits64 i;
for (i=0; i<arraySize; ++i)
    {
    char *curDna = dna + suffixArray[i];
    int d;
    for (d = 0; d<depth; ++d)
        {
	int prevIx = stack[d];
	char *prevDna = dna + suffixArray[prevIx];
	if (curDna[d] != prevDna[d])
	    {
	    int stackIx;
	    for (stackIx=d; stackIx<depth; ++stackIx)
	        {
		prevIx = stack[stackIx];
		traverseArray[prevIx] = i - prevIx;
		}
	    depth = d;
	    break;
	    }
	}
    if (depth >= stackSize)
        errAbort("Stack overflow, depth >= %d", stackSize);
    stack[depth] = i;
    depth += 1;
    }
/* Do final clear out of stack */
int stackIx;
for (stackIx=0; stackIx < depth; ++stackIx)
    {
    int prevIx = stack[stackIx];
    traverseArray[prevIx] = arraySize - prevIx;
    }
}


void sufxWriteMerged(struct chromInfo *chromList, DNA *allDna,
	bits32 *offsetArray, bits32 *listArray, bits32 *twelvemerIndex, char *output)
/* Write out a file that contains a single splix that is the merger of
 * all of the individual splixes in list.   As a side effect will replace
 * offsetArray with suffix array and listArray with traverse array */
{
FILE *f = mustOpen(output, "w+");

/** Allocate header and fill out easy constant fields. */
struct sufxFileHeader *header;
AllocVar(header);
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
header->dnaDiskSize = roundUpTo4(dnaDiskSize);
bits32 chromSizesSize = header->chromCount*sizeof(bits32);

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
verbose(1, "Wrote %lld bases of DNA including zero padding\n", header->dnaDiskSize);

/* Calculate and write suffix array. */
bits64 arraySize = 0;
off_t suffixArrayFileOffset = ftello(f);
int slotCount = sufxSlotCount;
int slotIx;
for (slotIx=0; slotIx < slotCount; ++slotIx)
    arraySize += finishAndWriteOneSlot(offsetArray, listArray, twelvemerIndex, slotIx, allDna, f);
verbose(1, "Wrote %lld suffix array positions\n", arraySize);

/* Now we're done with the offsetArray and listArray buffers, so use them for the
 * next phase. */
bits32 *suffixArray = offsetArray;
offsetArray = NULL;	/* Help make some errors more obvious */
bits32 *traverseArray = listArray;
listArray = NULL;	/* Help make some errors more obvious */

/* Read the suffix array back from the file. */
fseeko(f, suffixArrayFileOffset, SEEK_SET);
mustRead(f, suffixArray, arraySize*sizeof(bits32));
verbose(1, "Read suffix array back in\n");

/* Calculate traverse array */
memset(traverseArray, 0, arraySize*sizeof(bits32));
sufxFillInTraverseArray(allDna, suffixArray, arraySize, traverseArray);
verbose(1, "Filled in traverseArray\n");

/* Write out traverse array. */
mustWrite(f, traverseArray, arraySize*sizeof(bits32));
verbose(1, "Wrote out traverseArray\n");

/* Update a few fields in header, and go back and write it out again with
 * the correct magic number to indicate it's complete. */
header->magic = SUFX_MAGIC;
header->arraySize = arraySize;
header->size = sizeof(*header) 			// header
	+ header->chromNamesSize + 		// chromosome names
	+ header->chromCount * sizeof(bits32)	// chromosome sizes
	+ header->dnaDiskSize 			// dna sequence
	+ sizeof(bits32) * arraySize	 	// suffix array
	+ sizeof(bits32) * arraySize;  		// traverse array

rewind(f);
mustWrite(f, header, sizeof(*header));
carefulClose(&f);
verbose(1, "Completed %s is %lld bytes\n", output, header->size);
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

bits32 *offsetArray = needHugeMem(estimatedGenomeSize * sizeof(bits32));
bits32 *listArray = needHugeZeroedMem(estimatedGenomeSize * sizeof(bits32));;
verbose(1, "Allocated buffers: %lld bytes total\n", 9LL*estimatedGenomeSize + sufxSlotCount*4);

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
	verbose(2, "  About to do first pass index\n");
	chrom = indexChromPass1(seq, chromOffset, offsetArray, listArray, twelvemerIndex);
	verbose(2, "  Done first pass index\n");
	memcpy(allDna + chromOffset, seq->dna, seq->size + 1);
	verbose(2, "  Done copy to allDna + %lld, size %d, totalSize %lld\n", chromOffset, seq->size+1, currentSize);
	chromOffset = currentSize;
	slAddHead(&chromList, chrom);
	dnaSeqFree(&seq);
	}
    dnaLoadClose(&dl);
    }
verbose(1, "Done big bucket sort\n");
slReverse(&chromList);
sufxWriteMerged(chromList, allDna, offsetArray, listArray, twelvemerIndex, output);
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

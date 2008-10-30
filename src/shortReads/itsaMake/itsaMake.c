/* itsaMake - Create indexed traversable suffix array for fast searches of genome.. */
/* Copyright Jim Kent 2008 all rights reserved. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "dnaLoad.h"
#include "dnaseq.h"
#include "itsa.h"


static char const rcsid[] = "$Id: itsaMake.c,v 1.2 2008/10/30 04:54:22 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "itsaMake - Create indexed traversable suffix array for fast searches of genome.\n"
  "usage:\n"
  "   itsaMake input1 input2 ... inputN output.itsa\n"
  "where each input is either a fasta file, a nib file, a 2bit file, or a text file\n"
  "containing the names of the above file types one per line and output.itsa is the\n"
  "output file containing the suffix array in a binary format.\n"
  );
}

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
    };


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

void indexChromPass1(struct chromInfo *chrom, DNA *allDna,  
	bits32 *offsetArray, bits32 *listArray, bits32 *index13)
/* Create a itsaOneBaseListy for each base in seq, and hang it in appropriate slot
 * in listyIndex. */
{
bits32 baseIx;
bits32 seqSize = chrom->size;
bits32 chromOffset = chrom->offset;
DNA *dna = allDna + chromOffset;
bits32 maskTil = 0;

verbose(2, "   start short initial loop\n");
/* Preload the 13mer with the first 12 bases. */
bits32 thirteen = 0;
for (baseIx=0; baseIx<12; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 13;
    thirteen <<= 2;
    thirteen += baseToVal[baseLetter];
    }

verbose(2, "   start main loop\n");
/* Do the middle part of the sequence where there are no end conditions to consider. */
bits32 freePos = chromOffset;
for (baseIx = 12; baseIx < seqSize; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 13;
    thirteen = (thirteen << 2) + baseToVal[baseLetter];
    thirteen &= 0x3FFFFFF;
    if (baseIx >= maskTil)
	{
	offsetArray[freePos] = baseIx - 12 + chromOffset;
	listArray[freePos] = index13[thirteen];
	index13[thirteen] = freePos;
	++freePos;
	}
    }
verbose(2, "   end main loop\n");
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


bits64 finishAndWriteOneSlot(bits32 *offsetArray, bits32 *listArray, 
	bits32 slotFirstIx, DNA *allDna, FILE *f)
/* Do additional sorting and write results to file.  Return amount actually written.  */
{
bits64 basesIndexed = 0;
bits32 elIx, nextElIx;
if (slotFirstIx != 0)
    {
    /* Do in affect a secondary bucket sort on the 14-17th bases. */
    bits32 buckets[256];
    bits32 bucketIx;
    for (bucketIx = 0; bucketIx < ArraySize(buckets); bucketIx += 1)
        buckets[bucketIx] = 0;
    for (elIx = slotFirstIx; elIx != 0; elIx = nextElIx)
        {
	nextElIx = listArray[elIx];
	int bucketIx = binary4(allDna + offsetArray[elIx] + 13U);
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

void itsaFillInTraverseArray(char *dna, bits32 *suffixArray, bits32 arraySize, 
	bits32 *traverseArray, UBYTE *cursorArray)
/* Fill in the bits that will help us traverse the array as if it were a tree. */
{
int depth = 0;
int stackSize = 4*1024;
int *stack;
AllocArray(stack, stackSize);
bits32 i;
for (i=0; i<arraySize; ++i)
    {
    char *curDna = dna + suffixArray[i];
    int d;
    for (d = 0; d<depth; ++d)
        {
	bits32 prevIx = stack[d];
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
    cursorArray[i] = depth;  // May overflow. That's ok. We just care about indexed ones which are < 13.
    depth += 1;
    }
/* Do final clear out of stack */
int stackIx;
for (stackIx=0; stackIx < depth; ++stackIx)
    {
    bits32 prevIx = stack[stackIx];
    traverseArray[prevIx] = arraySize - prevIx;
    }
}

void itsaWriteMerged(struct chromInfo *chromList, DNA *allDna,
	bits32 *offsetArray, bits32 *listArray, bits32 *index13, char *output)
/* Write out a file that contains a single splix that is the merger of
 * all of the individual splixes in list.   As a side effect will replace
 * offsetArray with suffix array and listArray with traverse array */
{
FILE *f = mustOpen(output, "w+");

/** Allocate header and fill out easy constant fields. */
struct itsaFileHeader *header;
AllocVar(header);
header->majorVersion = ITSA_MAJOR_VERSION;
header->minorVersion = ITSA_MINOR_VERSION;

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
uglyTime("Wrote %lld bases of DNA including zero padding", header->dnaDiskSize);

/* Calculate and write suffix array. Convert index13 to index of array as opposed to index
 * of sequence. */
bits64 arraySize = 0;
off_t suffixArrayFileOffset = ftello(f);
int slotCount = itsaSlotCount;
int slotIx;
for (slotIx=0; slotIx < slotCount; ++slotIx)
    {
    int slotSize = finishAndWriteOneSlot(offsetArray, listArray, index13[slotIx], allDna, f);
    /* Convert index13 to hold the position in the suffix array where the first thing matching
     * the corresponding 13-base prefix is found. */
    if (slotSize != 0)
        index13[slotIx] = arraySize+1;	/* The +1 is so we can keep 0 for not found. */
    else
        index13[slotIx] = 0;
    arraySize += slotSize;
    if ((slotIx % 5000000 == 0) && slotIx != 0)
        verbose(1, "Wrote slot %d of %d\n", slotIx, slotCount);
    }
uglyTime("Wrote %lld suffix array positions", arraySize);

/* Now we're done with the offsetArray and listArray buffers, so use them for the
 * next phase. */
bits32 *suffixArray = offsetArray;
offsetArray = NULL;	/* Help make some errors more obvious */
bits32 *traverseArray = listArray;
listArray = NULL;	/* Help make some errors more obvious */

/* Read the suffix array back from the file. */
fseeko(f, suffixArrayFileOffset, SEEK_SET);
mustRead(f, suffixArray, arraySize*sizeof(bits32));
uglyTime("Read suffix array back in");

/* Calculate traverse array and cursor arrays */
memset(traverseArray, 0, arraySize*sizeof(bits32));
UBYTE *cursorArray = needHugeMem(arraySize);
itsaFillInTraverseArray(allDna, suffixArray, arraySize, traverseArray, cursorArray);
uglyTime("Filled in traverseArray");

/* Write out traverse array. */
mustWrite(f, traverseArray, arraySize*sizeof(bits32));
uglyTime("Wrote out traverseArray");

/* Write out 13-mer index. */
mustWrite(f, index13, itsaSlotCount*sizeof(bits32));
uglyTime("Wrote out index13");

/* Write out bits of cursor array corresponding to index. */
for (slotIx=0; slotIx<itsaSlotCount; ++slotIx)
    {
    bits32 indexPos = index13[slotIx];
    if (indexPos == 0)
       fputc(0, f);
    else
       fputc(cursorArray[indexPos-1], f);
    }
uglyTime("Wrote out cursors13");

/* Update a few fields in header, and go back and write it out again with
 * the correct magic number to indicate it's complete. */
header->magic = ITSA_MAGIC;
header->arraySize = arraySize;
header->size = sizeof(*header) 			// header
	+ header->chromNamesSize + 		// chromosome names
	+ header->chromCount * sizeof(bits32)	// chromosome sizes
	+ header->dnaDiskSize 			// dna sequence
	+ sizeof(bits32) * arraySize	 	// suffix array
	+ sizeof(bits32) * arraySize   		// traverse array
	+ sizeof(bits32) * itsaSlotCount 	// index13
	+ sizeof(UBYTE) * itsaSlotCount;	// cursors13

rewind(f);
mustWrite(f, header, sizeof(*header));
carefulClose(&f);
verbose(1, "Completed %s is %lld bytes\n", output, header->size);
}

int dnaSeqCmpName(const void *va, const void *vb)
/* Compare to sort based on sequence name. */
{
const struct dnaSeq *a = *((struct dnaSeq **)va);
const struct dnaSeq *b = *((struct dnaSeq **)vb);
return strcmp(a->name, b->name);
}

void itsaMake(int inCount, char *inputs[], char *output)
/* itsaMake - Make a suffix array file out of input DNA sequences.. */
{
uglyTime(NULL);
bits64 maxGenomeSize = 1024LL*1024*1024*4;

/* Fill out baseToVal array - A is already done. */
baseToVal[(int)'C'] = baseToVal[(int)'c'] = ITSA_C;
baseToVal[(int)'G'] = baseToVal[(int)'g'] = ITSA_G;
baseToVal[(int)'T'] = baseToVal[(int)'t'] = ITSA_T;


/* Load all DNA, make sure names are unique, and alphabetize by name. */
struct dnaSeq *seqList = NULL, *seq;
struct hash *uniqSeqHash = hashNew(0);
bits64 totalDnaSize = 1;	/* FOr space between. */
int inputIx;
for (inputIx=0; inputIx<inCount; ++inputIx)
    {
    char * input = inputs[inputIx];
    struct dnaLoad *dl = dnaLoadOpen(input);
    while ((seq = dnaLoadNext(dl)) != NULL)
	{
	verbose(1, "read %s with %d bases\n", seq->name, seq->size);
	if (hashLookup(uniqSeqHash, seq->name))
	    errAbort("Input sequence name %s repeated, all must be unique.", seq->name);
	totalDnaSize +=  seq->size + 1;
	if (totalDnaSize > maxGenomeSize)
	    errAbort("Too much DNA. Can only handle up to %lld bases", maxGenomeSize);
	slAddHead(&seqList, seq);
	}
    dnaLoadClose(&dl);
    }
slSort(&seqList, dnaSeqCmpName);
uglyTime("Loaded %lld bases in %d sequences", totalDnaSize, slCount(seqList));

/* Allocate big buffer for all DNA. */
DNA *allDna = globalAllDna = needHugeMem(totalDnaSize);
allDna[0] = 0;
bits64 chromOffset = 1;	/* Have zeroes between each chrom, and before and after. */

/* Copy DNA to a single big buffer, and create chromInfo on each sequence. */
struct chromInfo *chrom, *chromList = NULL;
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    AllocVar(chrom);
    chrom->name = cloneString(seq->name);
    chrom->size = seq->size;
    chrom->offset = chromOffset;
    slAddHead(&chromList, chrom);
    toUpperN(seq->dna, seq->size);
    memcpy(allDna + chromOffset, seq->dna, seq->size + 1);
    chromOffset += seq->size + 1;
    }
slReverse(&chromList);

/* Free up separate dna sequences because we're going to need a lot of RAM soon. */


/* Allocate index array, and offset and list arrays. */
dnaSeqFreeList(&seqList);
bits32 *index13;
AllocArray(index13, itsaSlotCount);
bits32 *offsetArray = needHugeMem(totalDnaSize * sizeof(bits32));
bits32 *listArray = needHugeZeroedMem(totalDnaSize * sizeof(bits32));;
uglyTime("Allocated buffers %lld bytes total", 
	9LL*totalDnaSize + itsaSlotCount*sizeof(bits32));

/* Where normally we'd keep some sort of structure with a next element to form a list
 * of matching positions in each slot of our index,  to conserve memory we'll do this
 * with two parallel arrays.  Because we're such cheapskates in terms of memory we'll
 * (and still using 9*genomeSize bytes of RAM) we'll use these arrays for two different
 * purposes.   
 *     In the first phase they will together be used to form linked lists of
 * offsets, and the 13mer index will point to the first item in each list.  In this
 * phase the offsetArray contains offsets into the allDna structure, and the listArray
 * contains the next pointers for the list.  After the first phase we write out the
 * suffix array to disk.
 *     In the second phase we read the suffix array back into the offsetArray, and
 * use the listArray for the traverseArray.  We write out the traverse array to finish
 * things up. */


/* Load up all DNA buffer. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    verbose(2, "  About to do first pass index\n");
    indexChromPass1(chrom, allDna, offsetArray, listArray, index13);
    verbose(2, "  Done first pass index\n");
    }
uglyTime("Done big bucket sort");
slReverse(&chromList);
itsaWriteMerged(chromList, allDna, offsetArray, listArray, index13, output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
dnaUtilOpen();
itsaMake(argc-2, argv+1, argv[argc-1]);
return 0;
}

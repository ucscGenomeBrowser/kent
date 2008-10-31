/* i16Make - Make a suffix array file out of input DNA sequences.. */
/* Copyright Jim Kent 2008 all rights reserved. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "dnaLoad.h"
#include "dnaseq.h"
#include "verbose.h"
#include "i16.h"

static char const rcsid[] = "$Id: i16Make.c,v 1.1 2008/10/31 02:54:35 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "i16Make - Make a suffix array file out of input DNA sequences.\n"
  "usage:\n"
  "   i16Make input1 input2 ... inputN output.i16\n"
  "where each input is either a fasta file, a nib file, a 2bit file, or a text file\n"
  "containing the names of the above file types one per line, genomeMegs is an estimate\n"
  "of the genome size in megabases (should be an upper bound), and output.i16 is the\n"
  "output file containing the suffix array in a binary format.\n"
  );
}


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

void dumpDnaPairs(DNA *s, int size, FILE *f)
{
int i;
for (i=0; i<size; i+=2)
    fprintf(f, "%c%c ", s[i], s[i+1]);
}

void indexChrom(struct chromInfo *chrom, DNA *allDna, bits32 *offsetArray, bits32 *listArray, 
	bits32 *index16)
/* Fill in index with info on one chromosome. */
{
bits32 chromOffset = chrom->offset;
DNA *dna = allDna + chromOffset;
bits32 seqSize = chrom->size;
bits32 maskTil = 0;

verbose(2, "   start short initial loop\n");
/* Preload the 16mer with the first 15 bases. */
bits32 baseIx;
bits32 slot = 0;
for (baseIx=0; baseIx<15; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 16;
    slot <<= 2;
    slot += ntVal[baseLetter];
    }

verbose(2, "   start main loop\n");
/* Do the middle part of the sequence where there are no end conditions to consider. */
bits32 freePos = chromOffset;
for (baseIx = 15; baseIx < seqSize; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 16;
    slot = (slot << 2) + ntVal[baseLetter];
    if (baseIx >= maskTil)
	{
	offsetArray[freePos] = baseIx - (16 - 1) + chromOffset;
	listArray[freePos] = index16[slot];
	index16[slot] = freePos;
	++freePos;
	}
    }
verbose(2, "   end main loop\n");
}

inline int listSize(bits32 ix, bits32 *listArray)
/* Go through linked list as reprented in an array and count elements. */
{
int count = 0;
while (ix != 0)
    {
    ++count;
    ix = listArray[ix];
    }
return count;
}

void reverseSlots(bits32 *index16, bits32 *listArray)
/* Things will be prettier if we reverse all the slots. */
{
bits64 slot;
for (slot = 0; slot < i16SlotCount; ++slot)
    {
    bits32 list = 0, next, el;
    for (el = index16[slot]; el != 0; el = next)
        {
	next = listArray[el];
	listArray[el] = list;
	list = el;
	}
    index16[slot] = list;
    }
}

void writeSlotCounts(FILE *f, bits32 *index16, bits32 *listArray,
	bits64 *retBasesInIndex,
	bits64 *retOverflowSlotCount, bits64 *retOverflowBaseCount)
/* Figure out the counts of each slot, and how the overflow is going to happen. 
 * Write counts to file*/
{
bits64 slot;
bits64 basesInIndex = 0, overflowSlotCount=0, overflowBaseCount = 0;
for (slot = 0; slot < i16SlotCount; ++slot)
    {
    int slotSize = listSize(index16[slot], listArray);
    if (slotSize >= i16OverflowCount)
        {
	++overflowSlotCount;
	overflowBaseCount += slotSize;
	fputc((char)255, f);
	}
    else
        {
	basesInIndex += slotSize;
	fputc((char)slotSize, f);
	}
    }
*retBasesInIndex = basesInIndex;
*retOverflowSlotCount = overflowSlotCount;
*retOverflowBaseCount = overflowBaseCount;
}

void writeIndexOffsets(FILE *f, bits32 *index16, bits32 *listArray, bits32 *offsetArray)
/* Write out list of offsets excepting overflow ones */
{
bits64 slot;
for (slot = 0; slot < i16SlotCount; ++slot)
    {
    int slotSize = listSize(index16[slot], listArray);
    if (slotSize < i16OverflowCount)
        {
	bits32 ix = index16[slot];
	while (ix != 0)
	    {
	    writeOne(f, offsetArray[ix]);
	    ix = listArray[ix];
	    }
	}
    }
}

void writeOverflowSlots(FILE *f, bits32 *index16, bits32 *listArray)
/* Write list of index slots that overflow. */
{
bits64 slot;
for (slot = 0; slot < i16SlotCount; ++slot)
    {
    int slotSize = listSize(index16[slot], listArray);
    if (slotSize >= i16OverflowCount)
        {
	bits32 shortSlot = slot;
	writeOne(f, shortSlot);
	}
    }
}

void writeOverflowSizes(FILE *f, bits32 *index16, bits32 *listArray)
/* Write number of items in each overflow slot. */
{
bits64 slot;
for (slot = 0; slot < i16SlotCount; ++slot)
    {
    bits32 slotSize = listSize(index16[slot], listArray);
    if (slotSize >= i16OverflowCount)
	writeOne(f, slotSize);
    }
}

void writeOverflowOffsets(FILE *f, bits32 *index16, bits32 *listArray, bits32 *offsetArray)
/* Write offsets into DNA for all overflows. */
{
bits64 slot;
for (slot = 0; slot < i16SlotCount; ++slot)
    {
    int slotSize = listSize(index16[slot], listArray);
    if (slotSize >= i16OverflowCount)
        {
	bits32 ix = index16[slot];
	while (ix != 0)
	    {
	    writeOne(f, offsetArray[ix]);
	    ix = listArray[ix];
	    }
	}
    }
}

void calcOverflows(bits32 *index16, bits32 *listArray, bits32 *offsetArray, 
	bits32 *overflowSlots, bits32 *overflowSizes, bits32 *overflowOffsets)
/* Go through and get all the overflow information, which we've already sized and
 * allocated. */
{
bits64 slot;
for (slot = 0; slot < i16SlotCount; ++slot)
    {
    bits32 slotSize = listSize(index16[slot], listArray);
    if (slotSize >= i16OverflowCount)
        {
	*overflowSlots++ = slot;
	*overflowSizes++ = slotSize;
	bits32 ix = index16[slot];
	while (ix != 0)
	    {
	    *overflowOffsets++ = offsetArray[ix];
	    ix = listArray[ix];
	    }
	}
    }
}

void i16Make(int inCount, char *inputs[], char *output)
/* itsaMake - Make a suffix array file out of input DNA sequences.. */
{
verboseTime(1, NULL);
bits64 maxGenomeSize = 1024LL*1024*1024*4;

/* Load all DNA, make sure names are unique, and alphabetize by name. */
struct dnaSeq *seqList = NULL, *seq;
struct hash *uniqSeqHash = hashNew(0);
bits64 totalDnaSize = 1;	/* For space between. */
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
verboseTime(1, "Loaded %lld bases in %d sequences", totalDnaSize, slCount(seqList));

/* Allocate big buffer for all DNA. */
DNA *allDna = needHugeMem(totalDnaSize);
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
dnaSeqFreeList(&seqList);


/* Allocate index array, and offset and list arrays. */
bits32 *index16 = needHugeZeroedMem(i16SlotCount*sizeof(bits32));
bits32 *offsetArray = needHugeMem(totalDnaSize * sizeof(bits32));
bits32 *listArray = needHugeZeroedMem(totalDnaSize * sizeof(bits32));
verboseTime(1, "Allocated buffers %lld bytes total", 
        sizeof(bits32)*(2*totalDnaSize + i16SlotCount));

/* Where normally we'd keep some sort of structure with a next element to form a list
 * of matching positions in each slot of our index,  to conserve memory on machines with
 * 64 bit pointers (which considering the overall memory requirements are what are going
 * to be running this), we'll do this with two parallel arrays of 32-bit offsets.   */

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    indexChrom(chrom, allDna, offsetArray, listArray, index16);
    verbose(1, "%s ", chrom->name);
    }
verbose(1, "\n");
verboseTime(1, "Indexed into lists in memory.");

reverseSlots(index16, listArray);
verboseTime(1, "Reversed slots");

/* Open output file, allocate and write all-zero header to reserve space. */
FILE *f = mustOpen(output, "w");
struct i16Header *header;
AllocVar(header);
mustWrite(f, header, sizeof(*header));

/* Write out chromosome names, and any zero padding */
bits32 chromNamesSize = 0;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
   {
   int nameSize = strlen(chrom->name) + 1;
   mustWrite(f, chrom->name, nameSize);
   chromNamesSize += nameSize;
   }
header->chromNamesSize = roundUpTo4(chromNamesSize);
zeroPad(f, header->chromNamesSize - chromNamesSize);

/* Write out chromosome sizes */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    writeOne(f, chrom->size);

/* Write DNA. Give ourselves at least 16 extra zeroes at end to avoid some end condition
 * tests during search. */
mustWrite(f, allDna, totalDnaSize);
header->dnaDiskSize = roundUpTo4(totalDnaSize+16);
zeroPad(f, header->dnaDiskSize - totalDnaSize);
verboseTime(1, "Wrote %lld bases of to disk including zero pads", header->dnaDiskSize);

/* Figure out the counts of each slot, and how the overflow is going to happen. */
writeSlotCounts(f, index16, listArray, &header->basesInIndex, 
	&header->overflowSlotCount, &header->overflowBaseCount);
verboseTime(1, "Write slot counts. %lld slots, %lld bases in %lld overflow slots",
	header->basesInIndex, header->overflowBaseCount, header->overflowSlotCount);

/* Write out the offsets in each non-overflow slot */
writeIndexOffsets(f, index16, listArray, offsetArray);
verboseTime(1, "Write index offsets");

/* Allocate and calculate the overflow bits. */
bits32 *overflowSlots, *overflowSizes, *overflowOffsets;
overflowSlots = needHugeMem(header->overflowSlotCount  * sizeof(bits32));
overflowSizes = needHugeMem(header->overflowSlotCount  * sizeof(bits32));
overflowOffsets = needHugeMem(header->overflowBaseCount * sizeof(bits32));
calcOverflows(index16, listArray, offsetArray, overflowSlots, overflowSizes, overflowOffsets);
verboseTime(1, "Calculated overflows");

mustWrite(f, overflowSlots, header->overflowSlotCount * sizeof(bits32));
mustWrite(f, overflowSizes, header->overflowSlotCount * sizeof(bits32));
mustWrite(f, overflowOffsets, header->overflowBaseCount * sizeof(bits32));
verboseTime(1, "Wrote overflows");

/* Finish up header, and write it at start of file. */
header->magic = I16_MAGIC;
header->majorVersion = I16_MAJOR_VERSION;
header->minorVersion = I16_MINOR_VERSION;
header->chromCount = slCount(chromList);
header->size =  sizeof(*header) + header->chromNamesSize
	       	+ sizeof(bits32) * header->chromCount 
	       	+ header->dnaDiskSize 
	       	+ sizeof(UBYTE) * i16SlotCount 
	       	+ sizeof(bits32) * header->basesInIndex
	       	+ sizeof(bits32) * header->overflowSlotCount 
	       	+ sizeof(bits32) * header->overflowSlotCount 
	       	+ sizeof(bits32) * header->overflowBaseCount
	       	;
if (fseek(f, 0L, SEEK_SET) < 0)
    errnoAbort("Couldn't seek back to write header.");
mustWrite(f, header, sizeof(*header));
verbose(1, "Successfully wrote %s which is %lld bytes\n", output, header->size);

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
dnaUtilOpen();
i16Make(argc-2, argv+1, argv[argc-1]);
return 0;
}

/* splixMake - Create splat index file from DNA sequences.  For structure of splix file see
 * comment in splix.h on splixFileHeader. */
/* This file is copyright 2008 Jim Kent.  All rights reserved. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "dnaLoad.h"
#include "dnaseq.h"
#include "splix.h"

static char const rcsid[] = "$Id: splixMake.c,v 1.1 2008/10/18 09:29:18 kent Exp $";

#define splixMaxStackSize 4096

boolean unmask = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "splixMake - Create splat index file from DNA sequences.\n"
  "usage:\n"
  "   splixMake input output.splix\n"
  "where input is either a fasta file, a nib file, a 2bit file, or a text file containing\n"
  "the names of the above file types one per line.\n"
  "options:\n"
  "   -unmask - ignore masking in input [not yet implemented, currently always ignores]\n"
  "   -verbose=N - set level of status messages, default 1.  Set to 0 for quiet, 2 for more.\n"
  );
}

static struct optionSpec options[] = {
   {"unmask", OPTION_BOOLEAN},
   {NULL, 0},
};

/* Defines that define order of the hexes in index. */
#define HEX_BEFORE1 0
#define HEX_BEFORE2 1
#define HEX_AFTER1 2
#define HEX_AFTER2 3

struct splixOneBaseListy
/* Temporary structure to store information on one base.  This will get compressed to a
 * smaller structure.  The motivation for this is that we can make a list out of these
 * as we go.  When we're done we can compress it into an array. */
    {
    struct splixOneBaseListy *next;
    bits32 dnaOffset;	/* Offset to start in dna. */
    bits16 hex[4];	/* Index with HEX_BEFORE1, HEX_BEFORE2, HEX_AFTER1, HEX_AFTER2 */
    };

static int cmpIx;

int splixOneBaseListyCmpHex(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct splixOneBaseListy *a = *((struct splixOneBaseListy **)va);
const struct splixOneBaseListy *b = *((struct splixOneBaseListy **)vb);
return a->hex[cmpIx] - b->hex[cmpIx];
}

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

struct chromInfo *indexSeq(struct dnaSeq *seq, int chromOffset, 
	struct lm *lm, struct splixOneBaseListy **listyIndex)
/* Create a splix in memory from dnaSeq. */
{
int baseIx,i;
DNA *dna = seq->dna;
int seqSize = seq->size;
int basesIndexed = 0;

struct chromInfo *chrom;
AllocVar(chrom);
chrom->name = cloneString(seq->name);
chrom->size = seq->size;
chrom->offset = chromOffset;
chrom->seq = seq;

/* Store six bases at a time: two sets of six before and after our middle set of 12 */
int hexes[6];
for (i=0; i<ArraySize(hexes); ++i)
    hexes[i] = 0;

/* Keep the middle 12 as it's own thing. */
int middle12 = 0;
int maskTil = 0;

/* Preload the hex arrays with the first 23 bases. */
for (baseIx=0; baseIx < 23; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 12;
    int base = (baseIx < seq->size ? ntValNoN[baseLetter] : 0);
    for (i=5; i>=0; --i)
        {
	int newHex = (hexes[i] << 2) + base;
	base = (newHex>>12);
	newHex &= 0xFFF;
	hexes[i] = newHex;
	}
    }

/* Do the middle part of the sequence where there are no end conditions to consider. */
for (baseIx = 23; baseIx < seqSize; ++baseIx)
    {
    int baseLetter = dna[baseIx];
    if (baseLetter == 'N')
        maskTil = baseIx + 12;
    int base = ntValNoN[baseLetter];
    for (i=5; i>=0; --i)
        {
	int newHex = (hexes[i] << 2) + base;
	base = (newHex>>12);
	newHex &= 0xFFF;
	hexes[i] = newHex;
	}
    if (baseIx >= maskTil)
	{
	middle12 = (hexes[2]<<12) + hexes[3];
	struct splixOneBaseListy *temp;
	lmAllocVar(lm, temp);
	temp->dnaOffset = baseIx - 23 + chromOffset;
	temp->hex[HEX_BEFORE1] = hexes[0];
	temp->hex[HEX_BEFORE2] = hexes[1];
	temp->hex[HEX_AFTER1] = hexes[4];
	temp->hex[HEX_AFTER2] = hexes[5];
	slAddHead(&listyIndex[middle12], temp);
	++basesIndexed;
	}
    }

/* Do the last 12 bases. */
for (baseIx = seqSize-12; baseIx < seqSize; ++baseIx)
    {
    int base = 0;
    for (i=5; i>=0; --i)
        {
	int newHex = (hexes[i] << 2) + base;
	base = (newHex>>12);
	newHex &= 0xFFF;
	hexes[i] = newHex;
	}
    middle12 = (hexes[2]<<12) + hexes[3];
    struct splixOneBaseListy *listy;
    lmAllocVar(lm, listy);
    listy->dnaOffset = baseIx - 11 + chromOffset;
    listy->hex[HEX_BEFORE1] = hexes[0];
    listy->hex[HEX_BEFORE2] = hexes[1];
    listy->hex[HEX_AFTER1] = hexes[4];
    listy->hex[HEX_AFTER2] = hexes[5];
    slAddHead(&listyIndex[middle12], listy);
    ++basesIndexed;
    }
chrom->basesIndexed = basesIndexed;
return chrom;
}

bits64 roundUpTo8(bits64 x)
/* Round x up to next multiple of 8 */
{
return (x+7) & (~(7));
}

void zeroPad(FILE *f, int count)
/* Write zeroes to file. */
{
while (--count >= 0)
    fputc(0, f);
}


void splixWriteMerged(struct chromInfo *chromList, struct splixOneBaseListy **listyIndex, 
	bits32 totalBasesIndexed, char *output)
/* Write out a file that contains a single splix that is the merger of
 * all of the individual splixes in list. */
{
FILE *f = mustOpen(output, "w");

/** Allocate header and fill out easy constant fields. */
struct splixFileHeader *header;
AllocVar(header);
header->magic = SPLIX_MAGIC;
header->majorVersion = SPLIX_MAJOR_VERSION;
header->minorVersion = SPLIX_MINOR_VERSION;

/* Figure out sizes of everything and fill out rest of header. */
header->chromCount = slCount(chromList);
struct chromInfo *chrom;
bits32 chromNamesSize = 0;
bits64 totalDnaSize = 0;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
   {
   chromNamesSize += strlen(chrom->name) + 1;
   totalDnaSize += chrom->size + 1;
   }
header->chromNamesSize = roundUpTo8(chromNamesSize);
int chromNamesSizePad = header->chromNamesSize - chromNamesSize;
bits32 chromSizesSize = roundUpTo8(header->chromCount*sizeof(bits32));
header->basesIndexed = totalBasesIndexed;
header->dnaDiskSize = roundUpTo8(totalDnaSize);
header->size = sizeof(*header) + header->chromNamesSize + chromSizesSize + 
	+ header->dnaDiskSize
	+ sizeof(bits32) * splixSlotCount 	/* Index slot size array */
	+ totalBasesIndexed * 4*(sizeof(bits32) + sizeof(bits16));  /* Slots themselves. */
verbose(1, "%s file size will be %lld\n", output, header->size);

/* Write header. */
mustWrite(f, header, sizeof(*header));

/* Write chromosome names. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    mustWrite(f, chrom->name, strlen(chrom->name)+1);
zeroPad(f, chromNamesSizePad);

/* Write chromosome sizes. */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    mustWrite(f, &chrom->size, sizeof(chrom->size));
int chromSizesSizePad = chromSizesSize - header->chromCount * sizeof(bits32);
zeroPad(f, chromSizesSizePad);

/* Write DNA */
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    toUpperN(chrom->seq->dna, chrom->size);
    mustWrite(f, chrom->seq->dna, chrom->size);
    fputc(0, f);
    }
zeroPad(f, header->dnaDiskSize - totalDnaSize);

/* Write slot sizes. */
int slotIx;
for (slotIx=0; slotIx < splixSlotCount; ++slotIx)
    {
    bits32 count = slCount(listyIndex[slotIx]);
    mustWrite(f, &count, sizeof(count));
    }

/* Write each of the slots */
for (slotIx=0; slotIx < splixSlotCount; ++slotIx)
     {
     struct splixOneBaseListy *el, *list = listyIndex[slotIx];
     if (list != NULL)
         {
	 /* Allocate arrays o stack if they are reasonable size, else dynamically.
	  * This works because 99% of them or more are small, but there are a few big ones.
	  * You can allocate an array dimensioned by a variable in gcc, but it crashes if
	  * it's too big.... */
	 bits16 hexOnStack[4][splixMaxStackSize];
	 bits32 offsetsOnStack[4][splixMaxStackSize];
	 bits16 *hex[4];
	 bits32 *offsets[4];
	 int slotSize = slCount(list);
	 int i;
	 if (slotSize <= splixMaxStackSize)
	     {
	     for (i=0; i<4; ++i)
	         {
		 hex[i] = hexOnStack[i];
		 offsets[i] = offsetsOnStack[i];
		 }
	     }
	 else
	     {
	     for (i=0; i<4; ++i)
	         {
		 AllocArray(hex[i], slotSize);
		 AllocArray(offsets[i], slotSize);
		 }
	     }
	 for (i=0; i<4; ++i)
	     {
	     cmpIx = i;
	     slSort(&list, splixOneBaseListyCmpHex);
	     int j;
	     for (j=0, el=list; el != NULL; ++j, el = el->next)
	         {
		 assert(el != NULL);
		 hex[i][j] = el->hex[i];
		 offsets[i][j] = el->dnaOffset;
		 }
	     }
	 for (i=0; i<4; ++i)
	     {
	     mustWrite(f, &hex[i][0], slotSize * sizeof(bits16));
	     }
	 for (i=0; i<4; ++i)
	     {
	     mustWrite(f, &offsets[i][0], slotSize * sizeof(bits32));
	     }
	 if (slotSize > splixMaxStackSize)
	     {
	     for (i=0; i<4; ++i)
	         {
		 freeMem(hex[i]);
		 freeMem(offsets[i]);
		 }
	     }
	 listyIndex[slotIx] = list;	/* Put list back after all the sorts. */
	 }
     }

freeMem(header);
carefulClose(&f);
}

void splixMake(char *input, char *output)
/* splixMake - Create splat index file from DNA sequences.. */
{
struct lm *lm = lmInit(0);
struct dnaLoad *dl = dnaLoadOpen(input);
struct dnaSeq *seq;
struct hash *uniqHash = hashNew(0);
struct chromInfo *chrom, *chromList = NULL;
bits64 chromOffset = 0, maxOffset;
struct splixOneBaseListy **listyIndex;
AllocArray(listyIndex, splixSlotCount);
maxOffset = 1;
maxOffset <<= 32;
bits32 totalBasesIndexed = 0;
while ((seq = dnaLoadNext(dl)) != NULL)
    {
    verbose(1, "processing %s with %d bases\n", seq->name, seq->size);
    hashAddUnique(uniqHash, seq->name, NULL);
    chrom = indexSeq(seq, chromOffset, lm, listyIndex);
    chromOffset += chrom->size + 1;
    if (chromOffset >= maxOffset)
        errAbort("Too much sequence to index, can only handle 4 Gig\n");
    totalBasesIndexed += chrom->basesIndexed;
    slAddHead(&chromList, chrom);
    }
verbose(1, "Indexed %lld bases\n", (long long)totalBasesIndexed);
slReverse(&chromList);
splixWriteMerged(chromList, listyIndex, totalBasesIndexed, output);
lmCleanup(&lm);
freez(&listyIndex);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
unmask = optionExists("unmask");
if (argc != 3)
    usage();
dnaUtilOpen();
splixMake(argv[1], argv[2]);
return 0;
}

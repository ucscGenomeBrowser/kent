/* i16Dump - Dump out i16 file into a readable format.. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "i16.h"

static char const rcsid[] = "$Id: i16Dump.c,v 1.2 2008/11/06 07:03:00 kent Exp $";

int maxSize = 50000;
int maxOverflow = 16;
char *mainIndex = NULL;
char *overflowIndex = NULL;
char *chromInfo = NULL;
boolean mmap;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "i16Dump - Dump out i16 file into a readable format.\n"
  "usage:\n"
  "   i16Dump input.i16\n"
  "options:\n"
  "   -maxSize=N Maximum bits to dump. Default %d\n"
  "   -maxOverflow=N Maximum overflows to report. Default %d\n"
  "   -mainIndex=fileName.  Place to put main index\n"
  "   -overflowIndex=filename. Place to put overflow index\n"
  "   -chromInfo=fileName. Place to put chrom names\n"
  "   -mmap - Generally faster if this is set.  Memory map rather than load file\n"
  , maxSize, maxOverflow
  );
}

static struct optionSpec options[] = {
   {"maxSize", OPTION_INT},
   {"maxOverflow", OPTION_INT},
   {"mainIndex", OPTION_STRING},
   {"overflowIndex", OPTION_STRING},
   {"chromInfo", OPTION_STRING},
   {"mmap", OPTION_BOOLEAN},
   {NULL, 0},
};

struct i16Header *i16ReadHeaderOnly(char *input)
/* Just read header, not rest. */
{
FILE *f = mustOpen(input, "r");
struct i16Header *header;
AllocVar(header);
mustRead(f, header, sizeof(*header));
carefulClose(&f);
return header;
}

void writeSlotAsDna(FILE *f, bits32 slot)
/* Convert binary to DNA representation. */
{
DNA dna[16];
int i;
for (i=15; i>=0; --i)
    {
    dna[i] = valToNt[slot&3];
    slot >>= 2;
    }
mustWrite(f, dna, 16);
}

void dumpChromInfo(struct i16 *i16, char *fileName)
/* Write out chromosome name and size info to file. */
{
int i;
FILE *f = mustOpen(fileName, "w");
for (i=0; i<i16->header->chromCount; ++i)
    fprintf(f, "%s\t%u\t%u\n", i16->chromNames[i], i16->chromSizes[i], i16->chromOffsets[i]);
carefulClose(&f);
}

void dumpOverflowIndex(struct i16 *i16, char *fileName, int maxCount, int maxOverflow)
/* Write out overflow index. */
{
FILE *f = mustOpen(fileName, "w");
int i;
if (maxCount > i16->header->overflowSlotCount)
    maxCount = i16->header->overflowSlotCount;
for (i=0; i<maxCount; ++i)
    {
    bits32 slot = i16->overflowSlots[i];
    fprintf(f, "%u\t", slot);
    writeSlotAsDna(f, slot);
    bits32 slotSize = i16->overflowSizes[i];
    fprintf(f, "\t%u\t", slotSize);
    if (slotSize > maxOverflow)
        slotSize = maxOverflow;
    bits32 *overflowList = i16->overflowOffsets + i16->overflowStarts[i];
    int j;
    for (j=0; j<slotSize; ++j)
	fprintf(f, "%u,", overflowList[j]);
    fprintf(f, "\n");
    }

carefulClose(&f);
}

void dumpMainIndex(struct i16 *i16, char *fileName, int maxCount)
/* Write out information on main index. */
{
FILE *f = mustOpen(fileName, "w");
int slot;
for (slot=0; slot<maxCount; ++slot)
    {
    fprintf(f, "%u\t", slot);
    writeSlotAsDna(f, slot);
    int slotSize = i16->slotSizes[slot];
    if (slotSize == 255)
        fprintf(f, "\tlots\t\t\n");
    else
	{
        fprintf(f, "\t%d\t", slotSize);
	bits32 *slotList = i16->slotOffsets + i16->slotStarts[slot];
	int j;
	for (j=0; j<slotSize; ++j)
	    fprintf(f, "%u,", slotList[j]);
	fprintf(f, "\n");
	}
    }

carefulClose(&f);
}

void i16Dump(char *input)
/* i16Dump - Dump out info on i16.  Useful for debugging. */
{
struct i16 *i16 = NULL;
struct i16Header *header = NULL;

if (mainIndex || overflowIndex || chromInfo)
    {
    i16 = i16Read(input, mmap);
    uglyTime("Read %s", input);
    header = i16->header;
    }
else
    header = i16ReadHeaderOnly(input);

printf("%d chromosomes, %lld bases, %lld indexed, %lld overflow bases, %lld overflow slots\n",
    header->chromCount, header->dnaDiskSize, header->basesInIndex,
    header->overflowBaseCount, header->overflowSlotCount);
printf("header->size=%lld\n", header->size);
    
if (chromInfo)
    dumpChromInfo(i16, chromInfo);

if (overflowIndex)
    dumpOverflowIndex(i16, overflowIndex, maxSize, maxOverflow);

if (mainIndex)
    dumpMainIndex(i16, mainIndex, maxSize);
}


int main(int argc, char *argv[])
/* Process command line. */
{
uglyTime(NULL);
dnaUtilOpen();
optionInit(&argc, argv, options);
maxSize = optionInt("maxSize", maxSize);
maxOverflow = optionInt("maxOverflow", maxOverflow);
mainIndex = optionVal("mainIndex", mainIndex);
overflowIndex = optionVal("overflowIndex", overflowIndex);
chromInfo= optionVal("chromInfo", chromInfo);
mmap = optionExists("mmap");
if (argc != 2)
    usage();
i16Dump(argv[1]);
return 0;
}

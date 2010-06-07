/* i16 - suffix array for genome.  Use i16Make utility to create one of these, and
 * the routines here to access it. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include <sys/mman.h>
#include "i16.h"
#include "net.h"

static void *pointerOffset(void *pt, bits64 offset)
/* A little wrapper around pointer arithmetic in terms of bytes. */
{
char *s = pt;
return s + offset;
}

struct i16 *i16Read(char *fileName, boolean memoryMap)
/* Read in a i16 from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */
{
/* Open file (low level), read in header, and check it. */
int fd = open(fileName, O_RDONLY);
if (fd < 0)
    errnoAbort("Can't open %s", fileName);
struct i16Header h;
if (read(fd, &h, sizeof(h)) < sizeof(h))
    errnoAbort("Couldn't read header of file %s", fileName);
if (h.magic != I16_MAGIC)
    errAbort("%s does not seem to be a i16 file.", fileName);
if (h.majorVersion > I16_MAJOR_VERSION)
    errAbort("%s is a newer, incompatible version of i16 format. "
             "This program works on version %d and below. "
	     "%s is version %d.",  fileName, I16_MAJOR_VERSION, fileName, h.majorVersion);

struct i16 *i16;
verbose(2, "i16 file %s size %lld\n", fileName, h.size);

/* Get a pointer to data in memory, via memory map, or allocation and read. */
struct i16Header *header ;
if (memoryMap)
    {
    header = mmap(NULL, h.size, PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);
    if (header == (void*)(-1))
	errnoAbort("Couldn't mmap %s, sorry", fileName);
    }
else
    {
    header = needHugeMem(h.size);
    if (lseek(fd, 0, SEEK_SET) < 0)
	errnoAbort("Couldn't seek back to start of i16 file %s.  "
		   "Splix files must be random access files, not pipes and the like"
		   , fileName);
    if (netReadAll(fd, header, h.size) < h.size)
        errnoAbort("Couldn't read all of i16 file %s.", fileName);
    }

/* Allocate wrapper structure and fill it in. */
AllocVar(i16);
i16->header = header;
i16->isMapped = memoryMap;

/* Make an array for easy access to chromosome names. */
int chromCount = header->chromCount;
char **chromNames = AllocArray(i16->chromNames, chromCount);
char *s = pointerOffset(header, sizeof(*header) );
int i;
for (i=0; i<chromCount; ++i)
    {
    chromNames[i] = s;
    s += strlen(s)+1;
    }

/* Keep track of where we are in memmap. */
bits64 mapOffset = sizeof(*header) + header->chromNamesSize;

/* Point into chromSizes array. */
bits32 *chromSizes = i16->chromSizes 
	= pointerOffset(header, mapOffset);
mapOffset += sizeof(bits32) * chromCount;

/* Calculate chromOffset array. */
bits32 offset = 1;
bits32 *chromOffsets = AllocArray(i16->chromOffsets, chromCount);
for (i=0; i<chromCount; ++i)
    {
    chromOffsets[i] = offset;
    offset += chromSizes[i] + 1;
    verbose(2, "i16 contains %s,  %d bases, %d offset\n", 
    	i16->chromNames[i], (int)i16->chromSizes[i], (int)chromOffsets[i]);
    }

verbose(2, "total dna size %lld in %d chromosomes\n", (long long)header->dnaDiskSize, header->chromCount);
i16->allDna = pointerOffset(header, mapOffset);
mapOffset += header->dnaDiskSize;

/* Point to the slotSizes array. */
UBYTE *slotSizes = i16->slotSizes = pointerOffset(header, mapOffset);
mapOffset += i16SlotCount;

/* Calculate slotStarts array */
bits32 *slotStarts = i16->slotStarts = needHugeMem(i16SlotCount * sizeof(bits32));
bits64 slot;
bits32 slotStart = 0;
for (slot=0; slot < i16SlotCount; ++slot)
    {
    int size = slotSizes[slot];
    if (size == i16OverflowCount)
        slotStarts[slot] = 0;
    else
	{
        slotStarts[slot] = slotStart;
	slotStart += size;
	}
    }
assert(slotStart == header->basesInIndex);
verbose(2, "calculated slotStarts\n");

/* Point to slotOffsets array. */
i16->slotOffsets = pointerOffset(header, mapOffset);
mapOffset += header->basesInIndex * sizeof(bits32);

/* Point to overflowSlots. */
i16->overflowSlots = pointerOffset(header, mapOffset);
mapOffset += header->overflowSlotCount * sizeof(bits32);

/* Point to overflowSizes. */
bits32 *overflowSizes = i16->overflowSizes = pointerOffset(header, mapOffset);
mapOffset += header->overflowSlotCount * sizeof(bits32);

/* Point to overflowOffsets */
i16->overflowOffsets = pointerOffset(header, mapOffset);
mapOffset += header->overflowBaseCount * sizeof(bits32);

/* Calculate overflowStarts */
bits32 *overflowStarts = i16->overflowStarts = 
	needHugeMem(header->overflowSlotCount * sizeof(bits32));
bits64 overflowStart = 0;
int overIx;
for (overIx=0; overIx < header->overflowSlotCount; ++overIx)
    {
    overflowStarts[overIx] = overflowStart;
    overflowStart += overflowSizes[overIx];
    }

assert(mapOffset == header->size);	/* Sanity check */
return i16;
}

void i16Free(struct i16 **pI16)
/* Free up resources associated with index. */
{
struct i16 *i16 = *pI16;
if (i16 != NULL)
    {
    freeMem(i16->chromNames);
    freeMem(i16->chromOffsets);
    if (i16->isMapped)
	munmap(i16->header, i16->header->size);
    else
	freeMem(i16->header);
    freez(pI16);
    }
}

int i16OffsetToChromIx(struct i16 *i16, bits32 tOffset)
/* Figure out index of chromosome containing tOffset */
{
int i;
int chromCount = i16->header->chromCount;
/* TODO - convert to binary search - will need to sort chromosome list though.... */
for (i=0; i<chromCount; ++i)
    {
    int chromStart = i16->chromOffsets[i];
    int chromEnd = chromStart + i16->chromSizes[i];
    if (tOffset >= chromStart && tOffset < chromEnd)
        return i;
    }
errAbort("tOffset %d out of range\n", tOffset);
return -1;
}


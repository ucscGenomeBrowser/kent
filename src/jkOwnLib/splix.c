/* splix - splat (speedy local alignment tool)  index.  Index that helps map short reads
 * quickly to the genome. */
/* This file is copyright 2008 Jim Kent.  All rights reserved. */

#include "common.h"
#include "splix.h"
#include "net.h"
#include <sys/mman.h>

static void *pointerOffset(void *pt, bits64 offset)
/* A little wrapper around pointer arithmetic in terms of bytes. */
{
char *s = pt;
return s + offset;
}

struct splix *splixRead(char *fileName, boolean memoryMap)
/* Read in a splix from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */
{
/* Open file (low level), read in header, and check it. */
int fd = open(fileName, O_RDONLY);
if (fd < 0)
    errnoAbort("Can't open %s", fileName);
struct splixFileHeader h;
if (read(fd, &h, sizeof(h)) < sizeof(h))
    errnoAbort("Couldn't read header of file %s", fileName);
if (h.magic != SPLIX_MAGIC)
    errAbort("%s does not seem to be a splix file.", fileName);
if (h.majorVersion > SPLIX_MAJOR_VERSION)
    errAbort("%s is a newer, incompatible version of splix format. "
             "This program works on version %d and below. "
	     "%s is version %d.",  fileName, SPLIX_MAJOR_VERSION, fileName, h.majorVersion);

struct splix *splix;

/* Get a pointer to data in memory, via memory map, or allocation and read. */
struct splixFileHeader *header ;
if (memoryMap)
    {
#ifdef MACHTYPE_sparc
    header = (struct splixFileHeader *)mmap(NULL, h.size, PROT_READ, MAP_SHARED, fd, 0);
#else
    header = mmap(NULL, h.size, PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);
#endif
    if (header == (void*)(-1))
	errnoAbort("Couldn't mmap %s, sorry", fileName);
    }
else
    {
    header = needHugeMem(h.size);
    if (lseek(fd, 0, SEEK_SET) < 0)
	errnoAbort("Couldn't seek back to start of splix file %s.  "
		   "Splix files must be random access files, not pipes and the like"
		   , fileName);
    if (netReadAll(fd, header, h.size) < h.size)
        errnoAbort("Couldn't read all of splix file %s.", fileName);
    }

/* Allocate wrapper structure and fill it in. */
AllocVar(splix);
splix->header = header;
splix->isMapped = memoryMap;

/* Make an array for easy access to chromosome names. */
int chromCount = header->chromCount;
char **chromNames = AllocArray(splix->chromNames, chromCount);
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
bits32 *chromSizes = splix->chromSizes 
	= pointerOffset(header, mapOffset);
mapOffset += sizeof(bits32) * chromCount;
if (chromCount&1)
    mapOffset += sizeof(bits32);	/* Take care of padding to 8 bytes */

verbose(2, "total dna size %lld in %d chromosomes\n", (long long)header->dnaDiskSize, header->chromCount);
splix->allDna = pointerOffset(header, mapOffset);
mapOffset += header->dnaDiskSize;

/* Calculate chromOffset array. */
bits32 offset = 0;
bits32 *chromOffsets = AllocArray(splix->chromOffsets, chromCount);
for (i=0; i<chromCount; ++i)
    {
    chromOffsets[i] = offset;
    offset += chromSizes[i] + 1;
    verbose(2, "splix contains %s,  %d bases, %d offset\n", 
    	splix->chromNames[i], (int)splix->chromSizes[i], (int)chromOffsets[i]);
    }

/* Deal with array of sizes */
splix->slotSizes = pointerOffset(header, mapOffset);
mapOffset += splixSlotCount * sizeof(bits32);

/* Allocate index array and fill in slots. */
char **slots = AllocArray(splix->slots, splixSlotCount);
for (i=0; i<splixSlotCount; ++i)
    {
    bits32 slotSize = splix->slotSizes[i];;
    if (slotSize)
	{
	slots[i] = pointerOffset(header, mapOffset);
	mapOffset += slotSize * 4 * (sizeof(bits32) + sizeof(bits16));
	}
    }

assert(mapOffset == header->size);	/* Sanity check */
return splix;
}

void splixFree(struct splix **pSplix)
/* Free up resources associated with index. */
{
struct splix *splix = *pSplix;
if (splix != NULL)
    {
    if (splix->isMapped)
	munmap((void *)splix->header, splix->header->size);
    else
	freeMem(splix->header);
    freez(pSplix);
    }
}

int splixOffsetToChromIx(struct splix *splix, bits32 tOffset)
/* Figure out index of chromosome containing tOffset */
{
int i;
int chromCount = splix->header->chromCount;
/* TODO - convert to binary search */
for (i=0; i<chromCount; ++i)
    {
    int chromStart = splix->chromOffsets[i];
    int chromEnd = chromStart + splix->chromSizes[i];
    if (tOffset >= chromStart && tOffset < chromEnd)
        return i;
    }
errAbort("tOffset %d out of range\n", tOffset);
return -1;
}


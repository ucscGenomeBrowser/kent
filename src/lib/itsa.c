/* itsa - indexed traversable suffix array.  Used for doing quick genomic searches.
 * Use itsaMake utility to create one of these files , and the routines here to access it.  
 * See comment by itsaFileHeader for file format. See src/shortReads/itsaMake/itsa.doc as well 
 * for an explanation of the data structures, particularly the traverse array. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include <sys/mman.h>
#include "itsa.h"

static void *pointerOffset(void *pt, bits64 offset)
/* A little wrapper around pointer arithmetic in terms of bytes. */
{
char *s = pt;
return s + offset;
}

struct itsa *itsaRead(char *fileName, boolean memoryMap)
/* Read in a itsa from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */
{
/* Open file (low level), read in header, and check it. */
int fd = open(fileName, O_RDONLY);
if (fd < 0)
    errnoAbort("Can't open %s", fileName);
struct itsaFileHeader h;
if (read(fd, &h, sizeof(h)) < sizeof(h))
    errnoAbort("Couldn't read header of file %s", fileName);
if (h.magic != ITSA_MAGIC)
    errAbort("%s does not seem to be a itsa file.", fileName);
if (h.majorVersion > ITSA_MAJOR_VERSION)
    errAbort("%s is a newer, incompatible version of itsa format. "
             "This program works on version %d and below. "
	     "%s is version %d.",  fileName, ITSA_MAJOR_VERSION, fileName, h.majorVersion);

struct itsa *itsa;
verbose(2, "itsa file %s size %lld\n", fileName, h.size);

/* Get a pointer to data in memory, via memory map, or allocation and read. */
struct itsaFileHeader *header ;
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
	errnoAbort("Couldn't seek back to start of itsa file %s.  "
		   "Splix files must be random access files, not pipes and the like"
		   , fileName);
    if (read(fd, header, h.size) < h.size)
        errnoAbort("Couldn't read all of itsa file %s.", fileName);
    }

/* Allocate wrapper structure and fill it in. */
AllocVar(itsa);
itsa->header = header;
itsa->isMapped = memoryMap;

/* Make an array for easy access to chromosome names. */
int chromCount = header->chromCount;
char **chromNames = AllocArray(itsa->chromNames, chromCount);
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
bits32 *chromSizes = itsa->chromSizes 
	= pointerOffset(header, mapOffset);
mapOffset += sizeof(bits32) * chromCount;

verbose(2, "total dna size %lld in %d chromosomes\n", (long long)header->dnaDiskSize, header->chromCount);
itsa->allDna = pointerOffset(header, mapOffset);
mapOffset += header->dnaDiskSize;

/* Calculate chromOffset array. */
bits32 offset = 0;
bits32 *chromOffsets = AllocArray(itsa->chromOffsets, chromCount);
for (i=0; i<chromCount; ++i)
    {
    chromOffsets[i] = offset;
    offset += chromSizes[i] + 1;
    verbose(2, "itsa contains %s,  %d bases, %d offset\n", 
    	itsa->chromNames[i], (int)itsa->chromSizes[i], (int)chromOffsets[i]);
    }

/* Point to the suffix array. */
itsa->array = pointerOffset(header, mapOffset);
mapOffset += header->arraySize * sizeof(bits32);

/* Point to the traverse array. */
itsa->traverse = pointerOffset(header, mapOffset);
mapOffset += header->arraySize * sizeof(bits32);

/* Point to the 13-mer index. */
itsa->index13 = pointerOffset(header, mapOffset);
mapOffset += itsaSlotCount * sizeof(bits32);

/* Make cursors array (faster to calculate than to load, and doesn't depend on data). */
itsa->cursors13 = pointerOffset(header, mapOffset);
mapOffset += itsaSlotCount * sizeof(UBYTE);

assert(mapOffset == header->size);	/* Sanity check */
return itsa;
}

void itsaFree(struct itsa **pItsa)
/* Free up resources associated with index. */
{
struct itsa *itsa = *pItsa;
if (itsa != NULL)
    {
    freeMem(itsa->chromNames);
    freeMem(itsa->chromOffsets);
    if (itsa->isMapped)
	munmap(itsa->header, itsa->header->size);
    else
	freeMem(itsa->header);
    freez(pItsa);
    }
}

int itsaOffsetToChromIx(struct itsa *itsa, bits32 tOffset)
/* Figure out index of chromosome containing tOffset */
{
int i;
int chromCount = itsa->header->chromCount;
/* TODO - convert to binary search - at least chrom list is sorted in itsas. */
for (i=0; i<chromCount; ++i)
    {
    int chromStart = itsa->chromOffsets[i];
    int chromEnd = chromStart + itsa->chromSizes[i];
    if (tOffset >= chromStart && tOffset < chromEnd)
        return i;
    }
errAbort("tOffset %d out of range\n", tOffset);
return -1;
}


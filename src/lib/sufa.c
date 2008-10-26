/* sufa - suffix array for genome.  Use sufaMake utility to create one of these, and
 * the routines here to access it. */

#include "common.h"
#include <sys/mman.h>
#include "sufa.h"

static void *pointerOffset(void *pt, bits64 offset)
/* A little wrapper around pointer arithmetic in terms of bytes. */
{
char *s = pt;
return s + offset;
}

struct sufa *sufaRead(char *fileName, boolean memoryMap)
/* Read in a sufa from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */
{
/* Open file (low level), read in header, and check it. */
int fd = open(fileName, O_RDONLY);
if (fd < 0)
    errnoAbort("Can't open %s", fileName);
struct sufaFileHeader h;
if (read(fd, &h, sizeof(h)) < sizeof(h))
    errnoAbort("Couldn't read header of file %s", fileName);
if (h.magic != SUFA_MAGIC)
    errAbort("%s does not seem to be a sufa file.", fileName);
if (h.majorVersion > SUFA_MAJOR_VERSION)
    errAbort("%s is a newer, incompatible version of sufa format. "
             "This program works on version %d and below. "
	     "%s is version %d.",  fileName, SUFA_MAJOR_VERSION, fileName, h.majorVersion);

struct sufa *sufa;
verbose(2, "sufa file %s size %lld\n", fileName, h.size);

/* Get a pointer to data in memory, via memory map, or allocation and read. */
struct sufaFileHeader *header ;
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
	errnoAbort("Couldn't seek back to start of sufa file %s.  "
		   "Splix files must be random access files, not pipes and the like"
		   , fileName);
    if (read(fd, header, h.size) < h.size)
        errnoAbort("Couldn't read all of sufa file %s.", fileName);
    }

/* Allocate wrapper structure and fill it in. */
AllocVar(sufa);
sufa->header = header;
sufa->isMapped = memoryMap;

/* Make an array for easy access to chromosome names. */
int chromCount = header->chromCount;
char **chromNames = AllocArray(sufa->chromNames, chromCount);
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
bits32 *chromSizes = sufa->chromSizes 
	= pointerOffset(header, mapOffset);
mapOffset += sizeof(bits32) * chromCount;

verbose(2, "total dna size %lld in %d chromosomes\n", (long long)header->dnaDiskSize, header->chromCount);
sufa->allDna = pointerOffset(header, mapOffset);
mapOffset += header->dnaDiskSize;

/* Calculate chromOffset array. */
bits32 offset = 0;
bits32 *chromOffsets = AllocArray(sufa->chromOffsets, chromCount);
for (i=0; i<chromCount; ++i)
    {
    chromOffsets[i] = offset;
    offset += chromSizes[i] + 1;
    verbose(2, "sufa contains %s,  %d bases, %d offset\n", 
    	sufa->chromNames[i], (int)sufa->chromSizes[i], (int)chromOffsets[i]);
    }

/* Finally point to the suffix array!. */
sufa->array = pointerOffset(header, mapOffset);
mapOffset += header->arraySize * sizeof(bits32);


assert(mapOffset == header->size);	/* Sanity check */
return sufa;
}

void sufaFree(struct sufa **pSufa)
/* Free up resources associated with index. */
{
struct sufa *sufa = *pSufa;
if (sufa != NULL)
    {
    freeMem(sufa->chromNames);
    freeMem(sufa->chromOffsets);
    if (sufa->isMapped)
	munmap(sufa->header, sufa->header->size);
    else
	freeMem(sufa->header);
    freez(pSufa);
    }
}

int sufaOffsetToChromIx(struct sufa *sufa, bits32 tOffset)
/* Figure out index of chromosome containing tOffset */
{
int i;
int chromCount = sufa->header->chromCount;
/* TODO - convert to binary search - will need to sort chromosome list though.... */
for (i=0; i<chromCount; ++i)
    {
    int chromStart = sufa->chromOffsets[i];
    int chromEnd = chromStart + sufa->chromSizes[i];
    if (tOffset >= chromStart && tOffset < chromEnd)
        return i;
    }
errAbort("tOffset %d out of range\n", tOffset);
return -1;
}


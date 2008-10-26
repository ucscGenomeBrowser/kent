/* sufx - suffix array for genome.  Use sufxMake utility to create one of these, and
 * the routines here to access it. */

#include "common.h"
#include <sys/mman.h>
#include "sufx.h"

static void *pointerOffset(void *pt, bits64 offset)
/* A little wrapper around pointer arithmetic in terms of bytes. */
{
char *s = pt;
return s + offset;
}

struct sufx *sufxRead(char *fileName, boolean memoryMap)
/* Read in a sufx from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */
{
/* Open file (low level), read in header, and check it. */
int fd = open(fileName, O_RDONLY);
if (fd < 0)
    errnoAbort("Can't open %s", fileName);
struct sufxFileHeader h;
if (read(fd, &h, sizeof(h)) < sizeof(h))
    errnoAbort("Couldn't read header of file %s", fileName);
if (h.magic != SUFX_MAGIC)
    errAbort("%s does not seem to be a sufx file.", fileName);
if (h.majorVersion > SUFX_MAJOR_VERSION)
    errAbort("%s is a newer, incompatible version of sufx format. "
             "This program works on version %d and below. "
	     "%s is version %d.",  fileName, SUFX_MAJOR_VERSION, fileName, h.majorVersion);

struct sufx *sufx;
verbose(2, "sufx file %s size %lld\n", fileName, h.size);

/* Get a pointer to data in memory, via memory map, or allocation and read. */
struct sufxFileHeader *header ;
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
	errnoAbort("Couldn't seek back to start of sufx file %s.  "
		   "Splix files must be random access files, not pipes and the like"
		   , fileName);
    if (read(fd, header, h.size) < h.size)
        errnoAbort("Couldn't read all of sufx file %s.", fileName);
    }

/* Allocate wrapper structure and fill it in. */
AllocVar(sufx);
sufx->header = header;
sufx->isMapped = memoryMap;

/* Make an array for easy access to chromosome names. */
int chromCount = header->chromCount;
char **chromNames = AllocArray(sufx->chromNames, chromCount);
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
bits32 *chromSizes = sufx->chromSizes 
	= pointerOffset(header, mapOffset);
mapOffset += sizeof(bits32) * chromCount;

verbose(2, "total dna size %lld in %d chromosomes\n", (long long)header->dnaDiskSize, header->chromCount);
sufx->allDna = pointerOffset(header, mapOffset);
mapOffset += header->dnaDiskSize;

/* Calculate chromOffset array. */
bits32 offset = 0;
bits32 *chromOffsets = AllocArray(sufx->chromOffsets, chromCount);
for (i=0; i<chromCount; ++i)
    {
    chromOffsets[i] = offset;
    offset += chromSizes[i] + 1;
    verbose(2, "sufx contains %s,  %d bases, %d offset\n", 
    	sufx->chromNames[i], (int)sufx->chromSizes[i], (int)chromOffsets[i]);
    }

/* Point to the suffix array. */
sufx->array = pointerOffset(header, mapOffset);
mapOffset += header->arraySize * sizeof(bits32);

/* Finally point to the traverse array!. */
sufx->traverse = pointerOffset(header, mapOffset);
mapOffset += header->arraySize * sizeof(bits32);

assert(mapOffset == header->size);	/* Sanity check */
return sufx;
}

void sufxFree(struct sufx **pSufx)
/* Free up resources associated with index. */
{
struct sufx *sufx = *pSufx;
if (sufx != NULL)
    {
    freeMem(sufx->chromNames);
    freeMem(sufx->chromOffsets);
    if (sufx->isMapped)
	munmap(sufx->header, sufx->header->size);
    else
	freeMem(sufx->header);
    freez(pSufx);
    }
}

int sufxOffsetToChromIx(struct sufx *sufx, bits32 tOffset)
/* Figure out index of chromosome containing tOffset */
{
int i;
int chromCount = sufx->header->chromCount;
/* TODO - convert to binary search - will need to sort chromosome list though.... */
for (i=0; i<chromCount; ++i)
    {
    int chromStart = sufx->chromOffsets[i];
    int chromEnd = chromStart + sufx->chromSizes[i];
    if (tOffset >= chromStart && tOffset < chromEnd)
        return i;
    }
errAbort("tOffset %d out of range\n", tOffset);
return -1;
}

void sufxFillInTraverseArray(char *dna, bits32 *suffixArray, int arraySize, bits32 *traverseArray)
/* Fill in the bits that will help us traverse the array as if it were a tree. */
{
bits32 *traverse;
AllocArray(traverse, arraySize);
int depth = 0;
int stackSize = 4*1024;
int *stack;
AllocArray(stack, stackSize);
int i;
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
	    traverse[prevIx] = i - prevIx;
	    depth = d;
	    break;
	    }
	}
    if (depth >= stackSize)
        errAbort("Stack overflow");
    stack[depth] = i;
    depth += 1;
    }
}


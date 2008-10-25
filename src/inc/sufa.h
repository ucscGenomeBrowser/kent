/* sufa - splat (speedy local alignment tool)  index.  Index that helps map short reads
 * quickly to the genome. */

#ifndef SUFA_H
#define SUFA_H

struct sufaFileHeader
/* Short read index file binary file header.  A sufa file starts with this fixed 128 byte
 * structure.  It is followed by the following sections:
 *    chromosome name strings - zero terminated.  Padded with zero to 4 byte boundary 
 *    chromosome sizes (32 bits each)
 *    chromosome DNA - one byte per base lower case.  A zero between each chrom, and a zero before
 *                     and after (to make some end conditions easier).  Padded if need be with
 *                     additional zeroes to 4 base boundary.
 *    suffixArray - 32 bits for each indexed base */
    {
    bits32 magic;	/* Always SUFA_MAGIC */
    bits16 majorVersion; /* This version changes when backward compatibility breaks. */
    bits16 minorVersion; /* This version changes whenever a feature is added. */
    bits64 size;	/* Total size to memmap, including header. */
    bits32 chromCount;	/* Total count of chromosomes/contigs in file. */
    bits32 chromNamesSize;	/* Size of names of all contigs (including zeroes at end),
    				   padded to 8 byte boundary as needed). */
    bits64 basesIndexed;/* Total number of bases actually indexed (non-N, unmasked). */
    bits64 dnaDiskSize;	/* Size of DNA on disk including zero separators */
    bits64 reserved[11];/* All zeroes for now. */
    };

struct sufa 
/* Short read index in memory */
    {
    struct sufa *next;
    boolean isMapped;	/* True if memory mapped. */
    struct sufaFileHeader *header;	/* File header. */
    char **chromNames;	/* Name of each chromosome. */
    bits32 *chromSizes;    /* Size of each chromosome.  No deallocation required (in memmap) */
    bits32 *chromOffsets;  /* Offset of each chromosome's DNA */
    char *allDna;	/* All DNA from each contig/chromosome with zero separators. */
    bits32 *array;	/* Alphabetized offsets into allDna. */
    };

struct sufa *sufaRead(char *fileName, boolean memoryMap);
/* Read in a sufa from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */

void sufaFree(struct sufa **pSplix);
/* Free up resources associated with index. */

int sufaOffsetToChromIx(struct sufa *sufa, bits32 tOffset);
/* Figure out index of chromosome containing tOffset */

/** Stuff to define SUFA files **/
#define SUFA_MAGIC 0x6727B283	/* Magic number at start of SUFA file */
#define SUFA_MAJOR_VERSION 0	
#define SUFA_MINOR_VERSION 0

#endif /* SUFA_H */

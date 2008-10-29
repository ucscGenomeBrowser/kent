/* sufx - suffix array with traversal extension for genome.  Use sufxMake utility to 
 * create one of these files , and the routines here to access it.  See comment by 
 * sufxFileHeader for file format. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef SUFX_H
#define SUFX_H

struct sufxFileHeader
/* Short read index file binary file header.  A sufx file starts with this fixed 128 byte
 * structure.  It is followed by the following sections:
 *    chromosome name strings - zero terminated.  Padded with zero to 4 byte boundary 
 *    chromosome sizes (32 bits each)
 *    chromosome DNA - one byte per base lower case.  A zero between each chrom, and a zero before
 *                     and after (to make some end conditions easier).  Padded if need be with
 *                     additional zeroes to 4 base boundary.
 *    suffix array -   32 bits for each indexed base. Alphabetical offsets into DNA
 *    traverse array - Also 32 bits per indexed base. Helper info to traverse array like a tree. */
    {
    bits32 magic;	 /* Always SUFX_MAGIC */
    bits16 majorVersion; /* This version changes when backward compatibility breaks. */
    bits16 minorVersion; /* This version changes whenever a feature is added. */
    bits64 size;	 /* Total size to memmap, including header. */
    bits32 chromCount;	 /* Total count of chromosomes/contigs in file. */
    bits32 chromNamesSize;	/* Size of names of all contigs (including zeroes at end),
    				   padded to 4 byte boundary as needed). */
    bits64 arraySize;	 /* Total number of bases actually indexed (non-N, unmasked). */
    bits64 dnaDiskSize;	 /* Size of DNA on disk with zero separators. Padded to 4 byte boundary  */
    bits64 reserved[11];/* All zeroes for now. */
    };

struct sufx 
/* Suffix array in memory */
    {
    struct sufx *next;
    boolean isMapped;	/* True if memory mapped. */
    struct sufxFileHeader *header;	/* File header. */
    char **chromNames;	/* Name of each chromosome. */
    bits32 *chromSizes;    /* Size of each chromosome.  No deallocation required (in memmap) */
    bits32 *chromOffsets;  /* Offset of each chromosome's DNA */
    char *allDna;	/* All DNA from each contig/chromosome with zero separators. */
    bits32 *array;	/* Alphabetized offsets into allDna. */
    bits32 *traverse;	/* Offsets to position in array where current letter changes. */
    };

struct sufx *sufxRead(char *fileName, boolean memoryMap);
/* Read in a sufx from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */

void sufxFree(struct sufx **pSufx);
/* Free up resources associated with index. */

int sufxOffsetToChromIx(struct sufx *sufx, bits32 tOffset);
/* Figure out index of chromosome containing tOffset */

/** Stuff to define SUFX files **/
#define SUFX_MAGIC 0x600BA3A1	/* Magic number at start of SUFX file */
#define SUFX_MAJOR_VERSION 0	
#define SUFX_MINOR_VERSION 0

#endif /* SUFX_H */

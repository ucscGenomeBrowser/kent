/* itsa - indexed traversable suffix array.  Used for doing quick genomic searches.
 * Use itsaMake utility to create one of these files , and the routines here to access it.  
 * See comment by itsaFileHeader for file format. See src/shortReads/itsaMake/itsa.doc as well 
 * for an explanation of the data structures, particularly the traverse array. */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef ITSA_H
#define ITSA_H

struct itsaFileHeader
/* Short read index file binary file header.  A itsa file starts with this fixed 128 byte
 * structure.  It is followed by the following sections:
 *    chromosome name strings - zero terminated.  Padded with zero to 4 byte boundary 
 *    chromosome sizes (32 bits each)
 *    chromosome DNA - one byte per base lower case.  A zero between each chrom, and a zero before
 *                     and after (to make some end conditions easier).  Padded if need be with
 *                     additional zeroes to 4 base boundary.
 *    suffix array -   32 bits for each indexed base. Alphabetical offsets into DNA
 *    traverse array - Also 32 bits per indexed base. Helper info to traverse array like a tree. 
 *    index13 - index of all 13-base prefixes in the suffix array.  4**13 32 bit values.  
 *              Will have zeros in positions where there is no data for this slot.
 *              Index+1 of suffix array where there is data (to free up zero for no data). 
 *    cursors13 - cursor position at each of the index13 positions*/
    {
    bits32 magic;	 /* Always ITSA_MAGIC */
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

struct itsa 
/* Suffix array in memory */
    {
    struct itsa *next;
    boolean isMapped;	/* True if memory mapped. */
    struct itsaFileHeader *header;	/* File header. */
    char **chromNames;	/* Name of each chromosome. */
    bits32 *chromSizes;    /* Size of each chromosome.  No deallocation required (in memmap) */
    bits32 *chromOffsets;  /* Offset of each chromosome's DNA */
    char *allDna;	/* All DNA from each contig/chromosome with zero separators. */
    bits32 *array;	/* Alphabetized offsets into allDna.  The suffix array. */
    bits32 *traverse;	/* Offsets to position in array where current prefix changes. */
    bits32 *index13;	/* Look up the first 13 bases of the query sequence here to find the corresponding
                         * offset into the suffix array.  0 indicates empty slot.  Subtract 1 to get
			 * offset (to free up zero for this meaning). */
    UBYTE *cursors13;	/* Cursor positions in suffix array at index positions. */
    };

struct itsa *itsaRead(char *fileName, boolean memoryMap);
/* Read in a itsa from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */

void itsaFree(struct itsa **pSufx);
/* Free up resources associated with index. */

int itsaOffsetToChromIx(struct itsa *itsa, bits32 tOffset);
/* Figure out index of chromosome containing tOffset */

/** Stuff to define ITSA files **/
#define ITSA_MAGIC 0x600BA3A1	/* Magic number at start of ITSA file */
#define ITSA_MAJOR_VERSION 0	
#define ITSA_MINOR_VERSION 0

/* Number of slots in itsa index. */
#define itsaSlotCount (1<<(13*2))

/* Base values in _alphabetical_ order. Unfortunately the ones in dnautil.h are not.... */
#define ITSA_A 0
#define ITSA_C 1
#define ITSA_G 2
#define ITSA_T 3

/* Table to convert letters to one of the above values. Call itsaBaseToValInit() before using. */
extern int itsaBaseToVal[256];

void itsaBaseToValInit();
/* Initialize itsaBaseToVal array */

int itsaDnaToBinary(char *dna, int size);
/* Convert dna to binary representation. Call itsaBaseToValInit() first. */

#endif /* ITSA_H */

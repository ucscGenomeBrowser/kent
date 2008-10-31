/* i16 - suffix array with traversal extension for genome.  Use i16Make utility to 
 * create one of these files , and the routines here to access it.  See comment by 
 * i16FileHeader for file format.  */
/* This file is copyright 2008 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef I16_H
#define I16_H

struct i16Header
/* Short read index file binary file header.  A i16 file starts with this fixed 128 byte
 * structure.  It is followed by the following sections:
 *    chromosome name strings - zero terminated.  Padded with zero to 4 byte boundary 
 *    chromosome sizes (32 bits each)
 *    chromosome DNA - one byte per base lower case.  A zero between each chrom, and a zero before
 *                     and after (to make some end conditions easier).  Padded if need be with
 *                     additional zeroes to 4 base boundary.
 *    slotCounts - 4**16 bytes saying the number of corresponding 16-mers that occur. 255 or more
 *                 represented as a 255, with additional data in the overflow table
 *    offsets - 4 bytes per base indexed. Contains offsets into DNA of each of the slots in order
 *              Offsets for overflow cases not stored here.
 *    overflowSlots - list of index slots that overflow in numeric order. 4*overflowCount bytes
 *    overflowSizes - sizes of overflow slots. 4*overflowSlotCount bytes
 *    overflowOffsets - Offsets to overflow positions. 4*overflowBaseCount bytes
 */
    {
    bits32 magic;	 /* Always I16_MAGIC */
    bits16 majorVersion; /* This version changes when backward compatibility breaks. */
    bits16 minorVersion; /* This version changes whenever a feature is added. */
    bits64 size;	 /* Total size to memmap, including header. */
    bits32 chromCount;	 /* Total count of chromosomes/contigs in file. */
    bits32 chromNamesSize;	/* Size of names of all contigs (including zeroes at end),
    				   padded to 4 byte boundary as needed). */
    bits64 basesInIndex; /* Total number of bases actually indexed.  Doesn't include overflow. */
    bits64 dnaDiskSize;	 /* Size of DNA on disk with zero separators. Padded to 4 byte boundary  */
    bits64 overflowSlotCount; /* Count of index slots with 255 or more items in them. */
    bits64 overflowBaseCount; /* Count of total number of offsets in overflowOffset. */
    bits64 reserved[9];/* All zeroes for now. */
    };

struct i16 
/* Suffix array in memory */
    {
    struct i16 *next;
    boolean isMapped;	/* True if memory mapped. */
    struct i16Header *header;	/* File header. */
    char **chromNames;	/* Name of each chromosome. */
    bits32 *chromSizes;    /* Size of each chromosome. */
    bits32 *chromOffsets;  /* Offset of each chromosome's DNA - not on disk*/
    char *allDna;	/* All DNA from each contig/chromosome with zero separators. */
    UBYTE *slotSizes;	/* Size of each index slot.  255 for overflow. */
    bits32 *slotStarts;  /* 4**16 index - points into slotOffsets  - not on disk*/
    bits32 *slotOffsets;/* Offsets into DNA for each non-overflow slot in order. */
    bits32 *overflowSlots; /* Array of slots with more than 255 items. */
    bits32 *overflowSizes; /* Array of sizes of slots with more than 255 items. */
    bits32 *overflowOffsets; /* Offsets into DNA for overflow slots. */
    bits32 *overflowStarts; /* Points into overflow offsets - not on disk. */
    };

struct i16 *i16Read(char *fileName, boolean memoryMap);
/* Read in a i16 from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */

void i16Free(struct i16 **pI16);
/* Free up resources associated with index. */

int i16OffsetToChromIx(struct i16 *i16, bits32 tOffset);
/* Figure out index of chromosome containing tOffset */

#define i16SlotCount (1LL<<(16*2))
#define i16OverflowCount 255

/** Stuff to define I16 files **/
#define I16_MAGIC 0x600BA3A1	/* Magic number at start of I16 file */
#define I16_MAJOR_VERSION 0	
#define I16_MINOR_VERSION 0

#endif /* I16_H */

/* splix - splat (speedy local alignment tool)  index.  Index that helps map short reads
 * quickly to the genome. */

#ifndef SPLIX_H
#define SPLIX_H

struct splixFileHeader
/* Short read index file binary file header.  A splix file starts with this fixed 128 byte
 * structure.  It is followed by the following sections:
 *    chromosome name strings - zero terminated
 *    chromosome sizes (32 bits each)
 *    chromosome DNA - one byte per base lower case.  A zero between each chrom.
 *    indexSlotSizes (4^^12 32 bit words containing size of each index slot
 *    indexSlots - explained more below
 * Each of these sections is padded with zeroes to end on an 8 byte (64 bit) boundary.
 * The index section consists of 4^^12 (16 million roughly) index slots.  Each slot 
 * corresponds to a DNA 12-mer.  The format of a slot is:
 *    hexesBefore1 - size # of 16 bit words, each containing 6 bases of DNA 2 bits/base 
 *                   and 4 bits of zero (most significant bits are zero).  These represent
 *                   the sixmers found before the 12-mer.  They are sorted numerically.
 *    hexesBefore2 - as hexesBefore, but contains sixmer six before the 12-mer.
 *    hexesAfter1 - sixmers after the 12-mer.
 *    hexesAfter2 - sixmers six after the 12-mer.
 *    offsetsBefore1 - 32 bit offsets into indexed DNA corresponding with hexBefore1
 *    offsetsBefore2 - 32 bit offsets into indexed DNA corresponding with hexBefore2
 *    offsetsAfter1 - 32 bit offsets corresponding ith hexesAfter1
 *    offsetsAfter2 - 32 bit offsets corresponding ith hexesAfter2
 * The splix files are structured so that they can be memory mapped relatively easily,
 * and so that on program load, and for a particular read, most of the action happens
 * in a few isolated piece of memory rather than scattered all over. */
    {
    bits32 magic;	/* Always SPLIX_MAGIC */
    bits16 majorVersion; /* This version changes when backward compatibility breaks. */
    bits16 minorVersion; /* This version changes whenever a feature is added. */
    bits64 size;	/* Total size to memmap, including header. */
    bits32 chromCount;	/* Total count of chromosomes/contigs in file. */
    bits32 chromNamesSize;	/* Size of names of all contigs (including zeroes at end),
    				   padded to 8 byte boundary as needed). */
    bits64 basesIndexed;/* Total number of bases actually indexed (non-N, unmasked). */
    bits64 dnaDiskSize;	/* Size of DNA on disk including zero separators and 8 byte padding */
    bits64 reserved[11];/* All zeroes for now. */
    };

struct splix 
/* Short read index in memory */
    {
    struct splix *next;
    boolean isMapped;	/* True if memory mapped. */
    struct splixFileHeader *header;	/* File header. */
    char **chromNames;	/* Name of each chromosome. */
    bits32 *chromSizes;    /* Size of each chromosome.  No deallocation required (in memmap) */
    bits32 *chromOffsets;	/* Offset of each chromosome's DNA */
    char *allDna;	/* All DNA from each contig/chromosome with zero separators. */
    bits32 *slotSizes;	/* 4^^12 array of slot sizes.  No deallocation required (in memmap) */
    char **slots;  	/* 16 M slots corresponding to 12 bases. Actual format of slot is
                         * explained in indexSlots section of splixFileHeader */
    };

#define splixSlotCount (1<<(12*2))
#define splixMinQuerySize 24

struct splix *splixRead(char *fileName, boolean memoryMap);
/* Read in a splix from a file.  Does this via memory mapping if you like,
 * which will be faster typically for about 100 reads, and slower for more
 * than that (_much_ slower for thousands of reads and more). */

void splixFree(struct splix **pSplix);
/* Free up resources associated with index. */

/** Stuff to define SPLIX files **/
#define SPLIX_MAGIC 0x5616A283	/* Magic number at start of SPLIX file */
#define SPLIX_MAJOR_VERSION 0	
#define SPLIX_MINOR_VERSION 0

#endif /* SPLIX_H */

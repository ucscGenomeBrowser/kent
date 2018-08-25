/* bwgInternal - stuff to create and use bigWig files.  Generally you'll want to use the
 * simpler interfaces in the bigWig module instead.  This file is good reading though
 * if you want to extend the bigWig interface, or work with bigWig files directly
 * without going through the Kent library. */

#ifndef BIGWIGFILE_H
#define BIGWIGFILE_H

enum bwgSectionType 
/* Code to indicate section type. */
    {
    bwgTypeBedGraph=1,
    bwgTypeVariableStep=2,
    bwgTypeFixedStep=3,
    };

struct bwgBedGraphItem
/* An bedGraph-type item in a bwgSection. */
    {
    struct bwgBedGraphItem *next;	/* Next in list. */
    bits32 start,end;		/* Range of chromosome covered. */
    float val;			/* Value. */
    };

struct bwgVariableStepItem
/* An variableStep type item in a bwgSection. */
    {
    struct bwgVariableStepItem *next;	/* Next in list. */
    bits32 start;		/* Start position in chromosome. */
    float val;			/* Value. */
    };

struct bwgVariableStepPacked
/* An variableStep type item in a bwgSection. */
    {
    bits32 start;		/* Start position in chromosome. */
    float val;			/* Value. */
    };

struct bwgFixedStepItem
/* An fixedStep type item in a bwgSection. */
    {
    struct bwgFixedStepItem *next;	/* Next in list. */
    float val;			/* Value. */
    };

struct bwgFixedStepPacked
/* An fixedStep type item in a bwgSection. */
    {
    float val;			/* Value. */
    };

union bwgItem
/* Union of item pointers for all possible section types. */
    {
    struct bwgBedGraphItem *bedGraphList;		/* A linked list */
    struct bwgFixedStepPacked *fixedStepPacked;		/* An array */
    struct bwgVariableStepPacked *variableStepPacked;	/* An array */
    /* No packed format for bedGraph... */
    };

struct bwgSection
/* A section of a bigWig file - all on same chrom.  This is a somewhat fat data
 * structure used by the bigWig creation code.  See also bwgSection for the
 * structure returned by the bigWig reading code. */
    {
    struct bwgSection *next;		/* Next in list. */
    char *chrom;			/* Chromosome name. */
    bits32 start,end;			/* Range of chromosome covered. */
    enum bwgSectionType type;
    union bwgItem items;		/* List/array of items in this section. */
    bits32 itemStep;			/* Step within item if applicable. */
    bits32 itemSpan;			/* Item span if applicable. */
    bits16 itemCount;			/* Number of items in section. */
    bits32 chromId;			/* Unique small integer value for chromosome. */
    bits64 fileOffset;			/* Offset of section in file. */
    };

struct bwgSectionHead
/* A header from a bigWig file section - similar to above bug what is on disk. */
    {
    bits32 chromId;	/* Chromosome short identifier. */
    bits32 start,end;	/* Range covered. */
    bits32 itemStep;	/* For some section types, the # of bases between items. */
    bits32 itemSpan;	/* For some section types, the # of bases in each item. */
    UBYTE type;		/* Type byte. */
    UBYTE reserved;	/* Always zero for now. */
    bits16 itemCount;	/* Number of items in block. */
    };

void bwgSectionHeadFromMem(char **pPt, struct bwgSectionHead *head, boolean isSwapped);
/* Read section header. */


int bwgSectionCmp(const void *va, const void *vb);
/* Compare to sort based on chrom,start,end.  */

struct bwgSection *bwgParseWig(
	char *fileName,       /* Name of ascii wig file. */
	boolean clipDontDie,  /* Skip items outside chromosome rather than aborting. */
	struct hash *chromSizeHash,  /* If non-NULL items checked to be inside chromosome. */
	int maxSectionSize,   /* Biggest size of a section.  100 - 100,000 is usual range. */
	struct lm *lm);	      /* Memory pool to allocate from. */
/* Parse out ascii wig file - allocating memory in lm. */

int bwgAverageResolution(struct bwgSection *sectionList);
/* Return the average resolution seen in sectionList. */

struct bbiSummary *bwgReduceSectionList(struct bwgSection *sectionList, 
	struct bbiChromInfo *chromInfoArray, int reduction);
/* Return summary of section list reduced by given amount. */

void bwgCreate(struct bwgSection *sectionList, struct hash *chromSizeHash, 
	int blockSize, int itemsPerSlot, boolean doCompress, boolean keepAllChromosomes,
        boolean fixedSummaries, boolean clipDontDie, char *fileName);
/* Create a bigWig file out of a sorted sectionList.  A lower level routine
 * than the one above. */

#endif /* BIGWIGFILE_H */

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

struct bwgSection *bwgParseWig(char *fileName, boolean clipDontDie, struct hash *chromSizeHash,
	int maxSectionSize, struct lm *lm);
/* Parse out ascii wig file - allocating memory in lm. */

int bwgAverageResolution(struct bwgSection *sectionList);
/* Return the average resolution seen in sectionList. */

struct bbiSummary *bwgReduceSectionList(struct bwgSection *sectionList, 
	struct bbiChromInfo *chromInfoArray, int reduction);
/* Return summary of section list reduced by given amount. */

#endif /* BIGWIGFILE_H */

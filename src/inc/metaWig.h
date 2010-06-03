/* Interface class so that wigs and bigWigs look similar */

#ifndef METAWIG_H
#define METAWIG_H

#ifndef BWGINTERNAL_H
#include "bwgInternal.h"
#endif

enum metaWigType 
    {
    mwtSections,
    mwtBigWig,
    };

struct metaWig
/* Interface class so that wigs and bigWigs look similar */
    {
    struct metaWig *next;
    enum metaWigType type;

    /* For type mwtSections */
    struct bwgSection *sectionList;
    struct hash *chromHash; /* Hash value is first section in that chrom */

    /* For type mwtBigWig */
    struct bbiFile *bwf;
    };

struct metaWig *metaWigOpen(char *fileName, struct lm *lm);
/* Wrap self around file.  Read all of it if it's wig, just header if bigWig. */

void metaWigClose(struct metaWig **pMw);
/* Close up metaWig file */

struct slName *metaWigChromList(struct metaWig *mw);
/* Return list of chromosomes covered in wig. */

struct bbiInterval *metaIntervalsForChrom(struct metaWig *mw, char *chrom, struct lm *lm);
/* Get sorted list of all intervals with data on chromosome. */

#endif /* METAWIG_H */


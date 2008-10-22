/* Splat.h - link together exported things from various splat modules. */

struct splatHit
/* Information on an index hit.  This only has about a 1% chance of being real after
 * extension, but hey, it's a start. */
    {
    bits32 tOffset;	/* Offset to DNA in target */
    int gapSize;	/* >0 for insert in query/del in target, <0 for reverse. 0 for no gap. */
    int subCount;	/* Count of substitutions we know about already. */
    int missingQuad;	/* Which data quadrant is lacking (not found in index). */
    };

struct splatTag
/* Roughtly 25-base match that passes maxGap and maxMismatch criteria. */
    {
    struct splatTag *next;	 /* Next in list. */
    int divergence;  /* mismatch + 2*inserts for now. */
    bits32 q1,t1;  /* Position of first block in query and target. */
    bits32 size1;	/* Size of first block. */
    bits32 q2,t2;  /* Position of second block if any. */
    bits32 size2;  /* Size of second block.  May be zero. */
    char strand; /* Query strand. */
    };
#define splatTagFree(pt) freez(pt)
#define splatTagFreeList(pList) slFreeList(pList)

struct splatAlign
/* A full alignment - the first 25 bases plus if need be a banded extension. */
    {
    struct splatAlign *next;	/* Next in list. */
    double score;		/* Score from scoring matrix. */
    struct cBlock *blockList;	/* List of aligning blocks. */
    char strand; 		/* Query strand. */
    };


extern char *version ;	/* Program version number. */

int countDnaDiffs(DNA *a, DNA *b, int size);
/* Count number of differing bases. */

void splatOutHeader(char *target, char *query, char *outType, FILE *f);
/* Output file header if any.  Also check and abort if outType is not supported. */

void splatOutTags(struct splatTag *tagList, char *outType,
	struct dnaSeq *qSeqF, struct dnaSeq *qSeqR, struct splix *splix, FILE *f);
/* Output tag matches to file. */


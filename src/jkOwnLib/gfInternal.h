/* gfInternal - some stuff that connects modules but we
 * don't want widely advertised in the blat code. */

struct gfRange
/* A range of bases found by genoFind.  Recursive
 * data structure.  Lowest level roughly corresponds
 * to an exon. */
    {
    struct gfRange *next;  /* Next in singly linked list. */
    int qStart;	/* Start in query */
    int qEnd;	/* End in query */
    char *tName;	  /* Target name.  Allocated here. */
    struct dnaSeq *tSeq;  /* Target Seq. May be NULL in a .nib.  Not allocated here. */
    int tStart;	/* Start in target */
    int tEnd;	/* End in target */
    struct gfRange *components;	/* Components of range. */
    int hitCount;	/* Number of hits. */
    int frame;		/* Reading frame (just for translated alignments) */
    struct trans3 *t3;	/* Translated frame or NULL. */
    int tTotalSize;	/* Size of entire target sequence, not just loaded parts.  Not set until late in the game. */
    char tStrand;	/* Just for PCR. */
    };

void gfRangeFree(struct gfRange **pEl);
/* Free a single dynamically allocated gfRange such as created
 * with gfRangeLoad(). */

void gfRangeFreeList(struct gfRange **pList);
/* Free a list of dynamically allocated gfRange's */

int gfRangeCmpTarget(const void *va, const void *vb);
/* Compare to sort based on target position. */

struct gfRange *gfRangesBundle(struct gfRange *exonList, int maxIntron);
/* Bundle a bunch of 'exons' into plausable 'genes'.  It's
 * not necessary to be precise here.  The main thing is to
 * group together exons that are close to each other in the
 * same target sequence. */

struct ssFfItem *gfRangesToFfItem(struct gfRange *rangeList, aaSeq *qSeq);
/* Convert ranges to ssFfItem's. */

struct ssBundle *ffSeedExtInMem(struct genoFind *gf, struct dnaSeq *qSeq, Bits *qMaskBits, 
	int qOffset, struct lm *lm, int minScore, boolean isRc);
/* Do seed and extend type alignment */

void dumpBuns(struct ssBundle *bunList);  /* uglyf */
void dumpFf(struct ffAli *left, DNA *needle, DNA *hay);/* uglyf */

void gfiExpandRange(struct gfRange *range, int qSize, int tSize, 
	boolean respectFrame, boolean isRc, int expansion);
/* Expand range to cover an additional 500 bases on either side. */

struct dnaSeq *gfiExpandAndLoadCached(struct gfRange *range, 
	struct hash *tFileCache, char *tSeqDir, int querySize, 
	int *retTotalSeqSize, boolean respectFrame, boolean isRc, int expansion);
/* Expand range to cover an additional expansion bases on either side.
 * Load up target sequence and return. (Done together because don't
 * know target size before loading.) */

void gfiGetSeqName(char *spec, char *name, char *file);
/* Extract sequence name and optionally file name from spec, 
 * which includes nib and 2bit files.  (The file may be NULL
 * if you don't care.) */

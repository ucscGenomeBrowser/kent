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

void dumpBuns(struct ssBundle *bunList);  /* uglyf */
void dumpFf(struct ffAli *left, DNA *needle, DNA *hay);/* uglyf */

void ffExpandExactRight(struct ffAli *ali, DNA *needleEnd, DNA *hayEnd);
/* Expand aligned segment to right as far as can exactly. */

void ffExpandExactLeft(struct ffAli *ali, DNA *needleStart, DNA *hayStart);
/* Expand aligned segment to left as far as can exactly. */


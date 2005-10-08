/* mkMafFrames - build mafFrames objects for exons */
#include "common.h"
#include "genePred.h"
#include "mafFrames.h"
#include "maf.h"
#include "dnautil.h"
#include "geneBins.h"
#include "chromBins.h"
#include "binRange.h"
#include "verbose.h"

/* Notes:
 * - functions are static to allow for implict inlining.
 * - both this code and the browser required no overlapping CDS exons
 * - strand in mafFrames matches the direction of transcript as projected
 *   onto the target species:
 *           maf   maf
 *      gene query target annotation
 *      +    +     +      +
 *      -    +     +      -
 *      +    -     +      -
 *      -    -     +      +
 */

/* assert for a frame being valid */
#define assertFrame(fr) assert(((fr) >= 0) && ((fr) <= 2))

/* data structure used to scan a mafComp object */
struct compCursor {
    struct mafComp *comp;   /* component object */
    int pos;                /* corresponding position in the sequence; it at a
                             * gap in the alignment, this is the next position
                             * in the sequence*/
    char *ptr;              /* pointer into text */
    boolean atBase;         /* is cursor at a base */
};

/* data structure used to scan a pair of mafComp objects */
struct scanCursor {
    struct compCursor query;   /* cursor for gene (query) */
    struct compCursor target;  /* cursor for target (genome) */
};

static struct compCursor compCursorNew(struct mafComp *comp)
/* construct a new compCursor.  Note: struct is copied, it is not dynamic */
{
struct compCursor cc;
cc.comp = comp;
cc.pos = comp->start;
cc.ptr = comp->text;
return cc;
}

static boolean compCursorAtBase(struct compCursor *cc)
/* Check if a position contains a base, this uses a quick check that really
 * doesn't test for a base, just >= A, so '.', '-', and '\0' fail */
{
return (*cc->ptr >= 'A');
}

static int revFrame(int fr)
/* reverse a frame, as if codon was reversed */
{
return (fr == 2) ? 0 : ((fr == 0) ? 2 : 1);
}


static int frameIncr(int frame, int amt)
/* increment the frame by the specified amount.  If amt is positive,
 * the frame is incremented in the direction of transcription, it 
 * it is negative it's incremented in the opposite direction. */
{
if (amt >= 0)
    return (frame + amt) % 3;
else
    return revFrame((revFrame(frame) - amt) % 3);
}

static struct scanCursor scanCursorNew(struct mafComp *geneComp, struct mafComp *targetComp)
/* construct a new scanCursor.  Note: struct is copied, it is not dynamic */
{
struct scanCursor sc;
sc.query = compCursorNew(geneComp);
sc.target = compCursorNew(targetComp);
return sc;
}

static boolean scanCursorIncr(struct scanCursor *sc)
/* increment scan cursor one position, return FALSE at end */
{
/* the < 'A' tests for '.', '-', '\0' so that we advance past the last position
 * in at the end of the string. */
if (compCursorAtBase(&sc->query))
    sc->query.pos++;
sc->query.ptr++;
if (compCursorAtBase(&sc->target))
    sc->target.pos++;
sc->target.ptr++;
return (*sc->query.ptr != '\0');
}

static boolean scanCursorAdvToQueryPos(struct scanCursor *sc, int qPos)
/* advance cursor to a position in the gene's component space */
{
while (sc->query.pos < qPos)
    {
    if (!scanCursorIncr(sc))
        return FALSE;
    }
return TRUE;
}

struct scanInfo
/* object that holds state while scanning the alignment for an exon */
{
    struct scanCursor *sc;  /* cursor into the alignment */
    struct cdsExon *exon;   /* exon being processed */
    int qStart;             /* exon coordinates mapped to query strand */
    int qEnd;
    int frame;              /* current frame */
    int scanDir;            /* scanning direction (+1|-1) relative to transcription */
    int subQStart;          /* query and target start of current subrange of exon,
                             * -1 for none */
    int subTStart;
    int subStartFrame;      /* frame frame subrange of exon, start scan start */
    int subEndFrame;
};

static void traceFrameDef(int level, struct scanInfo *si, struct mafFrames *mf)
/* verbose trace to indicate why a mafFrames record was defined */
{
verbose(level, "exon %s[%d] %s:%d-%d %c fm: %d; ",
        si->exon->gene, si->exon->iExon,
        si->exon->chrom, si->exon->chromStart, si->exon->chromEnd,
        si->exon->strand, si->exon->frame);
verbose(level, "subQuery %s:%d-%d %c fm: %d-%d; ",
        si->sc->query.comp->src, si->subQStart,
        si->sc->query.pos, si->sc->query.comp->strand, si->subStartFrame, si->subEndFrame);
verbose(level, "subTarget %s:%d-%d %c fm: %d\n",
        si->sc->target.comp->src, si->subTStart,
        si->sc->target.pos, si->sc->target.comp->strand, mf->frame);
}

static void addMafFrame(struct scanInfo *si)
/* add an MAF frame record for the current subrange to the exon. Assumes that
 * the cursor has gone past the last query column to include in record. Resets
 * the state.*/
{
struct mafFrames *mf;
char *dot;
AllocVar(mf);
mf->chrom = cloneString(strchr(si->sc->target.comp->src, '.')+1);
mf->chromStart = si->subTStart;
mf->chromEnd = si->sc->target.pos;

/* strip sequence name from src */
mf->src = cloneString(si->sc->query.comp->src);
dot = strchr(mf->src, '.');
if (dot != NULL)
    *dot = '\0';

/* frame is for first base in direction of transcription */
if (si->scanDir > 0)
    mf->frame = si->subStartFrame;
else
    mf->frame = si->subEndFrame;

/* project strand to target */
mf->strand[0] = (si->exon->strand == si->sc->query.comp->strand) ? '+' : '-';
if (si->sc->target.comp->strand == '-')
    mf->strand[0] = (mf->strand[0] == '+') ? '-' : '+';

mf->name = cloneString(si->exon->gene);
mf->prevEnd = -1;
mf->nextStart = -1;

slAddHead(&si->exon->frames, mf);

if (verboseLevel() >= 3)
    traceFrameDef(3, si, mf);

/* reset state */
si->subQStart = -1;
si->subTStart = -1;
si->subStartFrame = -1;
si->subEndFrame = -1;
}

static struct scanInfo scanInfoInit(struct scanCursor *sc, struct cdsExon *exon)
/* initialize scanInfo object for an exon.  This doesn't advance to the first
 * aligned position in the exon */
{
struct scanInfo si;
si.sc = sc;
si.exon = exon;
si.subTStart = -1;
si.subStartFrame = -1;
si.subEndFrame = -1;

/* genePred coordinates are always on the positive strand, we need to reverse
 * them to search the strand-specific MAF coords if the component is on the
 * negative strand */
si.qStart = exon->chromStart;
si.qEnd = exon->chromEnd;
if (sc->query.comp->strand == '-')
    reverseIntRange(&si.qStart, &si.qEnd, sc->query.comp->srcSize);

/* direction frame will increment */
si.scanDir = (exon->strand == sc->query.comp->strand) ? 1 : -1;

/* Get frame of the first base of the exon that will be scanned. */
si.frame =  (si.scanDir > 0) ? exon->frame
    : frameIncr(exon->frame, (exon->chromEnd-exon->chromStart)-1);
assertFrame(si.frame);
return si;
}

static void processColumn(struct scanInfo *si)
/* process one column of the alignment for an exon */
{
if (compCursorAtBase(&si->sc->query))
    {
    /* column contains query */
    if (compCursorAtBase(&si->sc->target))
        {
        /* target also aligns position */
        if (si->subTStart < 0)
            {
            /* remember start */
            si->subQStart = si->sc->query.pos;
            si->subTStart = si->sc->target.pos;
            si->subStartFrame = si->frame;
            }
        si->subEndFrame = si->frame;
        }
    else
        {
        /* target does not align position */
        if (si->subTStart >= 0)
            addMafFrame(si);
        }
    /* advance frame */
    si->frame = frameIncr(si->frame, si->scanDir); 
    }
}

static void mkCompExonFrames(struct scanCursor *sc, struct cdsExon *exon)
/* create mafFrames objects for a mafComp and an exon. */
{
struct scanInfo si = scanInfoInit(sc, exon);

/* advance to start of exon in component, which must be there due
 * to the bin search */
if (!scanCursorAdvToQueryPos(sc, si.qStart))
    errAbort("BUG: should have found exon in this component");
si.frame = frameIncr(si.frame, si.scanDir*(sc->query.pos - si.qStart));

/* scan columns of alignment overlapping exon */
while (sc->query.pos < si.qEnd)
    {
    processColumn(&si);
    if (!scanCursorIncr(si.sc))
        break;
    }
if (si.subTStart >= 0)
    addMafFrame(&si);
}

static void mkCompFrames(struct geneBins *genes, struct mafComp *geneComp,
                         struct mafComp *targetComp)
/* create mafFrames objects for an mafComp */
{
struct scanCursor sc = scanCursorNew(geneComp, targetComp);
struct binElement *exonRefs = geneBinsFind(genes, geneComp);
struct binElement *exonRef;

for (exonRef = exonRefs; exonRef != NULL; exonRef = exonRef->next)
    mkCompExonFrames(&sc, (struct cdsExon*)exonRef->val);

slFreeList(&exonRefs);
}

void mkMafFramesForMaf(char *geneDb, char *targetDb, struct geneBins *genes,
                       char *mafFilePath)
/* create mafFrames objects from an MAF file */
{
struct mafFile *mafFile = mafOpen(mafFilePath);
struct mafAli *ali;

while ((ali = mafNext(mafFile)) != NULL)
    {
    struct mafComp *geneComp = mafMayFindComponentDb(ali, geneDb);
    struct mafComp *targetComp = mafMayFindComponentDb(ali, targetDb);
    if ((geneComp != NULL) && (targetComp != NULL))
        mkCompFrames(genes, geneComp, targetComp);
    mafAliFree(&ali);
    }
mafFileFree(&mafFile);
}

static int mafFramesCmp(const void *va, const void *vb)
/* compare by target order */
{
const struct mafFrames *a = *((struct mafFrames **)va);
const struct mafFrames *b = *((struct mafFrames **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

static void sortExonFrames(struct binElement *exonRefs)
/* sort frames for all exons on a chrom */
{
struct binElement* exonRef;
for (exonRef = exonRefs; exonRef != NULL; exonRef = exonRef->next)
    {
    struct cdsExon *exon = exonRef->val;
    slSort(&exon->frames, mafFramesCmp);
    }
}

static int getPrevExonEnd(struct cdsExon *exon)
/* Get the chromEnd of the last frame of previous exon, or -1 if no previous
 * exon, or it is not aligned */
{
if (exon->prev != NULL)
    {
    struct mafFrames *mf = slLastEl(exon->prev->frames);
    if (mf != NULL)
        return mf->chromEnd;
    }
return -1;
}

static int getNextExonStart(struct cdsExon *exon)
/* Get the chromStart of the first frame of next exon, or -1 if no next
 * exon, or it is not aligned */
{
if ((exon->next != NULL) && (exon->next->frames != NULL))
    return exon->next->frames->chromStart;
return -1;
}

static void linkExonFrames(struct cdsExon *exon)
/* link mafFrames for an exon.  Assumes this is called in assending order, so
 * preceeding exon frames have been linked.  Does not require being called
 * in gene order.  */
{
int prevEnd = getPrevExonEnd(exon);
struct mafFrames *mf;
for (mf = exon->frames; mf != NULL; mf = mf->next)
    {
    mf->prevEnd = prevEnd;
    if (mf->next != NULL)
        mf->nextStart = mf->next->chromStart;
    else
        mf->nextStart = getNextExonStart(exon);
    prevEnd = mf->chromEnd;
    }
}

static void mkMafFramesFinishChrom(struct geneBins *genes, char *chrom)
/* link mafFrames for a chromosome */
{
struct binElement *exonRefs = geneBinsChromExons(genes, chrom);
struct binElement *exonRef;
sortExonFrames(exonRefs);
for (exonRef = exonRefs; exonRef != NULL; exonRef = exonRef->next)
    linkExonFrames((struct cdsExon*)exonRef->val);
slFreeList(&exonRefs);
}

void mkMafFramesFinish(struct geneBins *genes)
/* Finish mafFrames build, linking mafFrames prev/next fields */
{
struct slName *chroms = chromBinsGetChroms(genes->bins); 
struct slName *chrom;

for (chrom = chroms; chrom != NULL; chrom = chrom->next)
    mkMafFramesFinishChrom(genes, chrom->name);
slFreeList(&chroms);
}

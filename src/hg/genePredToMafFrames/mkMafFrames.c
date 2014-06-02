/* mkMafFrames - build mafFrames objects for exons, this does not handle
 * issues of linking mafFrames. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "genePred.h"
#include "frameIncr.h"
#include "maf.h"
#include "dnautil.h"
#include "orgGenes.h"
#include "chromBins.h"
#include "binRange.h"
#include "verbose.h"
#include "localmem.h"

/* assert for a frame being valid */
#define assertFrame(fr) assert(((fr) >= 0) && ((fr) <= 2))

/* data structure used to scan a mafComp object */
struct compCursor {
    struct mafComp *comp;   /* component object */
    int pos;                /* corresponding position in the sequence; it at a
                             * gap in the alignment, this is the next position
                             * in the sequence*/
    char *ptr;              /* pointer into text */
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
    int exonQStart;         /* exon coords mapped query seq strand */
    int exonQEnd;
    int blkQStart;          /* exon coords, mapped and adjust to block bounds */
    int blkQEnd;
    int frame;              /* current frame */
    int frScanDir;          /* frame scanning direction (+1|-1) relative to transcription */
    int subQStart;          /* query and target start of current subrange of exon,
                             * -1 for none */
    int subTStart;
    int subStartFrame;      /* frame frame subrange of exon, start scan start */
    int subEndFrame;
};

static void traceFrameDef(int level, struct scanInfo *si, struct exonFrames *ef)
/* verbose trace to indicate why a mafFrames record was defined */
{
verbose(level, "exon %s[%d] %s:%d-%d [%d] %c fm: %d; ",
        si->exon->gene->name, si->exon->exonNum,
        si->exon->gene->chrom, si->exon->chromStart, si->exon->chromEnd,
        (si->exon->chromEnd - si->exon->chromStart),
        si->exon->gene->strand, si->exon->frame);
verbose(level, "subQuery %s:%d-%d [%d] %c fm: %d-%d; ",
        si->sc->query.comp->src, si->subQStart, si->sc->query.pos,
        (si->sc->query.pos - si->subQStart),
        si->sc->query.comp->strand, si->subStartFrame, si->subEndFrame);
verbose(level, "subTarget %s:%d-%d [%d] %c fm: %d\n",
        si->sc->target.comp->src, si->subTStart, si->sc->target.pos, 
        (si->sc->target.pos - si->subTStart), 
        si->sc->target.comp->strand, ef->mf.frame);
}

static void addMafFrame(struct scanInfo *si)
/* add an MAF frame record for the current subrange to the exon. Assumes that
 * the cursor has gone past the last query column to include in record. Resets
 * the state.*/
{
struct exonFrames *ef;
char src[256], *tName, strand, *dot, frame;
int cdsOff;

/* target excludes organism */
tName = (strchr(si->sc->target.comp->src, '.')+1);

/* frame is for first base in direction of transcription */
if (si->frScanDir > 0)
    frame = si->subStartFrame;
else
    frame = si->subEndFrame;

/* project strand to target */
strand = (si->exon->gene->strand == si->sc->query.comp->strand) ? '+' : '-';
if (si->sc->target.comp->strand == '-')
    strand = (strand == '+') ? '-' : '+';

/* get src name, removing sequence id */
safef(src, sizeof(src), "%s", si->sc->query.comp->src);
dot = strchr(src, '.');
if (dot != NULL)
    *dot = '\0';

/* compute offset within CDS */
if (si->frScanDir > 0)
    cdsOff = si->exon->cdsOff + (si->subQStart - si->exonQStart);
else
    cdsOff = si->exon->cdsOff + (si->exonQEnd - si->sc->query.pos);

/* create and link exon */
ef = cdsExonAddFrames(si->exon, si->subQStart, si->sc->query.pos, si->sc->query.comp->strand,
                      tName, si->subTStart, si->sc->target.pos,
                      frame, strand, cdsOff);

if (verboseLevel() >= 2)
    traceFrameDef(2, si, ef);
assert((ef->srcEnd-ef->srcStart) == (ef->mf.chromEnd-ef->mf.chromStart));

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
int queryEnd = sc->query.comp->start + sc->query.comp->size;
int frameStart, frameEnd;

ZeroVar(&si);
si.sc = sc;
si.exon = exon;
si.subTStart = -1;
si.subStartFrame = -1;
si.subEndFrame = -1;

/* direction frame will increment */
assert(sc->target.comp->strand == '+');
si.frScanDir = (exon->gene->strand == sc->query.comp->strand) ? 1 : -1;

/* get coordinates and frame at each end of the exon */
si.exonQStart = exon->chromStart;
si.exonQEnd = exon->chromEnd;
frameStart = (exon->gene->strand == '+')
    ? exon->frame : frameIncr(exon->frame, (si.exonQEnd - si.exonQStart)-1);
frameEnd = (exon->gene->strand == '+')
    ? frameIncr(exon->frame, (si.exonQEnd - si.exonQStart)-1) : exon->frame;

/* genePred coordinates are always on the positive strand, we need to reverse
 * them to search the strand-specific MAF coords if the component is on the
 * negative strand */
if (sc->query.comp->strand == '-')
    {
    int hold = frameStart;
    frameStart = frameEnd;
    frameEnd = hold;
    reverseIntRange(&si.exonQStart, &si.exonQEnd, sc->query.comp->srcSize);
    }

/* adjust to bounds of query in maf block */
si.blkQStart = si.exonQStart;
si.blkQEnd = si.exonQEnd;
if (si.blkQStart < sc->query.comp->start)
    {
    int delta = sc->query.comp->start - si.blkQStart;
    si.blkQStart = sc->query.comp->start;
    frameStart = frameIncr(frameStart, si.frScanDir*delta);
    }
if (si.blkQEnd > queryEnd)
    {
    int delta = si.blkQEnd - queryEnd;
    si.blkQEnd = queryEnd;
    frameEnd = frameIncr(frameEnd, -1*si.frScanDir*delta);
    }
si.frame = frameStart;

/* Ugly side-affect: record src chrom size for gene the first time it's
 * encounter, since it was available when gene is read in */
if (exon->gene->chromSize == 0)
    exon->gene->chromSize = sc->query.comp->srcSize;
else if (exon->gene->chromSize != sc->query.comp->srcSize)
    errAbort("srcSize for %s differes in MAF; previous found as, %d, now %d",
             sc->query.comp->src, exon->gene->chromSize, sc->query.comp->srcSize);
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
            {
            addMafFrame(si);
            }
        }
    /* advance frame */
    si->frame = frameIncr(si->frame, si->frScanDir); 
    }
else
    {
    /* query not aligned, if target is aligned, output current frame */
    if (compCursorAtBase(&si->sc->target) && (si->subTStart >= 0))
        {
        addMafFrame(si);
        }
    }
}

static void mkCompExonFrames(struct scanCursor *sc, struct cdsExon *exon)
/* create mafFrames objects for a mafComp and an exon. */
{
struct scanInfo si = scanInfoInit(sc, exon);

/* advance to start of exon in component, which must be there due
 * to the bin search */
if (!scanCursorAdvToQueryPos(sc, si.blkQStart))
    errAbort("BUG: should have found exon in this component");
si.frame = frameIncr(si.frame, si.frScanDir*(sc->query.pos - si.blkQStart));

/* scan columns of alignment overlapping exon */
while (sc->query.pos < si.blkQEnd)
    {
    processColumn(&si);
    if (!scanCursorIncr(si.sc))
        break;
    }
if (si.subTStart >= 0)
    addMafFrame(&si);
}

static void mkCompFrames(struct orgGenes *genes,
                         struct mafComp *queryComp,
                         struct mafComp *targetComp)
/* create mafFrames objects for an mafComp */
{
struct scanCursor sc = scanCursorNew(queryComp, targetComp);

/* n.b. the order of scanning is very important here or will miss some exons
 * if they share a block */
int sortDir =  (queryComp->strand == targetComp->strand) ? 1 : -1;
struct binElement *exonRefs = orgGenesFind(genes, queryComp, sortDir);
struct binElement *exonRef;

for (exonRef = exonRefs; exonRef != NULL; exonRef = exonRef->next)
    mkCompExonFrames(&sc, (struct cdsExon*)exonRef->val);

slFreeList(&exonRefs);
}

static void mkFramesForOrg(struct mafAli *ali, struct mafComp *targetComp,
                           struct orgGenes *genes)
/* create frames for an organism and a maf alignment */
{
struct mafComp *queryComp = mafMayFindComponentDb(ali, genes->srcDb);
if (queryComp != NULL)
    mkCompFrames(genes, queryComp, targetComp);
}

void mkMafFramesForMaf(char *targetDb, struct orgGenes *orgs,
                       char *mafFilePath)
/* create mafFrames objects from an MAF file */
{
struct mafFile *mafFile = mafOpen(mafFilePath);
struct mafAli *ali;
struct orgGenes *genes;

while ((ali = mafNext(mafFile)) != NULL)
    {
    struct mafComp *targetComp = mafMayFindComponentDb(ali, targetDb);
    if (targetComp != NULL)
        {
        if (targetComp->strand == '-')
            mafFlipStrand(ali);  /* code requires target strand is + */
        for (genes = orgs; genes != NULL; genes = genes->next)
            mkFramesForOrg(ali, targetComp, genes);
        }
    mafAliFree(&ali);
    }
mafFileFree(&mafFile);
}

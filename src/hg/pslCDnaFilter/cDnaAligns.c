/* cDnaAligns - Objects to read and score sets of cDNA alignments. Filtering decissions are
 * not made here*/
#include "common.h"
#include "cDnaAligns.h"
#include "cDnaStats.h"
#include "psl.h"
#include "sqlNum.h"
#include "verbose.h"
#include "localmem.h"
#include "polyASize.h"

/* global control id alignment ids be added to qNames of PSLs being
 * written. User for debugging */
boolean cDnaAlignsAlnIdQNameMode = FALSE;

static void alignInfoVerb(int level, struct cDnaAlign *aln, char *desc)
/* print info about and alignment */
{
cDnaAlignVerb(5, aln, "%s: id=%0.3f cov=%0.3f rep=%0.3f alnPolyAT=%d score=%0.3f mat=%d mis=%d repMat=%d nCnt=%d adjAlnSize=%d",
              desc, aln->ident, aln->cover, aln->repMatch, aln->alnPolyAT, aln->score,
              aln->psl->match, aln->psl->misMatch, aln->psl->repMatch, aln->psl->nCount, aln->adjAlnSize);
}

static float calcCover(struct cDnaAlign *aln)
/* calculate coverage less poly-A tail */
{
unsigned totAlnSize = 0;  /* for sanity checking */
unsigned alnSize = 0;
int iBlk;

/* want to ignore actual poly-A, not just size diff, so need to count what is
 * not in poly-A. */
for (iBlk = 0; iBlk < aln->psl->blockCount; iBlk++)
    {
    struct range q = cDnaQueryBlk(aln->cdna, aln->psl, iBlk);
    if (q.start < q.end)
        alnSize += (q.end - q.start);
    totAlnSize += aln->psl->blockSizes[iBlk];
    }
if (aln->cdna->opts & cDnaIgnoreNs)
    alnSize -= aln->psl->nCount;
unsigned matchCnts = (aln->psl->match + aln->psl->misMatch + aln->psl->repMatch + aln->psl->nCount);
if (totAlnSize != matchCnts)
    cDnaAlignVerb(1, aln, "Warning: total alignment size (%d) doesn't match counts (%d)",
                  totAlnSize, matchCnts);
return ((float)alnSize)/((float)(aln->cdna->adjQEnd - aln->cdna->adjQStart));
}

static int alignMilliBadness(struct psl *psl, int adjMisMatch)
/* Determine a badness score.  This is the pslCalcMilliBad()
 * algorithm, with an option of not counting Ns. */
{
int sizeMul = pslIsProtein(psl) ? 3 : 1;
int milliBad = 0;
int qAliSize = sizeMul * (psl->qEnd - psl->qStart);
int tAliSize = psl->tEnd - psl->tStart;
int aliSize = min(qAliSize, tAliSize);
if (aliSize <= 0)
    return 0;
int sizeDif = qAliSize - tAliSize;
if (sizeDif < 0)
    sizeDif = 0;

int total = (sizeMul * (psl->match + psl->repMatch + adjMisMatch));
if (total != 0)
    milliBad = (1000 * (adjMisMatch*sizeMul + psl->qNumInsert + round(3*log(1+sizeDif)))) / total;
return milliBad;
}

static float intronFactor(struct psl *psl)
/* Figure bonus for having introns.  An intron is worth 3 bases... 
 * An intron in this case is just a gap of 0 bases in query and
 * 30 or more in target. */
{
int i, blockCount = psl->blockCount;
int ts, qs, te, qe, sz;
int bonus = 0;
if (blockCount <= 1)
    return 0;
sz = psl->blockSizes[0];
qe = psl->qStarts[0] + sz;
te = psl->tStarts[0] + sz;
for (i=1; i<blockCount; ++i)
    {
    qs = psl->qStarts[i];
    ts = psl->tStarts[i];
    if (qs == qe && ts - te >= 30)
        bonus += 3;
    sz = psl->blockSizes[i];
    qe = qs + sz;
    te = ts + sz;
    }
if (bonus > 10)
    bonus = 10;
return ((float)bonus)/1000.0;
}


static float sizeFactor(struct psl *psl)
/* Return a score factor that will favor longer alignments. */
{
return (4.0*sqrt(psl->match + psl->repMatch/4))/1000.0;
}

static float calcScore(struct psl *psl, int adjMisMatch, boolean ignoreIntrons)
/* calculate an alignment score, with weighting towards alignments with
 * introns and longer alignments.  The algorithm is based pslReps. */
{
return (1000.0-alignMilliBadness(psl, adjMisMatch))/1000.0 + sizeFactor(psl)
    + (ignoreIntrons ? 0 : intronFactor(psl));
}

static int getAlnPolyATLen(struct cDnaQuery *cdna,
                           struct psl *psl)
/* compute the number of bases of poly-A or poly-T that are aligned
 * in this alignment.*/
{
unsigned alnSize = 0;
int iBlk;
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    /* get block, adjusted for poly-A/T, and find what was omitted */
    struct range adj = cDnaQueryBlk(cdna, psl, iBlk);
    int start =  psl->qStarts[iBlk];
    int end = start+psl->blockSizes[iBlk];
    if (psl->strand[0] == '-')
        reverseIntRange(&start, &end, psl->qSize);
    alnSize += (adj.start - start) + (end - adj.end);
    }
return alnSize;
}

struct cDnaAlign *cDnaAlignNew(struct cDnaQuery *cdna, struct psl *psl)
/* construct a new object and add to the cdna list, updating the stats */
{
int adjMisMatch = psl->misMatch + ((cdna->opts & cDnaIgnoreNs) ? 0 : psl->nCount);
struct cDnaAlign *aln;
AllocVar(aln);
aln->cdna = cdna;
aln->psl = psl;
aln->alnId = cdna->numAln;
aln->alnPolyAT = getAlnPolyATLen(cdna, psl);
aln->adjAlnSize = ((psl->match + psl->repMatch + adjMisMatch) - aln->alnPolyAT);
aln->ident = pslIdent(psl);
aln->repMatch = ((float)psl->repMatch)/((float)(psl->match+psl->repMatch));
aln->score = calcScore(psl, adjMisMatch, (cdna->opts & cDnaIgnoreIntrons));
aln->cover = calcCover(aln);
assert(aln->alnPolyAT <= (psl->match+psl->misMatch+psl->repMatch+psl->nCount));

slAddHead(&cdna->alns, aln);
cdna->numAln++;
cdna->stats->totalCnts.aligns++;

if (verboseLevel() >= 5)
    alignInfoVerb(5, aln, "align");
return aln;
}

static void polyAAdjBounds(struct cDnaQuery *cdna,
                           struct polyASize *polyASize)
/* adjust mRNA bounds for poly-A/poly-Ts */
{
if (cdna->opts & cDnaUsePolyTHead)
    {
    /* use longest of poly-A tail or poly-T head */
    if (polyASize->headPolyTSize > polyASize->tailPolyASize)
        cdna->adjQStart += polyASize->headPolyTSize;
    else if (polyASize->tailPolyASize > polyASize->headPolyTSize)
        cdna->adjQEnd -= polyASize->tailPolyASize;
    }
else
    {
    /* adjust only with poly-A tail */
    cdna->adjQEnd -= polyASize->tailPolyASize;
    }
assert(cdna->adjQStart <= cdna->adjQEnd);
}

static void dropOne(struct cDnaAlign *aln, struct cDnaCnts *cnts, char *desc, char *reasonFmt, va_list ap)
/* flag an alignment as dropped */
{
assert(!aln->drop);       /* not allowing multiple drops keeps counts sane */
aln->drop = TRUE;
aln->cdna->numDrop++;
cnts->aligns++;
assert(aln->cdna->numDrop <= aln->cdna->numAln);
if (verboseLevel() >= 3)
    {
    char reasonBuf[512];
    va_list ap2;
    va_copy(ap2, ap);
    vasafef(reasonBuf, sizeof(reasonBuf), reasonFmt, ap2);
    va_end(ap2);
    cDnaAlignVerb(3, aln, "%s: %s", desc, reasonBuf);
    }
}

void cDnaAlignDrop(struct cDnaAlign *aln, boolean dropHapSetLinked,  struct cDnaCnts *cnts, char *reasonFmt, ...)
/* flag an alignment as dropped, optionally dropping linked in hapSet */
{
va_list ap;
va_start(ap, reasonFmt);
dropOne(aln, cnts, "drop", reasonFmt, ap);
if (dropHapSetLinked)
    {
    struct cDnaAlignRef *hapAln;
    for (hapAln = aln->hapAlns; hapAln != NULL; hapAln = hapAln->next)
        dropOne(hapAln->ref, cnts, "drop linked", reasonFmt, ap);
    }
va_end(ap);
}

struct cDnaQuery *cDnaQueryNew(unsigned opts, struct cDnaStats *stats,
                               struct psl *psl, struct polyASize *polyASize)
/* construct a new cDnaQuery.  This does not add an initial alignment.
 * polyASize is null if not available.*/
{
struct cDnaQuery *cdna;
AllocVar(cdna);
cdna->opts = opts;
cdna->stats = stats;
cdna->id = psl->qName;
cdna->adjQStart = 0;
cdna->adjQEnd = psl->qSize;
if (polyASize != NULL)
    polyAAdjBounds(cdna, polyASize);
return cdna;
}

struct range cDnaQueryBlk(struct cDnaQuery *cdna, struct psl *psl,
                          int iBlk)
/* Get the query range for a block of a psl, adjust to exclude a polyA tail or
 * a polyT head */
{
struct range r;
r.start = psl->qStarts[iBlk];
r.end = r.start+psl->blockSizes[iBlk];
if (psl->strand[0] == '-')
    reverseIntRange(&r.start, &r.end, psl->qSize);

/* don't include ignored poly-A/poly-Ts */
if (r.start < cdna->adjQStart)
    r.start = cdna->adjQStart;
if (r.end > cdna->adjQEnd)
    r.end = cdna->adjQEnd;
return r;
}


static void cDnaQueryAlignFree(struct cDnaQuery *cdna) 
/* free alignments associated with a cDnaQuery */
{
struct cDnaAlign *aln;
while ((aln = slPopHead(&cdna->alns)) != NULL)
    {
    pslFree(&aln->psl);
    slFreeList(&aln->hapAlns);
    freeMem(aln);
    }
 }

void cDnaQueryFree(struct cDnaQuery **cdnaPtr) 
/* free cDnaQuery and contained data  */
{
struct cDnaQuery *cdna = *cdnaPtr;
if (cdna != NULL)
    {
    slFreeList(&cdna->hapSets);
    cDnaQueryAlignFree(cdna);
    freeMem(cdna);
    *cdnaPtr = NULL;
    }
}

static int scoreRevCmp(const void *va, const void *vb)
/* Compare two alignments by cover+ident score */
{
const struct cDnaAlign *a = *((struct cDnaAlign **)va);
const struct cDnaAlign *b = *((struct cDnaAlign **)vb);
if (a->score < b->score)
    return 1;
if (a->score > b->score)
    return -1;
return 0;
}

void cDnaQueryRevScoreSort(struct cDnaQuery *cdna)
/* sort the alignments for this query by reverse cover+ident score */
{
slSort(&cdna->alns, scoreRevCmp);
}

void cDnaQueryWriteKept(struct cDnaQuery *cdna, FILE *outFh)
/* write the current set of psls that are flagged to keep */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop)
        {
        cdna->stats->keptCnts.aligns++;
        cDnaAlignOut(aln, outFh);
        if (aln->weirdOverlap)
            cdna->stats->weirdKeptCnts.aligns++;
        if (verboseLevel() >= 4)
            alignInfoVerb(4, aln, "keep");
        }
    }
}

void cDnaQueryWriteDrop(struct cDnaQuery *cdna, FILE *outFh)
/* write the current set of psls that are flagged to drop */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (aln->drop)
        cDnaAlignOut(aln, outFh);
    }
}

void cDnaQueryWriteWeird(struct cDnaQuery *cdna, FILE *outFh)
/* write the current set of psls that are flagged as weird overlap */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (aln->weirdOverlap)
        cDnaAlignOut(aln, outFh);
    }
}

void cDnaAlignPslOut(struct psl *psl, int alnId, FILE *fh)
/* output a PSL to a tab file.  If alnId is non-negative and
 * aldId out mode is set, append it to qName */
{
if (cDnaAlignsAlnIdQNameMode && (alnId >= 0))
    {
    char *qNameHold = psl->qName;
    char qNameId[512];
    safef(qNameId, sizeof(qNameId), "%s#%d", psl->qName, alnId);
    psl->qName = qNameId;
    pslTabOut(psl, fh);
    psl->qName = qNameHold;
    }
else
    pslTabOut(psl, fh);
}

void cDnaAlignOut(struct cDnaAlign *aln, FILE *fh)
/* Output a PSL to a tab file, include alnId in qname based on
 * alnIdQNameMode */
{
cDnaAlignPslOut(aln->psl, aln->alnId, fh);
}

void cDnaAlignVerbPsl(int level, struct psl *psl)
/* print psl location using verbose level */
{
verbose(level, "%s:%d-%d %s:%d-%d (%s)",
        psl->qName, psl->qStart, psl->qEnd,
        psl->tName, psl->tStart, psl->tEnd,
        psl->strand);
}

void cDnaAlignVerbLoc(int level, struct cDnaAlign *aln)
/* print aligment location using verbose level */
{
verbose(level, "[#%d]", aln->alnId);
cDnaAlignVerbPsl(level, aln->psl);
}

void cDnaAlignVerb(int level, struct cDnaAlign *aln, char *msg, ...)
/* write verbose messager followed by location of a cDNA alignment */
{
va_list ap;

va_start(ap, msg);
verboseVa(level, msg, ap);
verbose(level, ": ");
cDnaAlignVerbLoc(level, aln);
verbose(level, "\n");
va_end(ap);
}

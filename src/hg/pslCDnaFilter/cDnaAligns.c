/* Objects to read and score sets of cDNA alignments */
#include "common.h"
#include "cDnaAligns.h"
#include "psl.h"
#include "hash.h"
#include "sqlNum.h"
#include "verbose.h"
#include "linefile.h"
#include "localmem.h"
#include "polyASize.h"

static void alignInfoVerb(int level, struct cDnaAlign *aln, char *desc)
/* print info about and alignment */
{
cDnaAlignVerb(5, aln->psl, "%s: id=%0.4g cov=%0.4g rep=%0.4g alnPolyAT=%d score=%0.4g mat=%d mis=%d repMat=%d",
              desc, aln->ident, aln->cover, aln->repMatch, aln->alnPolyAT, aln->score,
              aln->psl->match, aln->psl->misMatch, aln->psl->repMatch);
}

static float calcIdent(struct psl *psl)
/* get fraction ident for a psl */
{
unsigned aligned = psl->match + psl->misMatch + psl->repMatch;
if (aligned == 0)
    return 0.0;
else
    return ((float)(psl->match + psl->repMatch))/((float)(aligned));
}

static float calcCover(struct cDnaQuery *cdna, struct psl *psl)
/* calculate coverage less poly-A tail */
{
unsigned alnSize = 0;
int iBlk;

/* want to ignore actual poly-A, not just size diff, so need to count what is
 * not in poly-A. */
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    struct cDnaRange q = cDnaQueryBlk(cdna, psl, iBlk);
    if (q.start < q.end)
        alnSize += (q.end - q.start);
    }
return ((float)alnSize)/((float)(cdna->adjQEnd - cdna->adjQStart));
}


static float intronFactor(struct psl *psl)
/* Figure bonus for having introns.  An intron is worth 3 bases... 
 * An intron in this case is just a gap of 0 bases in query and
 * 30 or more in target. */
{
/* FIXME should exclude polyA/T */
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

static float calcScore(struct psl *psl)
/* calculate an alignment score, with weighting towards alignments with
 * introns and longer alignments.  The algorithm is based pslReps. */
{
float score = (1000.0-pslCalcMilliBad(psl, TRUE))/1000.0;
score += sizeFactor(psl) + intronFactor(psl);
return score;
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
    struct cDnaRange adj = cDnaQueryBlk(cdna, psl, iBlk);
    int start =  psl->qStarts[iBlk];
    int end = start+psl->blockSizes[iBlk];
    if (psl->strand[0] == '-')
        reverseIntRange(&start, &end, psl->qSize);
    alnSize += (adj.start - start) + (end - adj.end);
    }
return alnSize;
}

static struct cDnaAlign *cDnaAlignNew(struct cDnaQuery *cdna,
                                      struct psl *psl)
/* construct a new object */
{
struct cDnaAlign *aln;
AllocVar(aln);
aln->psl = psl;
aln->ident = calcIdent(psl);
aln->cover = calcCover(cdna, psl);
aln->repMatch = ((float)psl->repMatch)/((float)(psl->match+psl->repMatch));
aln->score = calcScore(psl);
aln->alnPolyAT = getAlnPolyATLen(cdna, psl);
assert(aln->alnPolyAT <= (psl->match+psl->misMatch+psl->repMatch));

if (verboseLevel() >= 5)
    alignInfoVerb(5, aln, "align");
return aln;
}


static void polyAAdjBounds(struct cDnaQuery *cdna,
                           struct polyASize *polyASize)
/* adjust mRNA bounds for poly-A/poly-Ts */
{
if (cdna->reader->usePolyTHead)
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

static struct cDnaQuery *cDnaQueryNew(struct cDnaReader *reader, struct psl *psl,
                                      struct polyASize *polyASize)
/* construct a new cDnaQuery and add initial alignment.  polyASize is null if
 * not available.*/
{
struct cDnaQuery *cdna;
AllocVar(cdna);
cdna->reader = reader;
cdna->stats = &reader->stats;
cdna->id = psl->qName;
cdna->adjQStart = 0;
cdna->adjQEnd = psl->qSize;
if (polyASize != NULL)
    polyAAdjBounds(cdna, polyASize);
cdna->alns = cDnaAlignNew(cdna, psl);
return cdna;
}

struct cDnaRange cDnaQueryBlk(struct cDnaQuery *cdna, struct psl *psl,
                              int iBlk)
/* Get the query range for a block of a psl, adjust to exclude a polyA tail or
 * a polyT head */
{
struct cDnaRange r;
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

static void cDnaQueryFree(struct cDnaQuery **cdnaPtr) 
/* free cDnaQuery and contained data  */
{
struct cDnaQuery *cdna = *cdnaPtr;
if (cdna != NULL)
    {
    struct cDnaAlign *aln;
    while ((aln = slPopHead(&cdna->alns)) != NULL)
        {
        pslFree(&aln->psl);
        freeMem(aln);
        }
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

void cDnaQueryWriteKept(struct cDnaQuery *cdna,
                        FILE *outFh)
/* write the current set of psls that are flagged to keep */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop)
        {
        cdna->stats->keptCnts.aligns++;
        pslTabOut(aln->psl, outFh);
        if (aln->weirdOverlap)
            cdna->stats->weirdKeptCnts.aligns++;
        if (verboseLevel() >= 4)
            alignInfoVerb(4, aln, "keep");
        }
    }
}

void cDnaQueryWriteDrop(struct cDnaQuery *cdna,
                        FILE *outFh)
/* write the current set of psls that are flagged to drop */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (aln->drop)
        pslTabOut(aln->psl, outFh);
    }
}

void cDnaQueryWriteWeird(struct cDnaQuery *cdna,
                          FILE *outFh)
/* write the current set of psls that are flagged as weird overlap */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (aln->weirdOverlap)
        pslTabOut(aln->psl, outFh);
    }
}

static struct psl* readNextPsl(struct cDnaReader *reader)
/* Read the next psl. If one is save in alns, return it, otherwise
 * read the next one.  Discard invalid psls and try again */
{
struct psl* psl = NULL;
if (reader->nextCDnaPsl != NULL)
    {
    psl = reader->nextCDnaPsl;
    reader->nextCDnaPsl = NULL;
    }
else
    {
    char *row[PSLX_NUM_COLS];
    int nCols;
    if ((nCols = lineFileChopNextTab(reader->pslLf, row, ArraySize(row))) > 0)
        {
        if (nCols == PSL_NUM_COLS)
            psl = pslLoad(row);
        else if (nCols == PSLX_NUM_COLS)
            psl = pslxLoad(row);
        else
            errAbort("%s:%d: expected %d or %d columns, got %d",
                     reader->pslLf->fileName, reader->pslLf->lineIx,
                     PSL_NUM_COLS, PSLX_NUM_COLS, nCols);
        }
    }
return psl;
}

static void updateCounter(struct cDnaCnts *cnts)
/* update counts after processing a batch of queries */
{
if (cnts->aligns > cnts->prevAligns)
    cnts->queries++;
if (cnts->aligns > cnts->prevAligns+1)
    cnts->multAlnQueries++;
cnts->prevAligns = cnts->aligns;
}

static void updateCounters(struct cDnaReader *reader)
/* update counters after processing a batch of queries */
{
struct cDnaStats *stats = &reader->stats;
updateCounter(&stats->totalCnts);
updateCounter(&stats->badCnts);
updateCounter(&stats->keptCnts);
updateCounter(&stats->weirdOverCnts);
updateCounter(&stats->weirdKeptCnts);
updateCounter(&stats->minQSizeCnts);
updateCounter(&stats->overlapCnts);
updateCounter(&stats->minIdCnts);
updateCounter(&stats->minCoverCnts);
updateCounter(&stats->minAlnSizeCnts);
updateCounter(&stats->minNonRepSizeCnts);
updateCounter(&stats->maxRepMatchCnts);
updateCounter(&stats->maxAlignsCnts);
updateCounter(&stats->localBestCnts);
updateCounter(&stats->globalBestCnts);
updateCounter(&stats->minSpanCnts);
}

boolean cDnaReaderNext(struct cDnaReader *reader)
/* load the next set of cDNA alignments, return FALSE if no more */
{
struct psl *psl;
struct cDnaQuery *cdna;
struct polyASize *polyASize = NULL;

updateCounters(reader);
cDnaQueryFree(&reader->cdna);

/* first alignment */
psl = readNextPsl(reader);
if (psl == NULL)
    return FALSE;
if (reader->polyASizes != NULL)
    polyASize = hashFindVal(reader->polyASizes, psl->qName);
cdna = reader->cdna = cDnaQueryNew(reader, psl, polyASize);
reader->stats.totalCnts.aligns++;

/* remaining alignments for same sequence */
while (((psl = readNextPsl(reader)) != NULL)
       && sameString(psl->qName, cdna->id))
    {
    slSafeAddHead(&cdna->alns, cDnaAlignNew(cdna, psl));
    reader->stats.totalCnts.aligns++;
    }

reader->nextCDnaPsl = psl;  /* save for next time (or NULL) */
return TRUE;
}

struct cDnaReader *cDnaReaderNew(char *pslFile, boolean usePolyTHead, char *polyASizeFile)
/* construct a new object, opening the psl file */
{
struct cDnaReader *alns;
AllocVar(alns);
alns->pslLf = lineFileOpen(pslFile, TRUE);
alns->usePolyTHead = usePolyTHead;
if (polyASizeFile != NULL)
    alns->polyASizes = polyASizeLoadHash(polyASizeFile);
return alns;
}

void cDnaReaderFree(struct cDnaReader **readerPtr)
/* free object */
{
struct cDnaReader *reader = *readerPtr;
if (reader != NULL)
    {
    cDnaQueryFree(&reader->cdna);
    lineFileClose(&reader->pslLf);
    hashFree(&reader->polyASizes);
    freeMem(reader);
    *readerPtr = NULL;
    }
}

void cDnaAlignVerb(int level, struct psl *psl, char *msg, ...)
/* write verbose messager followed by location of a cDNA alignment */
{
va_list ap;

va_start(ap, msg);
verboseVa(level, msg, ap);
verbose(level, ": ");
verbose(level, "%s:%d-%d %s:%d-%d (%s)",
        psl->qName, psl->qStart, psl->qEnd,
        psl->tName, psl->tStart, psl->tEnd,
        psl->strand);
verbose(level, "\n");
va_end(ap);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


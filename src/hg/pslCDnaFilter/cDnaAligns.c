#include "common.h"
#include "cDnaAligns.h"
#include "psl.h"
#include "hash.h"
#include "sqlNum.h"
#include "verbose.h"
#include "linefile.h"
#include "localmem.h"
#include "polyASizes.h"

static void loadPolyASizeRec(struct hash *sizeMap, char **row)
/* load one polyA size record into the hash table.  This
 * places in hash localmem to reduce memory usage */
{
struct polyASizes pas, *pasRec;
struct hashEl *recHel;

polyASizesStaticLoad(row, &pas);
recHel = hashStore(sizeMap, pas.id);
if (recHel->val != NULL)
    {
    /* record already exists for this id, validate */
    pasRec = recHel->val;
    if ((pasRec->seqSize != pas.seqSize)
        ||(pasRec->tailPolyASize != pas.tailPolyASize)
        || (pasRec->headPolyTSize != pas.headPolyTSize))
        errAbort("multiple polyA size records for %s with different data", pas.id);
    }
else
    {
    /* add new record */
    lmAllocVar(sizeMap->lm, pasRec);
    pasRec->id = recHel->name;
    pasRec->seqSize = pas.seqSize;
    pasRec->tailPolyASize = pas.tailPolyASize;
    pasRec->headPolyTSize = pas.headPolyTSize;
    }
}

static struct hash *loadPolyASizes(char* polyASizeFile)
/* load coverage query sizes */
{
struct lineFile *lf = lineFileOpen(polyASizeFile, TRUE);
char *row[POLYASIZES_NUM_COLS];
struct hash *sizeMap = hashNew(21);

while (lineFileNextRowTab(lf, row, ArraySize(row)))
    loadPolyASizeRec(sizeMap, row);

lineFileClose(&lf);
return sizeMap;
}

static int getPolyASize(struct cDnaAligns *cdAlns, char* id)
/* get polyA size, or zero if we don't have it */
{
int size = 0;
if (cdAlns->polyASizes != NULL)
    {
    struct polyASizes *pasRec = hashFindVal(cdAlns->polyASizes, id);
    if (pasRec != NULL)
        size = pasRec->tailPolyASize;
    }
return size;
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

static float calcCover(struct cDnaAligns *cdAlns, struct psl *psl)
/* calculate coverage less poly-A tail */
{
unsigned polyASize = getPolyASize(cdAlns, psl->qName);
unsigned useSize = psl->qSize - polyASize;
unsigned alnSize = 0;
int iBlk;

/* want to ignore actual poly-A, not just size diff, so need to count what is
 * not in poly-A. */
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    unsigned qStart = psl->qStarts[iBlk];
    unsigned qEnd = qStart+psl->blockSizes[iBlk];
    if (psl->strand[0] == '+')
        {
        if (qEnd > useSize)
            qEnd = useSize;
        }
    else
        {
        if (qStart < polyASize)
            qStart = polyASize;
        }
    if (qStart < qEnd)
        alnSize += (qEnd - qStart);
    }
if (useSize == 0)
    return 0.0;
else
    return ((float)alnSize)/((float)useSize);
}

static struct cDnaAlign *cDnaAlignNew(struct cDnaAligns *cdAlns, struct psl *psl)
/* construct a new object */
{
struct cDnaAlign *cdAln;
AllocVar(cdAln);
cdAln->psl = psl;
cdAln->ident = calcIdent(psl);
cdAln->cover = calcCover(cdAlns, psl);
cdAln->score = (cdAlns->coverWeight*cdAln->cover)
    + ((1.0-cdAlns->coverWeight)*cdAln->ident);
cdAln->repMatch = ((float)psl->repMatch)/((float)(psl->match+psl->repMatch));

if (verboseLevel() >= 5)
    cDnaAlignVerb(5, psl, "align: id=%0.4g cov=%0.4g rep=%0.4g",
                  cdAln->ident, cdAln->cover, cdAln->repMatch);
return cdAln;
}

static void cDnaAlignFree(struct cDnaAlign **cdAlnPtr)
/* free object */
{
struct cDnaAlign *cdAln = *cdAlnPtr;
if (cdAln != NULL)
    {
    pslFree(&cdAln->psl);
    freeMem(cdAln);
    }
}

static void cDnaAlignsClean(struct cDnaAligns *cdAlns)
/* remove current cDNA from object */
{
cdAlns->id = NULL;
while (cdAlns->alns != NULL)
    {
    struct cDnaAlign *cdAln = slPopHead(&cdAlns->alns);
    cDnaAlignFree(&cdAln);
    }
}

static struct psl* readNextPsl(struct cDnaAligns *cdAlns)
/* Read the next psl. If one is save in cdAlns, return it, others
 * read the next one.  Discard invalid psls and try again */
{
struct psl* psl = NULL;
if (cdAlns->nextCDnaPsl != NULL)
    {
    psl = cdAlns->nextCDnaPsl;
    cdAlns->nextCDnaPsl = NULL;
    }
else
    {
    char *row[PSLX_NUM_COLS];
    int nCols;
    if ((nCols = lineFileChopNextTab(cdAlns->pslLf, row, ArraySize(row))) > 0)
        {
        if (nCols == PSL_NUM_COLS)
            psl = pslLoad(row);
        else if (nCols == PSLX_NUM_COLS)
            psl = pslxLoad(row);
        else
            errAbort("%s:%d: expected %d or %d columns, got %d",
                     cdAlns->pslLf->fileName, cdAlns->pslLf->lineIx,
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

static void updateCounters(struct cDnaAligns *cdAlns)
/* update counters after processing a batch of queries */
{
updateCounter(&cdAlns->totalCnts);
updateCounter(&cdAlns->badCnts);
updateCounter(&cdAlns->keptCnts);
updateCounter(&cdAlns->weirdOverCnts);
updateCounter(&cdAlns->weirdKeptCnts);
updateCounter(&cdAlns->minQSizeCnts);
updateCounter(&cdAlns->overlapCnts);
updateCounter(&cdAlns->minIdCnts);
updateCounter(&cdAlns->idTopCnts);
updateCounter(&cdAlns->minCoverCnts);
updateCounter(&cdAlns->coverTopCnts);
updateCounter(&cdAlns->maxRepMatchCnts);
updateCounter(&cdAlns->maxAlignsCnts);
updateCounter(&cdAlns->minSpanCnts);
}

boolean cDnaAlignsNext(struct cDnaAligns *cdAlns)
/* load the next set of cDNA alignments, return FALSE if no more */
{
struct psl *psl;

updateCounters(cdAlns);
cDnaAlignsClean(cdAlns);

/* first alignment */
psl = readNextPsl(cdAlns);
if (psl == NULL)
    return FALSE;
cdAlns->id = psl->qName;
cdAlns->alns = cDnaAlignNew(cdAlns, psl);
cdAlns->totalCnts.aligns++;

/* remaining alignments for same sequence */
while (((psl = readNextPsl(cdAlns)) != NULL)
       && sameString(psl->qName, cdAlns->id))
    {
    slSafeAddHead(&cdAlns->alns, cDnaAlignNew(cdAlns, psl));
    cdAlns->totalCnts.aligns++;
    }

cdAlns->nextCDnaPsl = psl;  /* save for next time (or NULL) */
return TRUE;
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

void cDnaAlignsRevScoreSort(struct cDnaAligns *cdAlns)
/* sort the alignments for this query by reverse cover+ident score */
{
slSort(&cdAlns->alns, scoreRevCmp);
}

void cDnaAlignsWriteKept(struct cDnaAligns *cdAlns,
                         FILE *outFh)
/* write the current set of psls that are flagged to keep */
{
struct cDnaAlign *cdAln;
for (cdAln = cdAlns->alns; cdAln != NULL; cdAln = cdAln->next)
    {
    if (!cdAln->drop)
        {
        cdAlns->keptCnts.aligns++;
        pslTabOut(cdAln->psl, outFh);
        if (cdAln->weirdOverlap)
            cdAlns->weirdKeptCnts.aligns++;
        if (verboseLevel() >= 4)
            cDnaAlignVerb(4, cdAln->psl, "keep: id=%0.4g cov=%0.4g", cdAln->ident, cdAln->cover);
        }
    }
}

void cDnaAlignsWriteDrop(struct cDnaAligns *cdAlns,
                         FILE *outFh)
/* write the current set of psls that are flagged to drop */
{
struct cDnaAlign *cdAln;
for (cdAln = cdAlns->alns; cdAln != NULL; cdAln = cdAln->next)
    {
    if (cdAln->drop)
        pslTabOut(cdAln->psl, outFh);
    }
}

void cDnaAlignsWriteWeird(struct cDnaAligns *cdAlns,
                          FILE *outFh)
/* write the current set of psls that are flagged as weird overlap */
{
struct cDnaAlign *cdAln;
for (cdAln = cdAlns->alns; cdAln != NULL; cdAln = cdAln->next)
    {
    if (cdAln->weirdOverlap)
        pslTabOut(cdAln->psl, outFh);
    }
}

struct cDnaAligns *cDnaAlignsNew(char *pslFile, float coverWeight,
                                 char *polyASizeFile)
/* construct a new object, opening the psl file */
{
struct cDnaAligns *cdAlns;
AllocVar(cdAlns);
cdAlns->pslLf = lineFileOpen(pslFile, TRUE);
cdAlns->coverWeight = coverWeight;
if (polyASizeFile != NULL)
    cdAlns->polyASizes = loadPolyASizes(polyASizeFile);
return cdAlns;
}

void cDnaAlignsFree(struct cDnaAligns **cdAlnsPtr)
/* free object */
{
struct cDnaAligns *cdAlns = *cdAlnsPtr;
if (cdAlns != NULL)
    {
    cDnaAlignsClean(cdAlns);
    lineFileClose(&cdAlns->pslLf);
    hashFree(&cdAlns->polyASizes);
    freeMem(cdAlns);
    }
}

void cDnaAlignVerb(int level, struct psl *psl, char *msg, ...)
/* write verbose messager followed by location of a cDNA alignment */
{
va_list ap;

va_start(ap, msg);
verboseVa(level, msg, ap);
verbose(level, ": ");
verbose(level, "%s %s:%d-%d/%s", psl->qName, psl->tName,
        psl->tStart, psl->tEnd, psl->strand);
verbose(level, "\n");
va_end(ap);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


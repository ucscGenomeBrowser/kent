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

static boolean validPsl(struct psl *psl)
/* check if a psl is internally consistent */
{
static FILE *devNull = NULL;
if (devNull == NULL)
    devNull = mustOpen("/dev/null", "w");
return (pslCheck("", devNull, psl) == 0);
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

if (verboseLevel() >= 4)
    cDnaAlignVerb(4, psl, "align: id=%g cov=%g", cdAln->ident, cdAln->cover);
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
struct psl* badPsls = NULL;  /* save till end for stat collection */
if (cdAlns->nextCDnaPsl != NULL)
    {
    psl = cdAlns->nextCDnaPsl;
    cdAlns->nextCDnaPsl = NULL;
    }
while (psl == NULL)
    {
    if ((psl = pslNext(cdAlns->pslLf)) == NULL)
        break; /* EOF */
    cdAlns->totalCnts.aligns++;
    if (!validPsl(psl))
        {
        cDnaAlignVerb(2, psl, "drop: invalid psl");
        cdAlns->badCnts.aligns++;
        if ((badPsls == NULL) || !sameString(badPsls->qName, psl->qName))
            cdAlns->badCnts.queries++;
        slAddHead(&badPsls, psl);
        psl = NULL;
        }
    }
pslFreeList(&badPsls);
return psl;
}

static void updateCounter(struct cDnaCnts *cnts)
/* update counts after processing a batch of queries */
{
if (cnts->aligns > cnts->prevAligns)
    cnts->queries++;
cnts->prevAligns = cnts->aligns;
}

static void updateCounters(struct cDnaAligns *cdAlns)
/* update counters after processing a batch of queries */
{
/* N.B. badCnts is handled by readNextPsl */
updateCounter(&cdAlns->totalCnts);
updateCounter(&cdAlns->keptCnts);
updateCounter(&cdAlns->weirdOverCnts);
updateCounter(&cdAlns->overlapCnts);
updateCounter(&cdAlns->minIdCnts);
updateCounter(&cdAlns->idTopCnts);
updateCounter(&cdAlns->minCoverCnts);
updateCounter(&cdAlns->coverTopCnts);
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

/* remaining alignments for same sequence */
while (((psl = readNextPsl(cdAlns)) != NULL)
       && sameString(psl->qName, cdAlns->id))
    slSafeAddHead(&cdAlns->alns, cDnaAlignNew(cdAlns, psl));

cdAlns->nextCDnaPsl = psl;  /* save for next time (or NULL) */
return TRUE;
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

struct cDnaAligns *cDnaAlignsNew(char *pslFile, char *polyASizeFile)
/* construct a new object, opening the psl file */
{
struct cDnaAligns *cdAlns;
AllocVar(cdAlns);
cdAlns->pslLf = pslFileOpen(pslFile);
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

/* FIXME: don't need both?? */
void cDnaAlignVerbPsl(int level, struct psl *psl)
/* write verbose info describing the location of a cDNA alignment */
{
verbose(level, "%s %s:%d-%d/%s", psl->qName, psl->tName,
        psl->tStart, psl->tEnd, psl->strand);
}

void cDnaAlignVerb(int level, struct psl *psl, char *msg, ...)
/* write verbose messager followed by location of a cDNA alignment */
{
va_list ap;

va_start(ap, msg);
verboseVa(level, msg, ap);
verbose(level, ": ");
cDnaAlignVerbPsl(level, psl);
verbose(level, "\n");
va_end(ap);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


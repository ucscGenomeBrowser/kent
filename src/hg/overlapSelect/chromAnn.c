/* chromAnn - chomosome annotations, generic object to store annotations from
 * other formats */
#include "common.h"
#include "chromAnn.h"
#include "binRange.h"
#include "rowReader.h"
#include "psl.h"
#include "bed.h"
#include "chain.h"
#include "genePred.h"
#include "coordCols.h"
#include "verbose.h"

static struct chromAnnBlk* chromAnnBlkNew(struct chromAnn *ca, int start, int end)
/* create new block object and add to chromAnn object */
{
struct chromAnnBlk* caBlk;
AllocVar(caBlk);
if (end < start)
    errAbort("invalid block coordinates for %s: start=%d end=%d", ca->name, start, end);

caBlk->ca = ca;;
caBlk->start = start;
caBlk->end = end;

if (ca->blocks == NULL)
    {
    ca->start = start;
    ca->end = end;
    }    
else
    {
    ca->start = min(ca->start, start);
    ca->end = max(ca->end, end);
    }
ca->totalSize += (end - start);
slAddHead(&ca->blocks, caBlk);
return caBlk;
}

static void chromAnnBlkFreeList(struct chromAnnBlk *blks)
/* free list of objects */
{
struct chromAnnBlk *blk;
while ((blk = slPopHead(&blks)) != NULL)
    freeMem(blk);
}

static struct chromAnn* chromAnnNew(char* chrom, char strand, char* name, void *rec,
                                    void (*recWrite)(struct chromAnn*, FILE *, char),
                                    void (*recFree)(struct chromAnn *))
/* create new object, ownership of rawCols is passed */
{
struct chromAnn* ca;
AllocVar(ca);
ca->chrom = cloneString(chrom);
ca->strand = strand;
if (name != NULL)
    ca->name = cloneString(name);
ca->start = 0;
ca->end = 0; 
ca->rec = rec;
ca->recWrite = recWrite;
ca->recFree = recFree;
return ca;
}

static int chromAnnBlkCmp(const void *va, const void *vb)
/* sort compare of two chromAnnBlk objects */
{
const struct chromAnnBlk *a = *((struct chromAnnBlk **)va);
const struct chromAnnBlk *b = *((struct chromAnnBlk **)vb);
int diff = a->start - b->start;
if (diff == 0)
    diff = a->end - b->end;
return diff;
}

static void chromAnnFinish(struct chromAnn* ca)
/* finish creation of a chromAnn after all blocks are added */
{
slSort(&ca->blocks, chromAnnBlkCmp);
}

void chromAnnFree(struct chromAnn **caPtr)
/* free an object */
{
struct chromAnn *ca = *caPtr;
if (ca != NULL)
    {
    ca->recFree(ca);
    freeMem(ca->chrom);
    freeMem(ca->name);
    chromAnnBlkFreeList(ca->blocks);
    freez(caPtr);
    }
}

int chromAnnTotalBlockSize(struct chromAnn* ca)
/* count the total bases in the blocks of a chromAnn */
{
int bases = 0;
struct chromAnnBlk *cab;
for (cab = ca->blocks; cab != NULL; cab = cab->next)
    bases += (cab->end - cab->start);
return bases;
}

static void strVectorWrite(struct chromAnn *ca, FILE *fh, char term)
/* write a chromAnn that is represented as a vector of strings */
{
char **cols = ca->rec;
assert(cols != NULL);
int i;
for (i = 0; cols[i] != NULL; i++)
    {
    if (i > 0)
        putc_unlocked('\t', fh);
    fputs(cols[i], fh);
    }
putc_unlocked(term, fh);
}

static void strVectorFree(struct chromAnn *ca)
/* free chromAnn data that is represented as a vector of strings */
{
freez(&ca->rec);
}

static void addBedBlocks(struct chromAnn* ca, unsigned opts, struct bed* bed)
/* add blocks from a bed */
{
int iBlk;
for (iBlk = 0; iBlk < bed->blockCount; iBlk++)
    {
    int start = bed->chromStart + bed->chromStarts[iBlk];
    int end = start + bed->blockSizes[iBlk];
    if (opts & chromAnnCds)
        {
        if (start < bed->thickStart)
            start = bed->thickStart;
        if (end > bed->thickEnd)
            end = bed->thickEnd;
        }
    if (start < end)
        chromAnnBlkNew(ca, start, end);
    }
}

static struct chromAnn* chromAnnBedReaderRead(struct chromAnnReader *car)
/* read next BED and convert to a chromAnn */
{
struct rowReader *rr = car->data;
if (!rowReaderNext(rr))
    return NULL;
rowReaderExpectAtLeast(rr, 3);

char **rawCols = (car->opts & chromAnnSaveLines) ? rowReaderCloneColumns(rr) : NULL;
struct bed *bed = bedLoadN(rr->row, rr->numCols);
struct chromAnn *ca = chromAnnNew(bed->chrom, bed->strand[0], bed->name, rawCols,
                                  strVectorWrite, strVectorFree);

if ((bed->blockCount == 0) || (car->opts & chromAnnRange))
    {
    if (car->opts & chromAnnCds)
        {
        if (bed->thickStart < bed->thickEnd)
            chromAnnBlkNew(ca, bed->thickStart, bed->thickEnd);
        }
    else
        chromAnnBlkNew(ca, bed->chromStart, bed->chromEnd);
    }
else
    addBedBlocks(ca, car->opts, bed);

chromAnnFinish(ca);
bedFree(&bed);
return ca;
}

static void chromAnnBedReaderFree(struct chromAnnReader **carPtr)
/* free object */
{
struct chromAnnReader *car = *carPtr;
if (car != NULL)
    {
    struct rowReader *rr = car->data;
    rowReaderFree(&rr);
    freez(carPtr);
    }
}

struct chromAnnReader *chromAnnBedReaderNew(char *fileName, unsigned opts)
/* construct a reader for a BED file */
{
struct chromAnnReader *car;
AllocVar(car);
car->caRead = chromAnnBedReaderRead;
car->carFree = chromAnnBedReaderFree;
car->opts = opts;
car->data = rowReaderOpen(fileName, FALSE);
return car;
}

static void addGenePredBlocks(struct chromAnn* ca, unsigned opts, struct genePred* gp)
/* add blocks from a genePred */
{
int iExon;
for (iExon = 0; iExon < gp->exonCount; iExon++)
    {
    int start = gp->exonStarts[iExon];
    int end = gp->exonEnds[iExon];
    if ((opts & chromAnnCds) && (gp->cdsStart > start))
        start = gp->cdsStart;
    if ((opts & chromAnnCds) && (gp->cdsEnd < end))
        end = gp->cdsEnd;
    if (start < end)
        chromAnnBlkNew(ca, start, end);
    }
}

static struct chromAnn* chromAnnGenePredReaderRead(struct chromAnnReader *car)
/* Read the next genePred row and create a chromAnn object row read from a
 * GenePred file or table.  If there is no CDS, and chromAnnCds is specified,
 * it will return a record with zero-length range.*/
{
struct rowReader *rr = car->data;
if (!rowReaderNext(rr))
    return NULL;
rowReaderExpectAtLeast(rr, GENEPRED_NUM_COLS);

char **rawCols = (car->opts & chromAnnSaveLines) ? rowReaderCloneColumns(rr) : NULL;
struct genePred *gp = genePredLoad(rr->row);
struct chromAnn* ca = chromAnnNew(gp->chrom, gp->strand[0], gp->name, rawCols,
                                  strVectorWrite, strVectorFree);

if (car->opts & chromAnnRange)
    {
    if (car->opts & chromAnnCds)
        {
        if (gp->cdsStart < gp->cdsEnd)
            chromAnnBlkNew(ca, gp->cdsStart, gp->cdsEnd);
        }
    else
        chromAnnBlkNew(ca, gp->txStart, gp->txEnd);
    }
else
    addGenePredBlocks(ca, car->opts, gp);

chromAnnFinish(ca);
genePredFree(&gp);
return ca;
}

static void chromAnnGenePredReaderFree(struct chromAnnReader **carPtr)
/* free object */
{
struct chromAnnReader *car = *carPtr;
if (car != NULL)
    {
    struct rowReader *rr = car->data;
    rowReaderFree(&rr);
    freez(carPtr);
    }
}

struct chromAnnReader *chromAnnGenePredReaderNew(char *fileName, unsigned opts)
/* construct a reader for a genePred file */
{
struct chromAnnReader *car;
AllocVar(car);
car->caRead = chromAnnGenePredReaderRead;
car->carFree = chromAnnGenePredReaderFree;
car->opts = opts;
car->data = rowReaderOpen(fileName, FALSE);
return car;
}

static void addPslBlocks(struct chromAnn* ca, unsigned opts, struct psl* psl)
/* add blocks from a psl */
{
boolean strand = (opts & chromAnnUseQSide) ? pslQStrand(psl) : pslTStrand(psl);
int size = (opts & chromAnnUseQSide) ? psl->qSize : psl->tSize;
unsigned *blocks = (opts & chromAnnUseQSide) ? psl->qStarts : psl->tStarts;
boolean blkSizeMult = pslIsProtein(psl) ? 3 : 1;
int iBlk;
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    int start = blocks[iBlk];
    int end = start + (blkSizeMult * psl->blockSizes[iBlk]);
    if (strand == '-')
        reverseIntRange(&start, &end, size);
    chromAnnBlkNew(ca, start, end);
    }
}

static struct chromAnn* chromAnnPslReaderRead(struct chromAnnReader *car)
/* read next chromAnn from a PSL file  */
{
struct rowReader *rr = car->data;
if (!rowReaderNext(rr))
    return NULL;
rowReaderExpectAtLeast(rr, PSL_NUM_COLS);

char **rawCols = (car->opts & chromAnnSaveLines) ? rowReaderCloneColumns(rr) : NULL;

struct psl *psl = pslLoad(rr->row);
struct chromAnn* ca;
if (car->opts & chromAnnUseQSide)
    ca = chromAnnNew(psl->qName, pslQStrand(psl), psl->tName, rawCols,
                     strVectorWrite, strVectorFree);
else
    ca = chromAnnNew(psl->tName, pslTStrand(psl), psl->qName, rawCols,
                     strVectorWrite, strVectorFree);

if (car->opts & chromAnnRange)
    {
    if (car->opts & chromAnnUseQSide)
        chromAnnBlkNew(ca, psl->qStart, psl->qEnd);
    else
        chromAnnBlkNew(ca, psl->tStart, psl->tEnd);
    }
else    
    addPslBlocks(ca, car->opts, psl);
chromAnnFinish(ca);
pslFree(&psl);
return ca;
}

static void chromAnnPslReaderFree(struct chromAnnReader **carPtr)
/* free object */
{
struct chromAnnReader *car = *carPtr;
if (car != NULL)
    {
    struct rowReader *rr = car->data;
    rowReaderFree(&rr);
    freez(carPtr);
    }
}

struct chromAnnReader *chromAnnPslReaderNew(char *fileName, unsigned opts)
/* construct a reader for a PSL file */
{
struct chromAnnReader *car;
AllocVar(car);
car->caRead = chromAnnPslReaderRead;
car->carFree = chromAnnPslReaderFree;
car->opts = opts;
car->data = rowReaderOpen(fileName, FALSE);
return car;
}

static void addChainQBlocks(struct chromAnn* ca, unsigned opts, struct chain* chain)
/* add query blocks from a chain */
{
struct cBlock *blk;
for (blk = chain->blockList; blk != NULL; blk = blk->next)
    {
    int start = blk->qStart;
    int end = blk->qEnd;
    if (chain->qStrand == '-')
        reverseIntRange(&start, &end, chain->qSize);
    chromAnnBlkNew(ca, start, end);
    }
}

static void addChainTBlocks(struct chromAnn* ca, unsigned opts, struct chain* chain)
/* add target blocks from a chain */
{
struct cBlock *blk;
for (blk = chain->blockList; blk != NULL; blk = blk->next)
    chromAnnBlkNew(ca, blk->tStart, blk->tEnd);
}

struct chromAnnChainReader
/* reader data for tab files */
{
    struct lineFile *lf;
};

static void chainRecWrite(struct chromAnn *ca, FILE *fh, char term)
/* write a chromAnn that is chain */
{
struct chain *chain = ca->rec;
assert(term == '\n');
chainWrite(chain, fh);
}

static void chainRecFree(struct chromAnn *ca)
/* free chromAnn chain data */
{
chainFree((struct chain**)&ca->rec);
}

static struct chromAnn* chromAnnChainReaderRead(struct chromAnnReader *car)
/* read a chromAnn object from a tab file or table */
{
struct chromAnnChainReader *carr = car->data;
struct chain *chain = chainRead(carr->lf);
if (chain == NULL)
    return NULL;

struct chromAnn* ca;
if (car->opts & chromAnnUseQSide)
    ca = chromAnnNew(chain->qName, chain->qStrand, chain->tName,
                     ((car->opts & chromAnnSaveLines) ? chain : NULL),
                     chainRecWrite, chainRecFree);
else
    ca = chromAnnNew(chain->tName, '+', chain->qName,
                     ((car->opts & chromAnnSaveLines) ? chain : NULL),
                     chainRecWrite, chainRecFree);

if (car->opts & chromAnnRange)
    {
    if (car->opts & chromAnnUseQSide)
        chromAnnBlkNew(ca, chain->qStart, chain->qEnd);
    else
        chromAnnBlkNew(ca, chain->tStart, chain->tEnd);
    }
else    
    {
    if (car->opts & chromAnnUseQSide)
        addChainQBlocks(ca, car->opts, chain);
    else
        addChainTBlocks(ca, car->opts, chain);
    }
chromAnnFinish(ca);
if (!(car->opts & chromAnnSaveLines))
    chainFree(&chain);
return ca;
}

static void chromAnnChainReaderFree(struct chromAnnReader **carPtr)
/* free object */
{
struct chromAnnReader *car = *carPtr;
if (car != NULL)
    {
    struct chromAnnChainReader *carr = car->data;
    lineFileClose(&carr->lf);
    freeMem(carr);
    freez(carPtr);
    }
}

struct chromAnnReader *chromAnnChainReaderNew(char *fileName, unsigned opts)
/* construct a reader for an arbitrary tab file. */
{
struct chromAnnChainReader *carr;
AllocVar(carr);
carr->lf = lineFileOpen(fileName, TRUE);

struct chromAnnReader *car;
AllocVar(car);
car->caRead = chromAnnChainReaderRead;
car->carFree = chromAnnChainReaderFree;
car->opts = opts;
car->data = carr;
return car;
}

struct chromAnnTabReader
/* reader data for tab files */
{
    struct coordCols  cols;  // column spec
    struct rowReader *rr;    // tab row reader
};

static struct chromAnn* chromAnnTabReaderRead(struct chromAnnReader *car)
/* read a chromAnn object from a tab file or table */
{
struct chromAnnTabReader *catr = car->data;
if (!rowReaderNext(catr->rr))
    return NULL;
rowReaderExpectAtLeast(catr->rr, catr->cols.minNumCols);

char **rawCols = (car->opts & chromAnnSaveLines) ? rowReaderCloneColumns(catr->rr) : NULL;
struct coordColVals colVals = coordColParseRow(&catr->cols, catr->rr);

struct chromAnn *ca = chromAnnNew(colVals.chrom, colVals.strand, NULL, rawCols,
                                  strVectorWrite, strVectorFree);
chromAnnBlkNew(ca, colVals.start, colVals.end);
return ca;
}

static void chromAnnTabReaderFree(struct chromAnnReader **carPtr)
/* free object */
{
struct chromAnnReader *car = *carPtr;
if (car != NULL)
    {
    struct chromAnnTabReader *catr = car->data;
    rowReaderFree(&catr->rr);
    freeMem(catr);
    freez(carPtr);
    }
}

struct chromAnnReader *chromAnnTabReaderNew(char *fileName, struct coordCols* cols, unsigned opts)
/* construct a reader for an arbitrary tab file. */
{
struct chromAnnTabReader *catr;
AllocVar(catr);
catr->cols = *cols;
catr->rr = rowReaderOpen(fileName, FALSE);

struct chromAnnReader *car;
AllocVar(car);
car->caRead = chromAnnTabReaderRead;
car->carFree = chromAnnTabReaderFree;
car->opts = opts;
car->data = catr;
return car;
}

int chromAnnRefLocCmp(const void *va, const void *vb)
/* Compare location of two chromAnnRef objects. */
{
const struct chromAnnRef *a = *((struct chromAnnRef **)va);
const struct chromAnnRef *b = *((struct chromAnnRef **)vb);
int diff = strcmp(a->ref->chrom, b->ref->chrom);
if (diff == 0)
    diff = a->ref->start - b->ref->start;
if (diff == 0)
    diff = a->ref->end - b->ref->end;
return diff;
}

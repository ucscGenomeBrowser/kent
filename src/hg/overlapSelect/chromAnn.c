/* chromAnn - chomosome annotations, generic object to store annotations from
 * other formats */
#include "common.h"
#include "chromAnn.h"
#include "linefile.h"
#include "binRange.h"
#include "psl.h"
#include "bed.h"
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

static struct chromAnn* chromAnnNew(char* chrom, char strand, char* name, char *recLine)
/* create new object. If recLine is not null, it's malloced and ownership
 * is passed to this object. */
{
struct chromAnn* ca;
AllocVar(ca);
ca->chrom = cloneString(chrom);
ca->strand = strand;
if (name != NULL)
    ca->name = cloneString(name);
ca->start = 0;
ca->end = 0; 
ca->recLine = recLine;
return ca;
}

static void chromAnnFinish(struct chromAnn* ca)
/* finish creation of a chromAnn after all blocks are added */
{
slReverse(&ca->blocks);
}

void chromAnnFree(struct chromAnn **caPtr)
/* free an object */
{
struct chromAnn *ca = *caPtr;
if (ca != NULL)
    {
    freeMem(ca->chrom);
    freeMem(ca->name);
    chromAnnBlkFreeList(ca->blocks);
    freeMem(ca->recLine);
    freez(caPtr);
    }
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

struct chromAnn* chromAnnFromBed(unsigned opts, struct lineFile *lf, char *line)
/* create a chromAnn object from line read from a BED file */
{
char *recLine = NULL;
char* row[256];  /* allow for extra columns */
int numCols;
struct chromAnn* ca;
struct bed *bed;

/* save copy of line before chopping; don't free this, ownership is passed */
if (opts & chromAnnSaveLines)
    recLine = cloneString(line);

numCols = chopTabs(line, row);
lineFileExpectAtLeast(lf, 3, numCols);
bed = bedLoadN(row, numCols);
ca = chromAnnNew(bed->chrom, bed->strand[0], bed->name, recLine);

if ((bed->blockCount == 0) || (opts & chromAnnRange))
    {
    if (opts & chromAnnCds)
        {
        if (bed->thickStart < bed->thickEnd)
            chromAnnBlkNew(ca, bed->thickStart, bed->thickEnd);
        }
    else
        chromAnnBlkNew(ca, bed->chromStart, bed->chromEnd);
    }
else
    addBedBlocks(ca, opts, bed);

chromAnnFinish(ca);
bedFree(&bed);
return ca;
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

struct chromAnn* chromAnnFromGenePred(unsigned opts, struct lineFile *lf, char *line)
/* create a chromAnn object from line read from a GenePred file.  If there is
 * no CDS, and chromAnnCds is specified, it will return a record with
 * zero-length range.*/
{
char *recLine = NULL;
char* row[GENEPRED_NUM_COLS];  /* allow for extra columns */
int numCols;
struct chromAnn* ca;
struct genePred *gp;

/* save copy of line before chopping; don't free this, ownership is passed */
if (opts & chromAnnSaveLines)
    recLine = cloneString(line);

numCols = chopTabs(line, row);
lineFileExpectAtLeast(lf, GENEPRED_NUM_COLS, numCols);
gp = genePredLoad(row);
ca = chromAnnNew(gp->chrom, gp->strand[0], gp->name, recLine);

if (opts & chromAnnRange)
    {
    if (opts & chromAnnCds)
        {
        if (gp->cdsStart < gp->cdsEnd)
            chromAnnBlkNew(ca, gp->cdsStart, gp->cdsEnd);
        }
    else
        chromAnnBlkNew(ca, gp->txStart, gp->txEnd);
    }
else
    addGenePredBlocks(ca, opts, gp);

chromAnnFinish(ca);
genePredFree(&gp);
return ca;
}

static void addPslBlocks(struct chromAnn* ca, unsigned opts, struct psl* psl)
/* add blocks from a psl */
{
boolean blkSizeMult = pslIsProtein(psl) ? 3 : 1;
int iBlk;
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    int start = psl->tStarts[iBlk];
    int end = start + (blkSizeMult * psl->blockSizes[iBlk]);
    if (psl->strand[1] == '-')
        reverseIntRange(&start, &end, psl->tSize);
    chromAnnBlkNew(ca, start, end);
    }
}

struct chromAnn* chromAnnFromPsl(unsigned opts, struct lineFile *lf, char *line)
/* create a chromAnn object from line read from a psl file */
{
char *recLine = NULL;
char* row[PSL_NUM_COLS];  /* allow for extra columns */
int numCols;
struct chromAnn* ca;
struct psl *psl;
char strand;

/* save copy of line before chopping; don't free this, ownership is passed */
if (opts & chromAnnSaveLines)
    recLine = cloneString(line);

numCols = chopTabs(line, row);
lineFileExpectAtLeast(lf, PSL_NUM_COLS, numCols);

psl = pslLoad(row);
if (psl->strand[1] == '\0')
    strand = psl->strand[0];
else
    strand = psl->strand[1];
ca = chromAnnNew(psl->tName, strand, psl->qName, recLine);

if (opts & chromAnnRange)
    chromAnnBlkNew(ca, psl->tStart, psl->tEnd);
else    
    addPslBlocks(ca, opts, psl);
chromAnnFinish(ca);
pslFree(&psl);
return ca;
}

struct chromAnn* chromAnnFromCoordCols(unsigned opts, struct lineFile *lf, char *line, struct coordCols* cols)
/* create a chromAnn object from a line read from tab file with coordiates at
 * a specified columns */
{
struct coordColVals colVals;
char *recLine = NULL;
char** row = needMem(cols->minNumCols*sizeof(char*));
int numCols;
struct chromAnn* ca;

/* save copy of line before chopping; don't free this, ownership is passed */
if (opts & chromAnnSaveLines)
    recLine = cloneString(line);

/* save copy of line before chopping; don't free this, ownership is passed */
if (opts & chromAnnSaveLines)
    recLine = cloneString(line);
numCols = chopByChar(line, '\t', row, cols->minNumCols);
lineFileExpectAtLeast(lf, cols->minNumCols, numCols);
colVals = coordColParseRow(cols, lf, row, numCols);

ca = chromAnnNew(colVals.chrom, colVals.strand, NULL, recLine);
chromAnnBlkNew(ca, colVals.start, colVals.end);
freez(&row);
return ca;
}

int chromAnnTotalBLockSize(struct chromAnn* ca)
/* count the total bases in the blocks of a chromAnn */
{
int bases = 0;
struct chromAnnBlk *cab;
for (cab = ca->blocks; cab != NULL; cab = cab->next)
    bases += (cab->end - cab->start);
return bases;
}

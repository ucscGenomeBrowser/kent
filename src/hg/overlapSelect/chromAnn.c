/* chromAnn - chomosome annotations, generic object to store annotations from
 * other formats */
#include "common.h"
#include "chromAnn.h"
#include "binRange.h"
#include "rowReader.h"
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

static struct chromAnn* chromAnnNew(char* chrom, char strand, char* name, char **rawCols)
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
ca->rawCols = rawCols;
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
    freeMem(ca->rawCols);
    freez(caPtr);
    }
}

void chromAnnWrite(struct chromAnn* ca, FILE *fh, char term)
/* write tab separated row using rawCols */
{
int i;
for (i = 0; ca->rawCols[i] != NULL; i++)
    {
    if (i > 0)
        fputc_unlocked('\t', fh);
    fputs_unlocked(ca->rawCols[i], fh);
    }
fputc_unlocked(term, fh);
}

static char **saveRawCols(struct rowReader *rr)
/* Save raw row columns for output.  Must be called before autoSql parser
 * is called on a row, as it will modify array rows. */
{
int vecSz = (rr->numCols+1)*sizeof(char*);
int memSz = vecSz;
int i;
for (i =-0; i < rr->numCols; i++)
    memSz += strlen(rr->row[i])+1;
char **rawCols = needMem(memSz);
char *p = ((char*)rawCols) + vecSz;
for (i =-0; i < rr->numCols; i++)
    {
    rawCols[i] = p;
    strcpy(p, rr->row[i]);
    p += strlen(rr->row[i]) + 1;
    }
return rawCols;
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

struct chromAnn* chromAnnFromBed(unsigned opts,  struct rowReader *rr)
/* create a chromAnn object from a row read from a BED file or table */
{
rowReaderExpectAtLeast(rr, 3);

char **rawCols = (opts & chromAnnSaveLines) ? saveRawCols(rr) : NULL;
struct bed *bed = bedLoadN(rr->row, rr->numCols);
struct chromAnn *ca = chromAnnNew(bed->chrom, bed->strand[0], bed->name, rawCols);

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

struct chromAnn* chromAnnFromGenePred(unsigned opts,  struct rowReader *rr)
/* create a chromAnn object from a row read from a GenePred file or table.  If
 * there is no CDS, and chromAnnCds is specified, it will return a record with
 * zero-length range.*/
{
rowReaderExpectAtLeast(rr, GENEPRED_NUM_COLS);

char **rawCols = (opts & chromAnnSaveLines) ? saveRawCols(rr) : NULL;
struct genePred *gp = genePredLoad(rr->row);
struct chromAnn* ca = chromAnnNew(gp->chrom, gp->strand[0], gp->name, rawCols);

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

struct chromAnn* chromAnnFromPsl(unsigned opts, struct rowReader *rr)
/* create a chromAnn object from a row read from a psl file or table */
{
rowReaderExpectAtLeast(rr, PSL_NUM_COLS);

char **rawCols = (opts & chromAnnSaveLines) ? saveRawCols(rr) : NULL;

struct psl *psl = pslLoad(rr->row);
char strand = (psl->strand[1] == '\0') ? psl->strand[0] :  psl->strand[1];
struct chromAnn* ca = chromAnnNew(psl->tName, strand, psl->qName, rawCols);

if (opts & chromAnnRange)
    chromAnnBlkNew(ca, psl->tStart, psl->tEnd);
else    
    addPslBlocks(ca, opts, psl);
chromAnnFinish(ca);
pslFree(&psl);
return ca;
}

struct chromAnn* chromAnnFromCoordCols(unsigned opts, struct coordCols* cols,
                                       struct rowReader *rr)
/* create a chromAnn object from a line read from a tab file or table with
 * coordiates at a specified columns */
{
rowReaderExpectAtLeast(rr, cols->minNumCols);

char **rawCols = (opts & chromAnnSaveLines) ? saveRawCols(rr) : NULL;
struct coordColVals colVals = coordColParseRow(cols, rr);

struct chromAnn *ca = chromAnnNew(colVals.chrom, colVals.strand, NULL, rawCols);
chromAnnBlkNew(ca, colVals.start, colVals.end);
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

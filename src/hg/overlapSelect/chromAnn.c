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
/* create new object */
{
struct chromAnnBlk* caBlk;
AllocVar(caBlk);
caBlk->ca = ca;;
caBlk->start = start;
caBlk->end = end;
if (start < ca->start)
    ca->start = start;
if (end > ca->end)
    ca->end = end;
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
ca->start = BIGNUM;
ca->end = 0; 
ca->recLine = recLine;
return ca;
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

if (bed->blockCount == 0)
    {
    chromAnnBlkNew(ca, bed->chromStart, bed->chromEnd);
    }
else
    {
    int iBlk;
    for (iBlk = 0; iBlk < bed->blockCount; iBlk++)
        {
        int start = bed->chromStart + bed->chromStarts[iBlk];
        int end = start + bed->blockSizes[iBlk];
        if ((opts & chromAnnCds) && (bed->thickStart < bed->thickEnd))
            {
            if (start < bed->thickStart)
                start - bed->thickStart;
            if (end > bed->thickEnd)
                end = bed->thickEnd;
            }
        if (start < end)
            chromAnnBlkNew(ca, start, end);
        }
    }
bedFree(&bed);
slReverse(&ca->blocks);
return ca;
}

struct chromAnn* chromAnnFromGenePred(unsigned opts, struct lineFile *lf, char *line)
/* create a chromAnn object from line read from a GenePred file.  If there is
 * no CDS, and chromAnnCds is specified, it will return a record with
 * zero-length range.*/
{
char *recLine = NULL;
char* row[GENEPRED_NUM_COLS];  /* allow for extra columns */
int numCols, iExon;
struct chromAnn* ca;
struct genePred *gp;

/* save copy of line before chopping; don't free this, ownership is passed */
if (opts & chromAnnSaveLines)
    recLine = cloneString(line);

numCols = chopTabs(line, row);
lineFileExpectAtLeast(lf, GENEPRED_NUM_COLS, numCols);
gp = genePredLoad(row);
ca = chromAnnNew(gp->chrom, gp->strand[0], gp->name, recLine);

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
genePredFree(&gp);
slReverse(&ca->blocks);
return ca;
}

struct chromAnn* chromAnnFromPsl(unsigned opts, struct lineFile *lf, char *line)
/* create a chromAnn object from line read from a psl file */
{
char *recLine = NULL;
char* row[PSL_NUM_COLS];  /* allow for extra columns */
int numCols, iBlk;
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
    strand = (psl->strand[0] != psl->strand[1]) ? '-' : '+';
ca = chromAnnNew(psl->tName, psl->strand[0], psl->qName, recLine);

for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    int start = psl->tStarts[iBlk];
    int end = start + psl->blockSizes[iBlk];
    if (psl->strand[1] == '-')
        reverseIntRange(&start, &end, psl->tSize);
    chromAnnBlkNew(ca, start, end);
    }
pslFree(&psl);
slReverse(&ca->blocks);
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

ca = chromAnnNew(colVals.chrom, '\0', NULL, recLine);
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

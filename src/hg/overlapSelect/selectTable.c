/* selectTable - module that contains ranges use to select */

#include "common.h"
#include "selectTable.h"
#include "linefile.h"
#include "binRange.h"
#include "psl.h"
#include "bed.h"
#include "genePred.h"
#include "coordCols.h"

#define SELTBL_DEBUG 0

struct selectRange
/* specifies a range to select */
{
    char* chrom;
    int start;
    int end;
    char strand;
    char* name;
};

/* table is never freed */
static struct hash* selectChromHash = NULL;  /* hash per-chrom binKeep of
                                              * selectRange objects */

static struct binKeeper* selectGetChromBins(char* chrom, boolean createMissing,
                                            char** key)
/* get the bins for a chromsome, optionally creating if it doesn't exist.
 * Return key if requested so string can be reused. */
{
struct hashEl* hel;

if (selectChromHash == NULL)
    selectChromHash = hashNew(10);  /* first time */
hel = hashLookup(selectChromHash, chrom);
if ((hel == NULL) && createMissing)
    {
    struct binKeeper* bins = binKeeperNew(0, 300000000);
    hel = hashAdd(selectChromHash, chrom, bins);
    }
if (hel == NULL)
    return NULL;
if ((key != NULL) && (hel != NULL))
    *key = hel->name;
return (struct binKeeper*)hel->val;
}

static void selectAddRange(char* chrom, int start, int end, char strand, char *name)
/* Add a range to the select table */
{
char *chromKey;
struct binKeeper* bins = selectGetChromBins(chrom, TRUE, &chromKey);
struct selectRange* range;
AllocVar(range);
if (SELTBL_DEBUG)
    fprintf(stderr, "selectAddRange: %s: %s %d-%d, %c\n", name, chrom, start, end, (strand == 0) ? '?' : strand);

range->chrom = chromKey;
range->start = start;
range->end = end;
range->strand = strand;
if (name != NULL)
    range->name = cloneString(name);

binKeeperAdd(bins, start, end, range);
}

static void selectAddPsl(struct psl* psl)
/* add blocks from a psl to the select table */
{
int iBlk;
char strand;
if (psl->strand[1] == '\0')
    strand = psl->strand[0];
else
    strand = (psl->strand[0] != psl->strand[1]) ? '-' : '+';
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    int start = psl->tStarts[iBlk];
    int end = start + psl->blockSizes[iBlk];
    if (psl->strand[1] == '-')
        reverseIntRange(&start, &end, psl->tSize);
    selectAddRange(psl->tName, start, end, strand, psl->qName);
    }
}

void selectAddPsls(struct lineFile *pslLf)
/* add records from a psl file to the select table */
{
struct psl* psl;

while ((psl = pslNext(pslLf)) != NULL)
    {
    selectAddPsl(psl);
    pslFree(&psl);
    }
}

static void selectAddGenePred(struct genePred* gp)
/* add blocks from a genePred to the select table */
{
int iExon;
for (iExon = 0; iExon < gp->exonCount; iExon++)
    {
    selectAddRange(gp->chrom, gp->exonStarts[iExon], gp->exonEnds[iExon], 
                   gp->strand[0], gp->name);
    }
}

void selectAddGenePreds(struct lineFile *genePredLf)
/* add blocks from a genePred file to the select table */
{
char* row[GENEPRED_NUM_COLS];
while (lineFileNextRowTab(genePredLf, row, GENEPRED_NUM_COLS))
    {
    struct genePred *gp = genePredLoad(row);
    selectAddGenePred(gp);
    genePredFree(&gp);
    }
}

static void selectAddBed(struct bed* bed)
/* add blocks from a bed to the select table */
{
if (bed->blockCount == 0)
    {
    selectAddRange(bed->chrom, bed->chromStart, bed->chromEnd, bed->strand[0],
                   bed->name);
    }
else
    {
    int iBlk;
    for (iBlk = 0; iBlk < bed->blockCount; iBlk++)
        {
        int start = bed->chromStart + bed->chromStarts[iBlk];
        selectAddRange(bed->chrom, start, start + bed->blockSizes[iBlk],
                       bed->strand[0], bed->name);
        }
    }
}

void selectAddBeds(struct lineFile* bedLf)
/* add records from a bed file to the select table */
{
char* row[12];
int numCols;
while ((numCols = lineFileChopNextTab(bedLf, row, ArraySize(row))) > 0)
    {
    struct bed *bed = bedLoadN(row, numCols);
    selectAddBed(bed);
    bedFree(&bed);
    }
}

void selectAddCoordCols(struct lineFile *tabLf, struct coordCols* cols)
/* add records with coordiates at a specified column */
{
char** row;
int numCols;
struct coordColVals colVals;

row = needMem(cols->minNumCols*sizeof(char*));

while ((numCols = lineFileChopNextTab(tabLf, row, cols->minNumCols)) > 0)
    {
    colVals = coordColParseRow(cols, tabLf, row, numCols);
    selectAddRange(colVals.chrom, colVals.start, colVals.end, colVals.strand,
                   NULL);
    }
freez(&row);
}

static boolean allowedOverlap(unsigned options, char *name, char strand, struct selectRange* range)
/* see if range attributes other than chrom,start, end pass */
{
if ((options & SEL_USE_STRAND) && (range->strand != '\0')
    && (strand != range->strand))
    return FALSE;

if ((options & SEL_EXCLUDE_SELF) && (range->name != NULL) && (name != NULL)
    && sameString(name, range->name))
    return FALSE;
return TRUE;
}

boolean selectIsOverlapped(unsigned options, char *name, char* chrom, int start, int end, char strand)
/* determine if a range is overlapped */
{
boolean isOverlapped = FALSE;
struct binKeeper* bins = selectGetChromBins(chrom, FALSE, NULL);
if (bins != NULL)
    {
    /* check each element */
    struct binElement* overlapping = binKeeperFind(bins, start, end);
    struct binElement* o;
    for (o = overlapping; (o != NULL) && !isOverlapped; o = o->next)
        {
        struct selectRange* range = o->val;
        isOverlapped = allowedOverlap(options, name, strand, range);
        }
    slFreeList(&overlapping);
    }
if (SELTBL_DEBUG)
    fprintf(stderr, "selectIsOverlapping: %s: %s %d-%d, %c => %s\n", name, chrom, start, end, 
            ((strand == '\0') ? '?' : strand), (isOverlapped ? "yes" : "no"));
return isOverlapped;
}


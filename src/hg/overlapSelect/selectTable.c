/* selectTable - module that contains ranges use to select */

#include "common.h"
#include "selectTable.h"
#include "linefile.h"
#include "binRange.h"
#include "hash.h"
#include "chromAnn.h"
#include "verbose.h"

/* tables are never freed */
static struct hash* selectChromHash = NULL;  /* hash per-chrom binKeeper of
                                              * chromAnn objects */
static struct chromAnn *selectObjs = NULL;  /* all chromAnn objects */

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

static void selectAddChromAnn(struct chromAnn *ca)
/* Add a chromAnn to the select table */
{
struct binKeeper* bins = selectGetChromBins(ca->chrom, TRUE, NULL);
if (verboseLevel() >= 2)
    {
    verbose(2, "selectAddChromAnn: %s: %s %c %d-%d\n", ca->name, ca->chrom,
            ((ca->strand == 0) ? '?' : ca->strand), ca->start, ca->end);
    if (verboseLevel() >= 3)
        {
        struct chromAnnBlk *cab;
        for (cab = ca->blocks; cab != NULL; cab = cab->next)
            verbose(3, "    blk: %d-%d\n", cab->start, cab->end);
        }
    }
binKeeperAdd(bins, ca->start, ca->end, ca);
}

static unsigned getChomAnnOpts(unsigned selOpts)
/* determine chromAnn options from selOps */
{
unsigned caOpts = 0;
if (selOpts & selSelectCds)
    caOpts |= chromAnnCds;
if (selOpts & selSaveLines)
    caOpts |= chromAnnSaveLines;
return caOpts;
}

void selectAddPsls(unsigned opts, struct lineFile *pslLf)
/* add records from a psl file to the select table */
{
char *line;

while (lineFileNextReal(pslLf, &line))
    selectAddChromAnn(chromAnnFromPsl(getChomAnnOpts(opts), pslLf, line));
}

void selectAddGenePreds(unsigned opts, struct lineFile *genePredLf)
/* add blocks from a genePred file to the select table */
{
char *line;
while (lineFileNextReal(genePredLf, &line))
    selectAddChromAnn(chromAnnFromGenePred(getChomAnnOpts(opts), genePredLf, line));
}

void selectAddBeds(unsigned opts, struct lineFile* bedLf)
/* add records from a bed file to the select table */
{
char *line;

while (lineFileNextReal(bedLf, &line))
    selectAddChromAnn(chromAnnFromBed(getChomAnnOpts(opts), bedLf, line));
}

void selectAddCoordCols(unsigned opts, struct lineFile *tabLf, struct coordCols* cols)
/* add records with coordiates at a specified column */
{
char *line;

while (lineFileNextReal(tabLf, &line))
    selectAddChromAnn(chromAnnFromCoordCols(getChomAnnOpts(opts), tabLf, line, cols));
}

static boolean isSelfMatch(unsigned opts, struct chromAnn *inCa, struct chromAnn* selCa)
/* check if this is a self record */
{
struct chromAnnBlk *inCaBlk, *selCaBlk;

/* already know we are on same chrom and strand (if check strand) */
if ((inCa->start != selCa->start) || (inCa->end != selCa->end))
    return FALSE;
if (((inCa->name != NULL) && (selCa->name != NULL))
    && !sameString(inCa->name, selCa->name))
    return FALSE;

/* check for identical block structures */
for (inCaBlk = inCa->blocks, selCaBlk = selCa->blocks;
     ((inCaBlk != NULL) && (selCaBlk != NULL));
     inCaBlk = inCaBlk->next, selCaBlk = selCaBlk->next)
    {
    if ((inCaBlk->start != selCaBlk->start) || (inCaBlk->end != selCaBlk->end))
        return FALSE;
    }
if ((inCaBlk != NULL) || (selCaBlk != NULL))
    return FALSE;  /* different lengths */

return TRUE;
}

static boolean isOverlapped(unsigned opts, struct chromAnn *inCa, struct chromAnn* selCa)
/* see an chromAnn objects overlap */
{
struct chromAnnBlk *inCaBlk, *selCaBlk;
if ((opts & selUseStrand) && (inCa->strand != selCa->strand))
    return FALSE;
if ((opts & selExcludeSelf) && isSelfMatch(opts, inCa, selCa))
    return FALSE;
for (inCaBlk = inCa->blocks; inCaBlk != NULL; inCaBlk = inCaBlk->next)
    for (selCaBlk = selCa->blocks; selCaBlk != NULL; selCaBlk = selCaBlk->next)
        {
            if ((inCaBlk->start < selCaBlk->end) && (inCaBlk->end > selCaBlk->start))
                return TRUE;
        }
return FALSE;
}

boolean selectIsOverlapped(unsigned opts, struct chromAnn *inCa,
                           struct slRef **overlappedRecLines)
/* Determine if a range is overlapped.  If overlappedRecLines is not null,
 * a list of the line form of overlaping select records is returned.  Free
 * with slFreelList. */
{
boolean hit = FALSE;
struct binKeeper* bins = selectGetChromBins(inCa->chrom, FALSE, NULL);
if (bins != NULL)
    {
    struct binElement* overlapping = binKeeperFind(bins, inCa->start, inCa->end);
    struct binElement* o;

    /* check each overlapping chomAnn */
    for (o = overlapping; o != NULL; o = o->next)
        {
        struct chromAnn* selCa = o->val;
        hit = isOverlapped(opts, inCa, selCa);
        if (hit)
            {
            if (overlappedRecLines != NULL)
                {
                if (!refOnList(*overlappedRecLines, selCa))
                    refAdd(overlappedRecLines, selCa);
                }
            else
                break;  /* only need one overlap */
            }
        }
    slFreeList(&overlapping);
    }
if (verboseLevel() >= 2)
    verboseLevel(2, "selectIsOverlapping: %s: %s %d-%d, %c => %s\n", inCa->name, inCa->chrom, inCa->start, inCa->end,
            ((inCa->strand == '\0') ? '?' : inCa->strand), (hit ? "yes" : "no"));
return hit;
}


/* selectTable - module that contains ranges use to select */
#ifndef SELECT_TABLE_H
#define SELECT_TABLE_H

enum selectOpts
/* selection table options */
{
    selExcludeSelf  = 0x01,    /* skipping matching records */
    selUseStrand    = 0x02,    /* select by strand */
    selSelectCds    = 0x04,    /* only use CDS range for select table */
    selSelectRange  = 0x08,    /* use entire range, not just blocks */
    selSaveLines    = 0x10,    /* save lines for merge */
    selIdMatch      = 0x20     /* ids must match and overlap  */
};

struct overlapStats
/* overlap stats */
{
    float inOverlap;          /* fraction of in object overlapped */
    unsigned inOverBases;     /* number of bases overlaped */
    unsigned inBases;         /* number of possible bases to overlap */
};


struct coordCols;
struct lineFile;
struct chromAnn;

void selectAddPsls(unsigned opts, struct lineFile *pslLf);
/* add records from a psl file to the select table */

void selectAddGenePreds(unsigned opts, struct lineFile *genePredLf);
/* add blocks from a genePred file to the select table */

void selectAddBeds(unsigned opts, struct lineFile* bedLf);
/* add records from a bed file to the select table */

void selectAddCoordCols(unsigned opts, struct lineFile *tabLf, struct coordCols* cols);
/* add records with coordiates at a specified columns */

float selectFracOverlap(struct chromAnn *inCa, struct chromAnn *selCa);
/* get the fraction of inCa overlapped by selCa */

boolean selectIsOverlapped(unsigned opts, struct chromAnn *inCa,
                           float overlapThreshold, float overlapSimilarity,
                           struct slRef **overlappingRecs);
/* Determine if a range is overlapped.  If overlappingRecs is not null, a list
 * of the of selected records is returned.  Free with slFreelList. */

struct overlapStats selectAggregateOverlap(unsigned opts, struct chromAnn *inCa);
/* Compute the aggregate overlap of a chromAnn */

#endif

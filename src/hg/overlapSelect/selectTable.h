/* selectTable - module that contains ranges use to select */
#ifndef SELECT_TABLE_H
#define SELECT_TABLE_H

enum selectOpts
/* selection table options */
{
    selExcludeSelf  = 0x01,    /* skipping matching records */
    selUseStrand    = 0x02,    /* select by strand */
    selSelectCds    = 0x04,    /* only use CDS range for select table */
    selSaveLines    = 0x08     /* save lines for merge */
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

boolean selectIsOverlapped(unsigned opts, struct chromAnn *inCa,
                           float overlapThreshold,
                           struct slRef **overlappedRecLines);
/* determine if an range is overlapped.  If overlappedRecLines is not null,
 * a list of the line form of overlapping chromAnn objects is returned.  Free
 * with slFreelList. */

#endif

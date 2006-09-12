/* selectTable - module that contains ranges use to select */
#ifndef SELECT_TABLE_H
#define SELECT_TABLE_H

struct rowReader;

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

struct overlapCriteria
/* criteria for selecting */
{
    float threshold;        // threshold fraction
    float thresholdCeil;    // threshold must be less than this value
    float similarity;       // bidirectional threshold
    float similarityCeil;   // similarity must be less than this value
    int bases;              // number of bases
};

struct overlapAggStats
/* aggregate overlap stats */
{
    float inOverlap;          // fraction of in object overlapped
    unsigned inOverBases;     // number of bases overlaped
    unsigned inBases;         // number of possible bases to overlap
};

struct coordCols;
struct lineFile;
struct chromAnn;

void selectAddPsls(unsigned opts, struct rowReader *rr);
/* add psl records to the select table */

void selectAddGenePreds(unsigned opts,  struct rowReader *rr);
/* add genePred records to the select table */

void selectAddBeds(unsigned opts,  struct rowReader *rr);
/* add bed records to the select table */

void selectAddCoordCols(unsigned opts, struct coordCols* cols, struct rowReader *rr);
/* add records with coordiates at a specified column */

int selectOverlapBases(struct chromAnn *ca1, struct chromAnn *ca2);
/* determine the number of bases of overlaping in two annotations */

float selectFracOverlap(struct chromAnn *ca, int overBases);
/* get the fraction of ca overlapped give number of overlapped bases */

boolean selectIsOverlapped(unsigned opts, struct chromAnn *inCa,
                           struct overlapCriteria *criteria,
                           struct slRef **overlappingRecs);
/* Determine if a range is overlapped.  If overlappingRecs is not null, a list
 * of the of selected records is returned.  Free with slFreelList. */

struct overlapAggStats selectAggregateOverlap(unsigned opts, struct chromAnn *inCa);
/* Compute the aggregate overlap of a chromAnn */

#endif

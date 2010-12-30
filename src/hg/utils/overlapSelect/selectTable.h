/* selectTable - module that contains ranges use to select.  This module
 * functions as a global object. */
#ifndef SELECT_TABLE_H
#define SELECT_TABLE_H
#include "binRange.h"
#include "hash.h"
struct chromAnnReader;
struct chromAnnRef;
#include "chromAnnMap.h"

enum selectOpts
/* selection table options */
{
    selExcludeSelf    = 0x01,    /* skipping matching records */
    selStrand         = 0x02,    /* select by strand */
    selIdMatch        = 0x04,    /* ids must match and overlap  */
    selOppositeStrand = 0x08     /* select by opposite strand */
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

void selectTableAddRecords(struct chromAnnReader *car);
/* add records to the select table */

int selectOverlapBases(struct chromAnn *ca1, struct chromAnn *ca2);
/* determine the number of bases of overlaping in two annotations */

float selectFracOverlap(struct chromAnn *ca, int overBases);
/* get the fraction of ca overlapped give number of overlapped bases */

float selectFracSimilarity(struct chromAnn *ca1, struct chromAnn *ca2,
                           int overBases);
/* get the fractions similarity betten two annotations, give number of
 * overlapped bases */

boolean selectIsOverlapped(unsigned opts, struct chromAnn *inCa,
                           struct overlapCriteria *criteria,
                           struct chromAnnRef **overlappingRecs);
/* Determine if a range is overlapped.  If overlappingRecs is not null, a list
 * of the of selected records is returned.  Free with slFreelList. */

struct overlapAggStats selectAggregateOverlap(unsigned opts, struct chromAnn *inCa);
/* Compute the aggregate overlap of a chromAnn */

struct chromAnnMapIter selectTableFirst();
/* iterator over select table */

void selectTableFree();
/* free selectTable structures. */

#endif

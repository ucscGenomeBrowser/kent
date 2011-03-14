#ifndef GBALIGNCOMMON_H
#define GBALIGNCOMMON_H
#include "common.h"
#include "gbDefs.h"

struct gbSelect;
struct gbProcessed;
struct gbAligned;
struct gbEntry;

/* Flags used in entry clientFlags */
#define MIGRATE_FLAG     0x01  /* entry should be migrated */
#define ALIGN_FLAG       0x02  /* entry should be aligned */
#define ACC_CNT_FLAG     0x04  /* flag indicated that the accession
                                * has been counted at least once */

struct gbEntryCnts 
/* Counts of accessions and record used in bookkeeping.  Both total and by
 * orgCat */
{
    unsigned accCnt[GB_NUM_ORG_CATS];     /* count of accessions, by orgCat */
    unsigned recCnt[GB_NUM_ORG_CATS];     /* count of records (psl, etc), by orgCat */
    unsigned accTotalCnt;                 /* total count of accessions */
    unsigned recTotalCnt;                 /* total count of record (psl, etc) */
};

struct gbAlignInfo
/* Alignment count information */
{
    struct gbEntryCnts migrate;  /* entries to migrate or migrated */
    struct gbEntryCnts align;    /* entries to align or aligned */
};

INLINE boolean gbEntryCntsHaveAny(struct gbEntryCnts *entryCnts, unsigned orgCat)
{
return (entryCnts->accCnt[orgCat] + entryCnts->recCnt[orgCat]) > 0;
}

void gbCountNeedAligned(struct gbEntryCnts* cnts, struct gbEntry* entry,
                        unsigned accIncr, unsigned recIncr);
/* Increment counts or entries to process */

void gbEntryCntsSum(struct gbEntryCnts* accum, struct gbEntryCnts* cnts);
/* sum alignment counts */
 
void gbAlignInfoSum(struct gbAlignInfo* accum, struct gbAlignInfo* cnts);
/* add fields in cnts to accum */

struct gbSelect* gbAlignGetMigrateRel(struct gbSelect* select);
/* Determine if alignments should be migrated from a previous release. */

struct gbAlignInfo gbAlignFindNeedAligned(struct gbSelect* select,
                                          struct gbSelect* prevSelect);
/* Find entries that need to be aligned or migrated for an update.
 * If prevSelect is not null, and select indicates the full update,
 * alignments will be flagged for migration if possible.
 *
 * If an entry is selected for migration:
 *   prevEntry.clientFlags |= MIGRATE_FLAG
 *   entry.clientFlags |= MIGRATE_FLAG
 *   update.selectAligns |= GB_NATIVE or GB_XENO
 *
 * If an entry is selected for alignment:
 *   entry.clientFlags |= MIGRATE_FLAG
 *   update.selectProc |= GB_NATIVE or GB_XENO
 * Returns counts of entries to align or migrate. */


#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

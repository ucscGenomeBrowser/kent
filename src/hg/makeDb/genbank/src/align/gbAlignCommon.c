#include "gbAlignCommon.h"
#include "gbUpdate.h"
#include "gbRelease.h"
#include "gbProcessed.h"
#include "gbAligned.h"
#include "gbEntry.h"
#include "gbGenome.h"
#include "gbVerb.h"

static char const rcsid[] = "$Id: gbAlignCommon.c,v 1.5 2006/03/20 23:00:25 markd Exp $";

void gbCountNeedAligned(struct gbEntryCnts* cnts, struct gbEntry* entry,
                        unsigned accIncr, unsigned recIncr)
/* Increment counts or entries to process */
{
unsigned orgCatIdx = gbOrgCatIdx(entry->orgCat);
cnts->accCnt[orgCatIdx] += accIncr;
cnts->recCnt[orgCatIdx] += recIncr;
cnts->accTotalCnt += accIncr;
cnts->recTotalCnt += recIncr;
}

void gbEntryCntsSum(struct gbEntryCnts* accum, struct gbEntryCnts* cnts)
/* sum alignment counts */
{
accum->accCnt[0] += cnts->accCnt[0];
accum->recCnt[0] += cnts->recCnt[0];
accum->accCnt[1] += cnts->accCnt[1];
accum->recCnt[1] += cnts->recCnt[1];
accum->accTotalCnt += cnts->accTotalCnt;
accum->recTotalCnt += cnts->recTotalCnt;
}

void gbAlignInfoSum(struct gbAlignInfo* accum, struct gbAlignInfo* cnts)
/* add fields in cnts to accum */
{
gbEntryCntsSum(&accum->migrate, &cnts->migrate);
gbEntryCntsSum(&accum->align, &cnts->align);
}

struct gbSelect* gbAlignGetMigrateRel(struct gbSelect* select)
/* Determine if alignments should be migrated from a previous release. */
{
struct gbSelect* prevSelect = NULL;
if (select->update->isFull)
    {
    struct gbRelease* prevRel = select->release->next;
    while (prevRel != NULL)
        {
        if (gbReleaseHasData(prevRel, GB_ALIGNED, select->type,
                             select->accPrefix))
            break;
        prevRel = prevRel->next;
        }
    if (prevRel != NULL)
        {
        AllocVar(prevSelect);
        prevSelect->release = prevRel;
        prevSelect->type = select->type;
        prevSelect->orgCats = select->orgCats;
        prevSelect->accPrefix = select->accPrefix;
        }
    }

return prevSelect;
}

static boolean isNewOrMod(struct gbEntry* entry, struct gbUpdate* update)
/* Determine if a sequence was new or modified (version changed) in
 * this update.  Always returned true for full. */
{
struct gbProcessed* processed = entry->processed;
struct gbProcessed* prevProc;

/* Find the first processed entry for this update. */
while ((processed != NULL) && (processed->update != update))
    {
    processed = processed->next;
    }
/* we should not be here if it's not in the update */
if (processed == NULL)
    errAbort("processed update %s not associated with %s",
             update->name, entry->acc);

/* Scan to find the next oldest entry in another update.  It's possible that
 * there will be other entries in this update with earlier versions, so skip
 * them. */
prevProc = processed->next;
while ((prevProc != NULL) && (prevProc->update == update))
    prevProc = prevProc->next;

if (prevProc != NULL)
    {
    if (prevProc->version > processed->version)
        errAbort("versions go backward in update %s for %s",
                 update->name, entry->acc);
    return (processed->version > prevProc->version);
    }
else
    return TRUE; /* new */
}

static boolean needAlignedSelect(struct gbSelect* select,
                                 struct gbEntry* entry)
/* Determing if an entry matches selection criteria and has not
 * already been flagged */
{
if (entry->clientFlags & (MIGRATE_FLAG|ALIGN_FLAG))
    return FALSE;  /* already migrated */
if (!(entry->type & select->type))
    return FALSE;  /* wrong type */
if (!(entry->orgCat & select->orgCats))
    return FALSE;  /* wrong organism category */

/* FIXME: special handling to only do human xeno refseqs for panTro1.
 * panTro2 uses a different hack.*/
if ((select->release->srcDb == GB_REFSEQ) && (entry->orgCat == GB_XENO)
    && sameString("panTro1", select->release->genome->database)
    && !sameString(entry->processed->organism, "Homo sapiens"))
    {
    return FALSE;
    }

return isNewOrMod(entry, select->update);
}

struct gbAligned* findPrevAligned(struct gbSelect* prevSelect,
                                  struct gbProcessed* processed)
/* Check to see if a accession is in the prevAligned.  If the organism
 * category of the alignment doesn't match the entry, it will be ignored.
 * This can happen if organism aliases are added or the organism name changed.
 */
{
struct gbAligned* prevAligned = NULL;  /* default if no previous */
struct gbEntry* prevEntry = gbReleaseFindEntry(prevSelect->release,
                                               processed->entry->acc);
if (prevEntry != NULL)
    prevAligned = gbEntryFindAlignedVer(prevEntry, processed->version);
if ((prevAligned != NULL)
    && (prevAligned->alignOrgCat != processed->entry->orgCat))
    return NULL;  /* ignore due to category change */
else
    return prevAligned;
}

static boolean canMigrate(struct gbProcessed* processed,
                          struct gbAligned* prevAligned)
/* check if an entry can be migrated */
{
/* Don't migrate if type or orgCat changed.  This is a bit tricky, since it
 * can change without the version changing and this could happen in any
 * update. So we just don't migrate if any of the previous processed entries
 * with this version don't match the current type of orgCat.  The explict type
 * check isn't really needed, as only one type is processed at a time.  Also,
 * we don't migrate if there isn't a processed entry in the same updated as
 * the aligned entry.  This generally indicates that the entry is in the
 * ignored file.  This isn't necessary in all cases, but the bookkeeping was
 * too ugly to figure it out, so we just realign */
struct gbProcessed* prevProcessed;
boolean haveUpdateProcessed = FALSE;
for (prevProcessed = prevAligned->entry->processed; prevProcessed != NULL;
     prevProcessed = prevProcessed->next)
    {
    if (prevProcessed->version == processed->version)
        {
        if ((prevProcessed->entry->type != processed->entry->type)
            || (prevProcessed->orgCat != processed->entry->orgCat))
            return FALSE;  /* can't migrate */
        if (prevProcessed->update == prevAligned->update)
            haveUpdateProcessed = TRUE;
        }
    }
return haveUpdateProcessed;
}

static void flagNeedAligned(struct gbSelect* select,
                            struct gbSelect* prevSelect,
                            struct gbProcessed* processed,
                            struct gbAlignInfo* alignInfo)
/* Function called for each sequence to set alignment and migrate flags.  The
 * migrate flag is set in the previous and curent entries, the align flag set
 * in only in current ones. */
{
struct gbAligned* prevAligned = NULL;
if (prevSelect != NULL)
    prevAligned = findPrevAligned(prevSelect, processed);

/* Migrate if same acc is aligned in the previous release and passed other
 * checks, otherwise mark the entry for alignment. */
if ((prevAligned != NULL) && canMigrate(processed, prevAligned))
    {
    struct gbEntry* prevEntry = prevAligned->entry;
    prevEntry->selectVer = prevAligned->version;
    prevEntry->clientFlags |= MIGRATE_FLAG;
    processed->entry->clientFlags |= MIGRATE_FLAG;
    prevAligned->update->selectAlign |= prevEntry->orgCat;
    gbCountNeedAligned(&alignInfo->migrate, prevEntry, 1, prevAligned->numAligns);
    if (gbVerbose >= 3)
        gbVerbPr(3, "migrate %s %s.%d %d psls", 
                 gbOrgCatName(prevEntry->orgCat), prevEntry->acc,
                 prevAligned->version, prevAligned->numAligns);
    }
else
    {
    struct gbEntry* entry = processed->entry;
    entry->selectVer = processed->version;
    entry->clientFlags |= ALIGN_FLAG;
    processed->update->selectProc |= entry->orgCat;
    gbCountNeedAligned(&alignInfo->align, entry, 1, 0);
    if (gbVerbose >= 3)
        gbVerbPr(3, "align %s %s.%d", gbOrgCatName(entry->orgCat),
                 entry->acc, processed->version);
    }
}

struct gbAlignInfo gbAlignFindNeedAligned(struct gbSelect* select,
                                          struct gbSelect* prevSelect)
/* Find entries that need to be aligned or migrated for an update.
 * If prevSelect is not null, and select indicates the full update,
 * alignments will be flagged for migration if possible.  Only the
 * update in select is processed, however alignments for any
 * of the prevSelect updates can be flagged for migration.
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
{
struct gbAlignInfo alignInfo;
struct gbProcessed* processed;
ZeroVar(&alignInfo);
        
/* visit all processed entries for this update  */
for (processed = select->update->processed; processed != NULL;
     processed = processed->updateLink)
    {
    /* this will always select entries if this is the full update */
    if (needAlignedSelect(select, processed->entry))
        flagNeedAligned(select, prevSelect, processed, &alignInfo);
    }
return alignInfo;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


#include "gbAlignCommon.h"
#include "gbUpdate.h"
#include "gbRelease.h"
#include "gbProcessed.h"
#include "gbAligned.h"
#include "gbEntry.h"
#include "gbGenome.h"

static char const rcsid[] = "$Id: gbAlignCommon.c,v 1.1 2003/06/03 01:27:41 markd Exp $";

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
 * them.
 */
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
/* Determing if an entry matches selection criteria */
{
if (!(entry->type & select->type))
    return FALSE;  /* wrong type */
if (!(entry->orgCat & select->orgCats))
    return FALSE;  /* wrong organism category */
return isNewOrMod(entry, select->update);
}

struct gbAligned* findPrevAligned(struct gbSelect* prevSelect,
                                  struct gbProcessed* processed)
/* Check to see if a accession is in the prevAligned.  If the organism *
 * category of the alignment doesn't match the entry, it will be ignored.
 * This can happen if organism aliases are added or the organism name changed.
 */
{
struct gbAligned* prevAligned = NULL;  /* default if no previous */
struct gbEntry* prevEntry = gbReleaseFindEntry(prevSelect->release,
                                               processed->entry->acc);
if (prevEntry != NULL)
    prevAligned = gbEntryFindAlignedVer(prevEntry,
                                        processed->version);
if ((prevAligned != NULL)
    && (prevAligned->alignOrgCat != processed->entry->orgCat))
    return NULL;  /* ignore due to category change */
else
    return prevAligned;
}

void gbAlignFindNeedAligned(struct gbSelect* select,
                            struct gbSelect* prevSelect,
                            gbNeedAlignedCallback* callback, void* clientData)
/* Find sequences that need to be aligned for this update and genome.
 * call the specified funciton on each sequence.  If prevSelect is not
 * NULL, it is check to determine if the sequence is aligned in that
 * release.  If select->orgCat is set to GB_NATIVE or GB_XENO, only matching
 * organisms are returned. */
{
struct gbAligned* prevAligned = NULL;  /* default if no previous */
        
/* visit all processed sequences for this uppdates  */
struct gbProcessed* processed = select->update->processed;
while (processed != NULL)
    {
    struct gbEntry* entry = processed->entry;
    if (needAlignedSelect(select, entry))
        {
        if (prevSelect != NULL)
            prevAligned = findPrevAligned(prevSelect, processed);
        (*callback)(select, processed, prevAligned, clientData);
        }
    processed = processed->updateLink;  /* next in update */
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


/* check gbIndex entries */
#include "chkGbIndex.h"
#include "gbIndex.h"
#include "chkCommon.h"
#include "metaData.h"
#include "chkMetaDataTbls.h"
#include "gbRelease.h"
#include "gbEntry.h"
#include "gbAligned.h"
#include "gbUpdate.h"
#include "gbGenome.h"
#include "gbProcessed.h"
#include "gbFileOps.h"
#include "gbVerb.h"


static void chkProcessed(struct gbEntry* entry, struct gbProcessed* processed)
/* check a single processed object */
{
struct gbProcessed* nextProc = processed->next;
if (nextProc != NULL)
    {
    /* check moddates/versions are in descending or same moddates for dups */
    if (processed->modDate < nextProc->modDate)
        gbError("%s processed moddates are in wrong order", entry->acc);
    if (processed->version < nextProc->version)
        gbError("%s processed versions are in wrong order", entry->acc);

    /* scan ahead to check that there are none with same moddate and different
     * versions. */
    for (; nextProc != NULL; nextProc = nextProc->next)
        {
        if ((nextProc->modDate == processed->modDate)
            && (nextProc->version != processed->version))
            gbError("%s processed %s has same modDate as %s, with different version",
                    entry->acc, processed->update->name, nextProc->update->name);
        }
    }
}

static void chkEntryProcessed(struct gbEntry* entry)
/* check the processed objects for an entry, genome is not null if checking
 * aligned */
{
struct gbProcessed* processed = entry->processed;
if (gbVerbose >= 3)
    gbVerbMsg(3, "chkEntryProcessed %s", entry->acc);
    
/* must be a processed entry, unless ignored */
if (processed == NULL)
    gbError("no processed records for %s", entry->acc);
else
    {
    for (; processed != NULL; processed = processed->next)
        chkProcessed(entry, processed);
    }
}

static void chkAligned(struct gbEntry* entry, struct gbAligned* aligned)
/* check a single aligned object */
{
struct gbAligned* nextAln = aligned->next;
struct gbProcessed* processed;
if (nextAln != NULL)
    {
    /* check versions are in descending order (allow for dups) */
    if (aligned->version < nextAln->version)
        gbError("%s aligned versions are in wrong order", entry->acc);
    }

/* there must be a processed entry for this aligned entry in this update,
 * however there might be more that one due to multiple changes or dups */
processed = entry->processed;
while ((processed != NULL) && (processed->update != aligned->update))
    processed = processed->next;
if (processed == NULL)
    gbError("%s aligned entry %s does not have a processed object", entry->acc,
            aligned->update->name);
else
    {
    /* scan for version, due to possible multiple entries. */
    while ((processed != NULL) && (processed->update == aligned->update)
           && (processed->version != aligned->version))
        processed = processed->next;
    if ((processed == NULL) || (processed->update != aligned->update)
        || (processed->version != aligned->version))
        gbError("%s aligned entry %s does not have a processed object with version %d",
                entry->acc, aligned->update->name, aligned->version);
    }
}

static void chkEntryAligned(struct gbEntry* entry)
/* check the aligned objects for an entry*/
{
struct gbAligned* aligned = entry->aligned;
gbVerbMsg(3, "chkEntryAligned %s", entry->acc);
for (; aligned != NULL; aligned = aligned->next)
    chkAligned(entry, aligned);
}

static void chkEntry(struct gbEntry* entry)
/* Check an index entry, including adding it to the metadata table */
{
chkEntryProcessed(entry);
chkEntryAligned(entry);
}

static boolean shouldCheck(struct gbEntry* entry)
/* should an entry be checked */
{
// check entries with no processed, or genbanks of type mRNA or any refseq
return ((entry->processed == NULL) || (entry->processed->molType == mol_mRNA)
        || (entry->processed->update->release->srcDb == GB_REFSEQ));
}

void chkGbIndex(struct gbSelect* select, struct metaDataTbls* metaDataTbls)
/* Check a single partationing of genbank or refseq gbIndex files,
 * checking internal consistency and add to metadata */
{
struct hashCookie cookie;
struct hashEl* hel;

gbVerbEnter(1, "check gbIndex: %s", gbSelectDesc(select));

cookie = hashFirst(select->release->entryTbl);
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (shouldCheck(hel->val))
        {
        chkEntry(hel->val);
        chkMetaDataGbEntry(metaDataTbls, select, hel->val);
        }
    }
gbVerbLeave(1, "check gbIndex: %s", gbSelectDesc(select));
}


/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


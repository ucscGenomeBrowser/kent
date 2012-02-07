#include "gbEntry.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbGenome.h"
#include "gbProcessed.h"
#include "gbAligned.h"
#include "gbFileOps.h"
#include "localmem.h"
#include "hash.h"


struct gbEntry* gbEntryNew(struct gbRelease* release, char* acc, unsigned type)
/* allocate a new gbEntry object and add it to the release*/
{
struct gbEntry* entry;
struct hashEl* hashEl;
    
gbReleaseAllocEntryVar(release, entry);
    
/* add to hash table, which copy key into it's own local memory,
 * then save that key */
hashEl = hashAdd(release->entryTbl, acc, entry);
entry->acc = hashEl->name;
/* don't let others flags slip in */
entry->type = type & GB_TYPE_MASK;
entry->selectVer = NULL_VERSION;
if (type == GB_MRNA)
    release->numMRnas++;
else if (type == GB_EST)
    release->numEsts++;
else
    errAbort("invalid entry type");

return entry;
}

struct gbProcessed* gbEntryFindProcessed(struct gbEntry* entry,
                                         int version)
/* Find the newest processed object for a specific version, or NULL not
 * found */
{
struct gbProcessed* processed = entry->processed;
while ((processed != NULL) && (processed->version != version))
    processed = processed->next;
return processed;
}

struct gbProcessed* gbEntryFindUpdateProcessed(struct gbEntry* entry,
                                               struct gbUpdate* update)
/* Find the processed object for a specific update, or NULL if not in this
 * update. */
{
struct gbProcessed* processed = entry->processed;
while ((processed != NULL) && (processed->update != update))
    processed = processed->next;
return processed;
}

static int cmpProcessedVers(int version, time_t modDate,
                            struct gbUpdate* update,
                            struct gbProcessed* processed2)
/* compare a version and modDate with a processed entry */
{
int cmp = (version - processed2->version);
if (cmp == 0)
    cmp = (modDate - processed2->modDate);
if (cmp == 0)
    {
    /* Same entry. If one of the updates is the full it is returned.  This
     * handles the case of a daily that is older than the full, which happens
     * with RefSeq. */
    if (update == processed2->update)
        cmp = 0;
    else if (update->isFull)
        cmp = 1;
    else if (processed2->update->isFull)
        cmp = -1;
    else
        cmp = gbUpdateCmp(update, processed2->update);
    }
return cmp;
}

struct gbProcessed* gbEntryAddProcessed(struct gbEntry* entry,
                                        struct gbUpdate* update,
                                        int version, time_t modDate,
                                        char* organism, enum molType molType)
/* Create a new processed object in the entry and link with it's update */
{
struct gbProcessed* cur = entry->processed;
struct gbProcessed* prev = NULL;
int cmp = 1;

/* Search the place to insert */
while ((cur != NULL)
       && ((cmp = cmpProcessedVers(version, modDate, update, cur)) < 0))
    {
    prev = cur;
    cur = cur->next;
    }
if (cmp == 0)
    {
    /* ignore duplicates (it happens in dailies) */
    return cur;
    }
else
    {
    struct gbProcessed* processed = gbProcessedNew(entry, update,
                                                   version, modDate,
                                                   organism, molType);
    if (prev == NULL)
        {
        /* add to head */
        processed->next = entry->processed;
        entry->processed = processed;
        }
    else
        {
        /* add after previous */
        processed->next = prev->next;
        prev->next = processed;
        }
    /* link in with it's update */
    processed->updateLink = processed->update->processed;
    processed->update->processed = processed;

    /* set organism category from organism if we have a genome */
    if (update->release->genome != NULL)
        {
        entry->orgCat = gbGenomeOrgCat(update->release->genome, organism);
        }
    return processed;
    }
}

struct gbAligned* gbEntryFindAligned(struct gbEntry* entry,
                                     struct gbUpdate* update)
/* Find an aligned entry for a specific update, or NULL not found */
{
struct gbAligned* aligned = entry->aligned;
while ((aligned != NULL) && (aligned->update != update))
    aligned = aligned->next;
return aligned;
}

struct gbAligned* gbEntryFindAlignedVer(struct gbEntry* entry, int version)
/* Find an aligned entry for a specific version, or NULL not found */
{
struct gbAligned* aligned = entry->aligned;
while ((aligned != NULL) && (aligned->version != version))
    aligned = aligned->next;
return aligned;
}

struct gbAligned* gbEntryGetAligned(struct gbEntry* entry,
                                    struct gbUpdate* update, int version,
                                    boolean *created)
/* Get an aligned entry for a specific version, creating a new one if not
 * found */
{
struct gbAligned* aligned = entry->aligned;
struct gbAligned* prev = NULL;
if (created != NULL)
    *created = FALSE;
while ((aligned != NULL) && (version < aligned->version))
    {
    prev = aligned;
    aligned = aligned->next;
    }
if ((aligned == NULL) || (aligned->update != update)
    || (aligned->version != version))
    {
    /* create new and add to list */
    aligned = gbAlignedNew(entry, update, version);
    if (prev == NULL)
        {
        /* add to head */
        aligned->next = entry->aligned;
        entry->aligned = aligned;
        }
    else
        {
        /* add after previous */
        aligned->next = prev->next;
        prev->next = aligned;
        }
    if (created != NULL)
        *created = TRUE;
    }
return aligned;
}

boolean gbEntryMatch(struct gbEntry* entry, unsigned flags)
/* Determine if an entry matches the entry type flag (GB_MRNA or GB_EST) in
 * flags, and if organism category is in flags (GB_NATIVE or GB_XENO),
 * check this as well. */
{
if ((flags & GB_TYPE_MASK) != entry->type)
    return FALSE;
if ((flags & GB_ORG_CAT_MASK) != 0)
    return ((flags & GB_ORG_CAT_MASK) == entry->orgCat);
else
    return TRUE;
}

void gbEntryDump(struct gbEntry* entry, FILE* out, int indent)
/* print a gbEntry object */
{
struct gbProcessed* prNext;
struct gbAligned* alNext;

fprintf(out, "%*s%s: %s\n", indent, "", entry->acc,
        ((entry->type == GB_MRNA) ? "mRNA" : "EST"));
for (prNext = entry->processed; prNext != NULL; prNext = prNext->next)
    fprintf(out, "%*spr: %s: %d %s \"%s\"\n", indent+2, "",
            prNext->update->name, prNext->version,
            gbFormatDate(prNext->modDate), prNext->organism);

for (alNext = entry->aligned; alNext != NULL; alNext = alNext->next)
    fprintf(out, "%*sal: %s: %d\n", indent+2, "",
            alNext->update->name, alNext->version);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


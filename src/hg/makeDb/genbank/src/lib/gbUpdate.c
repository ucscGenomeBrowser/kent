#include "gbUpdate.h"
#include "gbRelease.h"
#include "gbProcessed.h"
#include "gbAligned.h"
#include "gbGenome.h"
#include "gbEntry.h"
#include "localmem.h"

static char const rcsid[] = "$Id: gbUpdate.c,v 1.3 2007/06/16 19:01:50 markd Exp $";

void gbUpdateNameFromFile(char* updateName, char* fileName)
/* Get an update name from a file name in the dir */
{
char* lastSep = strrchr(fileName, '/');
char* prevSep;
updateName[0] = '\0';

/* find the directory */
if (lastSep != NULL)
    {
    *lastSep = '\0';
    prevSep = strrchr(fileName, '/');
    if (prevSep != NULL)
        strcpy(updateName, prevSep+1);
    *lastSep = '/';
    }
if (updateName[0] == '\0')
    errAbort("can't parse directory from \"%s\"", fileName);
}


struct gbUpdate* gbUpdateNew(struct gbRelease* release, char* updateName)
/* create a new update object */
{
struct gbUpdate* update;
AllocVar(update);

update->isFull = sameString(updateName, GB_FULL_UPDATE);
if (update->isFull)
    {
    update->name = cloneString(GB_FULL_UPDATE);
    update->shortName = GB_FULL_UPDATE;
    }
else
    {
    if (!startsWith(GB_DAILY_UPDATE_PREFIX, updateName))
        errAbort("invalid update name: \"%s\"", updateName);
    update->name = cloneString(updateName);
    update->shortName = update->name + strlen(GB_DAILY_UPDATE_PREFIX);
    }
update->release = release;

return update;
}

void gbUpdateFree(struct gbUpdate **updatePtr)
/* free an update object */
{
struct gbUpdate *update = *updatePtr;
if (update != NULL)
    {
    freeMem(update->name);
    freeMem(update);
    *updatePtr = NULL;
    }
}

int gbUpdateCmp(struct gbUpdate* update1, struct gbUpdate* update2)
/* compare two updates for sorting. */
{
if (update1->isFull && update2->isFull)
    return 0;
else if (update1->isFull)
        return -1;
else if (update2->isFull)
    return 1;
else
    return strcmp(update1->name, update2->name);
}

void gbUpdateAddAligned(struct gbUpdate* update,
                        struct gbAligned* aligned)
/* add an aligned object associated with the update */
{
aligned->updateLink = update->aligned;
update->aligned = aligned;
gbUpdateCountAligns(update, aligned, aligned->numAligns);
}

void gbUpdateCountAligns(struct gbUpdate* update, struct gbAligned* aligned,
                         unsigned numAligns)
/* increment the count of the number of alignments */
{
if (aligned->alignOrgCat == GB_NATIVE)
    update->numNativeAligns[gbTypeIdx(aligned->entry->type)] += numAligns;
else
    update->numXenoAligns[gbTypeIdx(aligned->entry->type)] += numAligns;
}

unsigned gbUpdateGetNumAligns(struct gbUpdate* update,
                              unsigned type, unsigned orgCat)
/* Get the number of alignments for a type and orgCat */
{
switch (orgCat) {
case GB_NATIVE:
    return update->numNativeAligns[gbTypeIdx(type)];
case GB_XENO:
    return update->numXenoAligns[gbTypeIdx(type)];
}
assert(FALSE);
return 0;
}

struct gbUpdate* gbUpdateGetPrev(struct gbUpdate* update)
/* get the next oldest update */
{
struct gbUpdate* prev = NULL;
struct gbUpdate* next = update->release->updates;
while (next != update)
    {
    prev = next;
    next = next->next;
    }
return prev;
}

void gbUpdateClearSelectVer(struct gbUpdate* update)
/* Clear the selected version and clientFlags fields in all gbEntry objects in
 * the update.  note that since entries are shared by multiple updates, this
 * can't be used to use with multiple updates at the same time.  It's intended
 * to be a quicker way to clear selectVer by limiting it to one update.
 */
{
struct gbProcessed* proc;
for (proc = update->processed; proc != NULL; proc = proc->next)
    {
    proc->entry->selectVer = NULL_VERSION;
    proc->entry->clientFlags = 0;
    }
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


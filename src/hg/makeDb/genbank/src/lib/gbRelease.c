#include "gbRelease.h"
#include "gbGenome.h"
#include "gbIndex.h"
#include "gbUpdate.h"
#include "gbEntry.h"
#include "gbProcessed.h"
#include "gbAligned.h"
#include "gbFileOps.h"
#include "gbIgnore.h"
#include "common.h"
#include "errno.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"
#include "portable.h"

static char const rcsid[] = "$Id: gbRelease.c,v 1.5 2007/11/16 18:19:50 markd Exp $";

/* size power of hash tables for shared strings */
#define ORG_NAMES_HASH_SIZE   19

/* size power of hash for entries */
#define ACC_HASH_SIZE       22

static void getUpdates(struct gbRelease* release, char* updateGlob, struct slName** head)
/* Search for update directories, adding to list. If a update is not
 * readable, it is skipped. */
{
char path[PATH_LEN], dirPath[PATH_LEN];
struct slName *updDirs;
safef(path, sizeof(path), "%s/%s/%s", release->index->gbRoot,
      GB_PROCESSED_DIR, release->name);
updDirs = listDir(path, updateGlob);
while (updDirs != NULL)
    {
    struct slName *dir = updDirs;
    updDirs = updDirs->next;
    safef(dirPath, sizeof(dirPath), "%s/%s", path, dir->name);
    if (gbIsReadable(dirPath))
        slAddHead(head, dir);
    else
        freeMem(dir);
    }
}

struct gbRelease* gbReleaseNew(struct gbIndex* index, int srcDb, char* name)
/* Construct a new gbRelease object in the empty state */
{
struct gbRelease* release;
char *dot;
struct slName* updates = NULL, *upd;
AllocVar(release); /* zeros struct */
release->srcDb = srcDb;
release->name = cloneString(name);
release->index = index;
release->genome = index->genome;

/* parse the version number from the name */
dot = strchr(name, '.');
if (dot != NULL)
    safef(release->version, sizeof(release->version), "%s", dot+1);
else
    errAbort("can't extract version number from \"%s\"", name);

/* Find existing updates and create objects for them */
getUpdates(release, GB_FULL_UPDATE, &updates);
getUpdates(release, GB_DAILY_UPDATE_GLOB, &updates);

for (upd = updates; upd != NULL; upd = upd->next)
    gbReleaseGetUpdate(release, upd->name);

slFreeList(&updates);
return release;
}

static void entryInit(struct gbRelease* release)
/* initialized entry memory in an object */
{
release->orgNames = hashNew(ORG_NAMES_HASH_SIZE);
release->entryTbl = hashNew(ACC_HASH_SIZE);
release->ignore = gbIgnoreNew(release);
}

static void clearUpdate(struct gbUpdate* update)
/* reset update information on unload */
{
int i;
update->selectProc = FALSE;
update->selectAlign = FALSE;
update->processed = NULL;
update->aligned = NULL;
for (i = 0; i < GB_NUM_TYPES; i++)
    {
    update->numNativeAligns[i] = 0;
    update->numXenoAligns[i] = 0;
    }
}

void gbReleaseUnload(struct gbRelease* release)
/* Unload entry data, freeing memory, keep release and update objects */
{
/* clear links in updates */
struct gbUpdate* update;
for (update = release->updates; update != NULL; update = update->next)
    clearUpdate(update);
#ifdef DUMP_HASH_STATS
if (release->orgNames != NULL)
    hashPrintStats(release->orgNames, "orgNames", stderr);
if (release->entryTbl != NULL)
    hashPrintStats(release->entryTbl, "releaseEntries", stderr);
#endif
gbIgnoreFree(&release->ignore);
hashFree(&release->orgNames);
hashFree(&release->entryTbl);
}

void gbReleaseFree(struct gbRelease** relPtr)
/* Free a gbRelease object */
{
struct gbRelease* release = *relPtr;
if (release != NULL)
    {
    struct gbUpdate *update;
    while ((update = slPopHead(&release->updates)) != NULL)
        gbUpdateFree(&update);
#ifdef DUMP_HASH_STATS
    if (release->orgNames != NULL)
        hashPrintStats(release->orgNames, "orgNames", stderr);
    if (release->entryTbl != NULL)
        hashPrintStats(release->entryTbl, "releaseEntries", stderr);
#endif
    hashFree(&release->orgNames);
    hashFree(&release->entryTbl);
    freeMem(release->name);
    free(release);
    *relPtr = NULL;
    }
}

char* gbReleaseObtainOrgName(struct gbRelease* release, char* orgName)
/* obtain an organism name from the string pool */
{
struct hashEl* hashEl = hashStore(release->orgNames, orgName);
return hashEl->name;
}

static struct gbUpdate* createUpdate(struct gbRelease* release,
                                     char* updateName)
/* create a new update and insert in oldest to newest order */
{
struct gbUpdate* update;
struct gbUpdate* prev = NULL;
struct gbUpdate* scan = release->updates;
update = gbUpdateNew(release, updateName);

while ((scan != NULL) && (gbUpdateCmp(update, scan) > 0))
    {
    prev = scan;
    scan = scan->next;
    }
if (prev == NULL)
    release->updates = update;
else
    prev->next = update;
update->next = scan;
return update;
}

struct gbUpdate* gbReleaseGetUpdate(struct gbRelease* release,
                                    char* updateName)
/* find or create an update  */
{
struct gbUpdate* update = release->updates;
while ((update != NULL) && !sameString(update->name, updateName))
    update = update->next;
if (update == NULL)
    update = createUpdate(release, updateName);

return update;
}

struct gbUpdate* gbReleaseMustFindUpdate(struct gbRelease* release,
                                         char* updateName)
/* find a update or generate an error */
{
struct gbUpdate* update = release->updates;
while ((update != NULL) && !sameString(update->name, updateName))
    update = update->next;

if (update == NULL)
    errAbort("can't find update \"%s\" in release \"%s\"",
             updateName, release->name);
return update;

}

struct gbEntry* gbReleaseFindEntry(struct gbRelease* release, char* acc)
/* find an entry object or null if not found */
{
return hashFindVal(release->entryTbl, acc);
}

static int cmpUpdateDirs(const void *va, const void *vb)
/* Compare two fileInfo with update directories, sort full first */
{
const struct fileInfo *a = *((struct fileInfo **)va);
const char* af = strrchr(a->name, '/');
boolean aIsFull = sameString(af, GB_FULL_UPDATE);
const struct fileInfo *b = *((struct fileInfo **)vb);
const char* bf = strrchr(b->name, '/');
boolean bIsFull = sameString(bf, GB_FULL_UPDATE);

if (aIsFull && bIsFull)
    return 0;
if (aIsFull)
    return -1;
if (bIsFull)
    return 1;
return strcmp(a->name, b->name);
}


static struct fileInfo* getUpdateDirs(struct gbRelease* release, char* section,
                                      struct gbGenome* genome)
/* Get lists of readable update directories in a section (and optional
 * genome database). */
{
int len;
char sectionDir[PATH_LEN];
struct fileInfo* updateDirs = NULL;

/* e.g. $gbRoot/processed/genbank.130.0/full/$type.gbidx */
len = safef(sectionDir, sizeof(sectionDir), "%s/%s/%s",
            release->index->gbRoot,  section, release->name);
if (genome != NULL)
    safef(sectionDir+len, sizeof(sectionDir)-len, "/%s", genome->database);

/* build list of dirs, include full paths */
if (gbIsReadable(sectionDir))
    {
    struct fileInfo *dirList
        = slCat(listDirX(sectionDir, GB_FULL_UPDATE, TRUE),
                listDirX(sectionDir, GB_DAILY_UPDATE_GLOB, TRUE));
    struct fileInfo *dir;
    while ((dir = dirList) != NULL)
        {
        dirList = dirList->next;
        if (gbIsReadable(dir->name))
            slAddHead(&updateDirs, dir);
        else
            freez(&dir);
        }
    }
if (updateDirs != NULL)
    slSort(&updateDirs, cmpUpdateDirs);
return updateDirs;
}

struct slName* gbReleaseFindIndices(struct gbRelease* release, char* section,
                                    struct gbGenome* genome, char* fileName,
                                    char* accPrefix)
/* Build list of index files matching the specified parameters, checked
 * that they are readable.  If accPrefix is not NULL, it is the accession
 * prefix used to partition files. */
{
char idxFile[PATH_LEN];
struct slName* idxFiles = NULL;
struct fileInfo* dirList = getUpdateDirs(release, section, genome);
struct fileInfo* dir = dirList;
while (dir != NULL)
    {
    strcpy(idxFile, dir->name);
    strcat(idxFile, "/");
    if (accPrefix != NULL)
        {
        strcat(idxFile, accPrefix);
        strcat(idxFile, ".");
        }
    strcat(idxFile, fileName);
    if (gbIsReadable(idxFile))
        slSafeAddHead(&idxFiles, newSlName(idxFile));
    dir = dir->next;
    }
slFreeList(&dirList);
return idxFiles;
}

static void addIdxFileAccPrefix(char* idxPath, struct hash* prefixHash)
/* add new a accession prefixe from an index file path */
{
char* slash = NULL, *dot1 = NULL, *dot2 = NULL;
/* extract prefix from .../est.aa.gbidx or .../est.aa.native.alidx */
slash = strrchr(idxPath, '/');
if (slash != NULL)
    {
    dot1 = strchr(slash, '.');
    if (dot1 != NULL)
        dot2 = strchr(dot1+1, '.');
    }
if (dot2 == NULL)
    errAbort("can't parse acc prefix from %s", idxPath);

*dot2 = '\0';
if (hashLookup(prefixHash, dot1+1) == NULL)
    hashAdd(prefixHash, dot1+1, NULL);
*dot2 = '.';
}

static void addIdxFileAccPrefixes(struct fileInfo* idxList,
                                  struct hash* prefixHash)
/* add new accession prefixes from a list of index files */
{
struct fileInfo* idx = idxList;
for (idx = idxList; idx != NULL; idx = idx->next)
    addIdxFileAccPrefix(idx->name, prefixHash);
}

static struct slName* findAccPrefixes(struct gbRelease* release,
                                      char* section, struct gbGenome* genome,
                                      char* baseFileName, char* ext)
/* Find all accession prefixes for the given base file name */
{
struct hash* prefixHash = hashNew(12);
struct fileInfo* dirList = getUpdateDirs(release, section, genome);
struct fileInfo* dir = dirList;
struct slName* prefixes = NULL;
struct hashCookie cookie;
struct hashEl* hel;
char idxGlob[PATH_LEN];

safef(idxGlob, sizeof(idxGlob), "%s.*.%s", baseFileName, ext);

/* build hash of prefixes from index files */
while (dir != NULL)
    {
    struct fileInfo* idxList = listDirX(dir->name, idxGlob, TRUE);
    addIdxFileAccPrefixes(idxList, prefixHash);
    slFreeList(&idxList);
    dir = dir->next;
    }
slFreeList(&dirList);

/* convert hash to a list */
cookie = hashFirst(prefixHash);
while ((hel = hashNext(&cookie)) != NULL)
    slSafeAddHead(&prefixes, newSlName(hel->name));
if (prefixes != NULL)
    slNameSort(&prefixes);
#ifdef DUMP_HASH_STATS
hashPrintStats(prefixHash, "prefix", stderr);
#endif
hashFree(&prefixHash);
return prefixes;

}

struct slName* gbReleaseGetAccPrefixes(struct gbRelease* release,
                                       unsigned state, unsigned type)
/* Find all accession prefixes for processed or aligned data (based
 * on state). */
{
assert((state == GB_PROCESSED) || (state == GB_ALIGNED));
if (state == GB_PROCESSED)
    return findAccPrefixes(release, GB_PROCESSED_DIR, NULL,
                           ((type == GB_MRNA) ? "mrna" : "est"),
                           GBIDX_EXT);
else
    return findAccPrefixes(release, GB_ALIGNED_DIR,
                           release->genome,
                           ((type == GB_MRNA) ? "mrna" : "est"),
                           ALIDX_EXT);
}

void gbReleaseLoadIgnore(struct gbRelease* release)
/* Load the gbIgnore object if not already loaded.  Also loaded by 
 * by gbReleaseLoadProcessed/Aligned, this allows access before one of those
 * methods is called.
 */
{
if (release->entryTbl == NULL)
    entryInit(release);
assert(release->ignore != NULL);
}

void gbReleaseLoadProcessed(struct gbSelect* select)
/* load processed index files. */
{
/* FIXME: maybe we can load just select index sometimes */
struct gbUpdate* saveUpdate = select->update;
if (select->release->entryTbl == NULL)
    entryInit(select->release);

/* load indices for all updates, meeting other criteria */
for (select->update = select->release->updates; select->update != NULL;
     select->update = select->update->next)
    gbProcessedParseIndex(select);
select->update = saveUpdate;
}

void gbReleaseLoadAligned(struct gbSelect* select)
/* load index files from an aligned directory. */
{
struct gbUpdate* saveUpdate = select->update;
if (select->release->orgNames == NULL)
    entryInit(select->release);

/* load indices for all updates, meeting other criteria */
for (select->update = select->release->updates; select->update != NULL;
     select->update = select->update->next)
    gbAlignedParseIndex(select);
select->update = saveUpdate;
}

void gbReleaseClearSelectVer(struct gbRelease* release)
/* clear the selected version and clientFlags fields in all gbEntry objects in
 * the release */
{
struct hashCookie cookie = hashFirst(release->entryTbl);
struct hashEl* hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct gbEntry* entry = hel->val;
    entry->selectVer = NULL_VERSION;
    entry->clientFlags = 0;
    }
}

void gbReleaseDump(struct gbRelease* release, FILE* out, int indent)
/* print a gbRelease object for debugging */
{
fprintf(out, "%*sgbRelease %s\n", indent, "", release->name);
if (release->entryTbl != NULL)
    {
    struct hashCookie cookie = hashFirst(release->entryTbl);
    struct hashEl* hel;
    while ((hel = hashNext(&cookie)) != NULL)
        gbEntryDump((struct gbEntry*)hel->val, out, indent+2);
    }
}

boolean gbReleaseHasData(struct gbRelease* release, unsigned state,
                         unsigned type, char* accPrefix)
/* determine if the release has any processed or aligned entries, based on
 * state, for the specified type */
{
struct fileInfo *updateList, *upd;
boolean found = FALSE;
char releaseDir[PATH_LEN], idxPattern[PATH_LEN];

/* determine directory and index file extension by state state */
assert((state == GB_PROCESSED) || (state == GB_ALIGNED));

if (state == GB_PROCESSED)
    {
    safef(releaseDir, sizeof(releaseDir), "%s/%s/%s",
          release->index->gbRoot, GB_PROCESSED_DIR, release->name);
    /* determine pattern of file to look for */
    if (type == GB_MRNA)
        safef(idxPattern, sizeof(idxPattern), "mrna.%s", GBIDX_EXT);
    else
        {
        if (accPrefix != NULL)
            safef(idxPattern, sizeof(idxPattern), "est.%s.%s", accPrefix,
                  GBIDX_EXT);
        else
            safef(idxPattern, sizeof(idxPattern), "est.*.%s", GBIDX_EXT);
        }
    }
else
    {
    /* aligned */
    safef(releaseDir, sizeof(releaseDir), "%s/%s/%s/%s",
          release->index->gbRoot,  GB_ALIGNED_DIR,
          release->name, release->index->genome->database);
    /* determine pattern of file to look for, with orgCat as wild */
    if (type == GB_MRNA)
        safef(idxPattern, sizeof(idxPattern), "mrna.*.%s", ALIDX_EXT);
    else
        {
        if (accPrefix != NULL)
            safef(idxPattern, sizeof(idxPattern), "est.%s.*.%s", accPrefix,
                  ALIDX_EXT);
        else
            safef(idxPattern, sizeof(idxPattern), "est.*.*.%s", ALIDX_EXT);
        }
    }

/* look in each update */
updateList = listDirX(releaseDir, "*", TRUE);
for (upd = updateList; (upd != NULL) && !found; upd = upd->next)
    {
    if (upd->isDir)
        {
        struct fileInfo *idxList = listDirX(upd->name, idxPattern, FALSE);
        if (idxList != NULL)
            found = TRUE;
        slFreeList(&idxList);
        }
    }
slFreeList(&updateList);
return found;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


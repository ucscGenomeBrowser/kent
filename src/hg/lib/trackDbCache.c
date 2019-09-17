#include <sys/mman.h>
#include <openssl/sha.h>
#include "common.h"
#include "trackDb.h"
#include "hex.h"
#include "portable.h"
#include "localmem.h"
#include "hgConfig.h"

static char *trackDbCacheDir;     // the trackDb cache directory root   

#ifndef CACHELOG
#define cacheLog(a,...)
#else /* CACHELOG */
void vaCacheLog(char *format, va_list args)
/* Call top of warning stack to issue warning. */
{
fputs("cacheLog: ", stderr);
vfprintf(stderr, format, args);
fprintf(stderr, "\n");
fflush(stderr);
}

void cacheLog(char *format, ...)
/* Issue a warning message. */
{
va_list args;
va_start(args, format);
vaCacheLog(format, args);
va_end(args);
}
#endif /* CACHELOG */

struct trackDb *lmCloneSuper(struct lm *lm, struct trackDb *tdb, struct hash *superHash)
/* clone a super track tdb structure. */
{
if (superHash == NULL)
    errAbort("parsing supertrack without superHash");

struct trackDb *super = (struct trackDb *)hashFindVal(superHash, tdb->parent->track);

if (super == NULL)
    {
    super = lmCloneTdb(lm, tdb->parent, NULL, NULL);
    hashAdd(superHash, super->track, super);
    }
lmRefAdd(lm, &super->children, tdb);

return super;
}

struct hashEl *lmCloneHashElList(struct lm *lm, struct hashEl *list)
/* Clone a list of hashEl's. */
{
struct hashEl *newList = NULL;

for(; list; list = list->next)
    {
    struct hashEl *hel = lmAlloc(lm, sizeof(struct hashEl));
    slAddHead(&newList, hel);
    hel->name = lmCloneString(lm, list->name);
    hel->val = lmCloneString(lm, (char *)list->val);  // we're assuming that the values are strings
    hel->hashVal = list->hashVal;
    }

return newList;
}

struct hash *lmCloneHash(struct lm *lm, struct hash *hash)
/* Clone a hash into local memory. */
{
struct hash *newHash = lmAlloc(lm, sizeof(struct hash));

*newHash = *hash;
newHash->lm = NULL;
newHash->ownLm = FALSE;
newHash->next = NULL;
lmAllocArray(lm, newHash->table, hash->size);

int ii;
for(ii=0; ii < hash->size; ii++)
    if (hash->table[ii] != NULL)
        newHash->table[ii] = lmCloneHashElList(lm, hash->table[ii]);

return newHash;
}

struct trackDb *lmCloneTdb(struct lm *lm, struct trackDb *tdb, struct trackDb *parent,  struct hash *superHash)
/* clone a single tdb structure.  Will clone its children if it has any */
{
struct trackDb *newTdb = lmAlloc(lm, sizeof(struct trackDb));

*newTdb = *tdb;

if (tdb->subtracks)
    newTdb->subtracks = lmCloneTdbList(lm, tdb->subtracks, newTdb, NULL);

if ((tdb->parent != NULL) && (superHash != NULL))
    {
    newTdb->parent = lmCloneSuper(lm, newTdb, superHash);
    }
else
    newTdb->parent = parent;

newTdb->track = lmCloneString(lm, tdb->track);
newTdb->table = lmCloneString(lm, tdb->table);
newTdb->shortLabel = lmCloneString(lm, tdb->shortLabel);
newTdb->longLabel = lmCloneString(lm, tdb->longLabel);
newTdb->type = lmCloneString(lm, tdb->type);
if ( newTdb->restrictCount )
    {
    lmAllocArray(lm, newTdb->restrictList, newTdb->restrictCount);
    int ii;
    for(ii=0; ii < newTdb->restrictCount; ii++)
        newTdb->restrictList[ii] = lmCloneString(lm, tdb->restrictList[ii]);
    }
newTdb->url = lmCloneString(lm, tdb->url);
newTdb->html = lmCloneString(lm, tdb->html);
newTdb->grp = lmCloneString(lm, tdb->grp);
newTdb->parentName = lmCloneString(lm, tdb->parentName);

if (tdb->viewHash)
    newTdb->viewHash =  lmCloneHash(lm, tdb->viewHash);
newTdb->children = NULL;
newTdb->overrides = NULL;
newTdb->tdbExtras = NULL;

newTdb->settings = lmCloneString(lm, tdb->settings);
newTdb->settingsHash = lmCloneHash(lm, tdb->settingsHash);

return newTdb;
}

struct trackDb *lmCloneTdbList(struct lm *lm, struct trackDb *list, struct trackDb *parent, struct hash *superHash)
/* clone a list of tdb structures. */
{
struct trackDb *tdb, *prevTdb = NULL, *newList = NULL;

for(tdb = list; tdb; tdb = tdb->next)
    {
    struct trackDb *newTdb = lmCloneTdb(lm, tdb, parent, superHash); 

    if (prevTdb)
        prevTdb->next = newTdb;
    else
        newList = newTdb;

    prevTdb = newTdb;
    }

return newList;
}


static struct trackDb *checkCache(char *string, time_t time)
/* Check to see if this db or hub has a cached trackDb. string is either a db 
 * or a SHA1 calculated from a hubUrl. Use time to see if cache should be flushed. */
{
char dirName[4096];

safef(dirName, sizeof dirName, "%s/%s", trackDbCacheDir, string);
if (!isDirectory(dirName))
    {
    cacheLog("abandoning cache search for %s, no directory", string);
    return NULL;
    }

// look for files named by the address they use
struct slName *files = listDir(dirName, "*");
char fileName[4096];
for(; files; files = files->next)
    {
    safef(fileName, sizeof fileName, "%s/%s/%s", trackDbCacheDir, string, files->name);
    cacheLog("checking cache file %s", fileName);
    
    struct stat statBuf;
    if (stat(fileName, &statBuf) < 0)
        {
        // if we can't stat the shared memory, let's just toss it
        cacheLog("can't stat file %s, unlinking", fileName);
        mustRemove(fileName);
        continue;
        }

    if (statBuf.st_mtime < time)
        {
        // if the cache is older than the data, toss it
        cacheLog("cache is older than source, unlinking");
        mustRemove(fileName);
        continue;
        }

    int oflags = O_RDONLY;
    int fd = open(fileName, oflags);
    if (fd < 0)
        {
        cacheLog("cannot open %s", fileName);
        continue;
        }

    unsigned long address = atoi(files->name); // the name of the file is the address it uses
    unsigned long size = fileSize(fileName);

    u_char *mem = (u_char *) mmap((void *)address, size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    cacheLog("asked for memory %lx of size %ld, got %lx",address, size, mem);

    if ((unsigned long)mem == address)  // make sure we can get this address
        {
        u_char *ret = mem + lmBlockHeaderSize();
        cacheLog("using cache memory at %lx", ret);
        return (struct trackDb *)ret;
        }
    cacheLog("unmapping cache memory at %lx", mem);
    munmap((void *)mem, size);
    close(fd);
    }

cacheLog("abandoning cache search for %s", string);
return NULL;
}


struct trackDb *trackDbCache(char *db, time_t time)
/* Check to see if this db has a cached trackDb. */
{
cacheLog("checking for cache for db %s at time  %ld", db, time);
return checkCache(db, time);
}

struct trackDb *trackDbHubCache(char *trackDbUrl, time_t time)
{
cacheLog("checking for cache for hub %s at time  %ld", trackDbUrl, time);
unsigned char hash[SHA_DIGEST_LENGTH];
SHA1((const unsigned char *)trackDbUrl, strlen(trackDbUrl), hash);

char newName[(SHA_DIGEST_LENGTH + 1) * 2];
hexBinaryString(hash,  SHA_DIGEST_LENGTH, newName, (SHA_DIGEST_LENGTH + 1) * 2);

return checkCache(newName, time);
}

static void cloneTdbListToSharedMem(char *string, struct trackDb *list, unsigned long size, char *name)
/* Allocate shared memory and clone trackDb list into it. */
{
static int inited = 0;

if (inited == 0)
    {
    srandom(time(NULL));
    inited = 1;
    }
    
int oflags=O_RDWR | O_CREAT;

char dirName[4096];
safef(dirName, sizeof dirName, "%s/%s", trackDbCacheDir, string);

if (!isDirectory(dirName))
    {
    cacheLog("making directory %s", dirName);
    makeDir(dirName);
    chmod(dirName, 0777);
    }

char tempFileName[4096];
safef(tempFileName, sizeof tempFileName, "%s/%s", trackDbCacheDir, rTempName(string, "temp", ""));

int fd = open(tempFileName, oflags, 0666 );
if (fd < 0)
    {
    cacheLog("unable to open shared memory %s errno %d", tempFileName, errno);
    mustRemove(tempFileName);
    return;
    }
else
    {
    cacheLog("open shared memory %s", tempFileName);
    }
ftruncate(fd, 0);
ftruncate(fd, size);

size_t psize = getpagesize();
unsigned long pageMask = psize - 1;
unsigned long paddress = 0;

unsigned char *mem;
int numTries = 20;

// we try numTries times to connect to a random address 
for(; numTries; numTries--)
    {
    unsigned long address = random();
    paddress = (address + psize - 1) & ~pageMask;

    mem = (u_char *) mmap((void *)paddress, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    cacheLog("asked for memory %lx of size %ld, got %lx",paddress, size, mem);
    if ((unsigned long)mem == paddress)
        break;
    cacheLog("unmapping memory at %lx",mem);
    munmap((void *)mem, size);
    mem = 0;
    }

if (mem == 0)
    {
    cacheLog("giving up on finding memory");
    mustRemove(tempFileName);
    return;
    }

struct lm *lm = lmInitWMem(mem, size);
struct hash *superHash = newHash(8);

lmCloneTdbList(lm, list, NULL, superHash);

unsigned long memUsed = lmUsed(lm) + lmBlockHeaderSize();
cacheLog("cloning tdbList %p used %ld bytes called with %ld", list, memUsed, size);

msync((void *)paddress, memUsed, MS_SYNC);
ftruncate(fd, memUsed);

// for the moment we're not unmapping these so multiple attached hubs will get
// different addresses
//munmap((void *)paddress, size);
//close(fd);

char fileName[4096];
safef(fileName, sizeof fileName, "%s/%s/%ld", trackDbCacheDir, string, paddress);

cacheLog("renaming %s to %s", tempFileName, fileName);
mustRename(tempFileName, fileName);

// write out the name of the trackDb being cached.
safef(fileName, sizeof fileName, "%s/%s/name.txt", trackDbCacheDir, string);
FILE *stream = mustOpen(fileName, "w");
fprintf(stream, "%s\n", name);
carefulClose(&stream);
}

void trackDbCloneTdbListToSharedMem(char *db, struct trackDb *list, unsigned long size)
/* For this native db, allocate shared memory and clone trackDb list into it. */
{
cacheLog("cloning memory for db %s %ld", db, size);
cloneTdbListToSharedMem(db, list, size, db);
}

void trackDbHubCloneTdbListToSharedMem(char *trackDbUrl, struct trackDb *list, unsigned long size)
/* For this hub, Allocate shared memory and clone trackDb list into it. */
{
if ((*trackDbUrl == '.') || (list == NULL)) // don't cache empty lists or collections
    return;
cacheLog("cloning memory for hub %s %ld", trackDbUrl, size);
unsigned char hash[SHA_DIGEST_LENGTH];
SHA1((const unsigned char *)trackDbUrl, strlen(trackDbUrl), hash);

char newName[(SHA_DIGEST_LENGTH + 1) * 2];
hexBinaryString(hash,  SHA_DIGEST_LENGTH, newName, (SHA_DIGEST_LENGTH + 1) * 2);

cloneTdbListToSharedMem(newName, list, size, trackDbUrl);
}

boolean trackDbCacheOn()
/* Check to see if we're caching trackDb contents. */
{
static boolean checkedCache = FALSE;
static boolean doCache = FALSE;

if (!checkedCache)
    {
    trackDbCacheDir = cfgOption("cacheTrackDbDir");
    if (trackDbCacheDir != NULL)
        doCache = TRUE;

    checkedCache = TRUE;
    }

return doCache;
}

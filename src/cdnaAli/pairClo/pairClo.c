/* pairClo - pair up 3' and 5' ESTs taken from the same clone.
 * Input - an EST .ra file and it's clone-keyed index.
 * Output - a pair list file. */
#include "common.h"
#include "hash.h"
#include "sig.h"
#include "snof.h"
#include "snofmake.h"

enum {
    maxCloNameSize = 64,
    };

void usage()
{
errAbort(
    "pairClo - pair up 3' and 5' ESTs taken from the same clone.\n"
    "usage:\n"
    "   pairClo  est.ra est_clo.ix output.pai\n");
}

struct cloIx
/* Structure to manage an index file. */
    {
    FILE *file;
    int itemSize;
    int maxNameSize;
    char *fileName;
    };

struct cloIx *openIx(char *ixName)
/* Open up index file */
{
struct cloIx *cx;
int sigBuf[4];
FILE *f;

AllocVar(cx);
cx->fileName = ixName;
f = cx->file = mustOpen(ixName, "rb");
mustRead(f, sigBuf, sizeof(sigBuf));
if (!isSnofSig(&sigBuf))
    errAbort("Bad signature on %s", ixName);
mustReadOne(f, cx->maxNameSize);
if (cx->maxNameSize >= maxCloNameSize)
    errAbort("Name field %d chars, can only handle %d", cx->maxNameSize, maxCloNameSize-1);
cx->itemSize = cx->maxNameSize + sizeof(unsigned);
return cx;
}

boolean nextIx(struct cloIx *cx, unsigned *retOff, char retName[maxCloNameSize])
/* Read next record from index file.  Return FALSE at EOF. */
{
int nameSize = cx->maxNameSize;
FILE *f = cx->file;
if (fread(retName, nameSize, 1, f) < 1)
    return FALSE;
retName[nameSize] = 0;
mustRead(f, retOff, sizeof(*retOff));
return TRUE;
}

enum {
    maxValSize = 512,
    };

struct keyVal
/* A list of key/value pairs. */
    {
    struct keyVal *next;
    struct keyVal *listNext;
    char key[16];
    char val[maxValSize];
    };

enum {
    keyHashSize = 32,
};
    
struct keyHash
/* This structure holds one parsed record from
 * a .ra (flat keyed) file. */
    {
    struct keyHash *next;
    struct keyVal *hash[keyHashSize];
    struct keyVal *list;
    };

struct keyVal *freeKeyValList = NULL;
struct keyHash *freeKeyHashList = NULL;

void freeKeyHash(struct keyHash **pKeyHash)
/* Free up keyHash for reuse. */
{
struct keyHash *kh;
if ((kh = *pKeyHash) != NULL)
    {
    struct keyVal *kv, *nextKv;
    for (kv = kh->list; kv != NULL; kv = nextKv)
        {
        nextKv = kv->listNext;
        slAddHead(&freeKeyValList, kv);
        }
    memset(kh, 0, sizeof(*kh));
    slAddHead(&freeKeyHashList, kh);
    *pKeyHash = NULL;
    }
}

struct keyHash *newKeyHash()
/* Get a new keyHash structure, from free list if possible. */
{
struct keyHash *kh;
if ((kh = freeKeyHashList) != NULL)
    {
    freeKeyHashList = kh->next;
    kh->next = NULL;
    return kh;
    }
else
    return needMem(sizeof(struct keyHash));
}

int keyHashFunc(char *key)
/* Hash function optimized for keyHash. */
{
int acc = 0;
char c;
while ((c = *key++) != 0)
    {
    acc <<= 2;
    acc += (c - 'a');
    if (acc > keyHashSize)
        {
        acc += (acc >> 5);
        acc &= (keyHashSize-1);
        }
    }
return acc;
}

void addKey(struct keyHash *kh, char *key, char *val)
/* Add new key/val pair to keyHash. */
{
struct keyVal *kv;
int hashIx;
int keySize, valSize;

/* Get a keyVal. */
if ((kv = freeKeyValList) != NULL)
    {
    freeKeyValList = kv->next;
    kv->next = NULL;
    }
else
    {
    kv = needMem(sizeof(*kv));
    }
/* Make sure that key and val aren't too long. */
keySize = strlen(key);
if (keySize >= sizeof(kv->key))
    errAbort("Key too long in addKey(%s %s)", key, val);
valSize = strlen(val);
if (valSize >= sizeof(kv->val))
    errAbort("Val too long in addKey(%s %s)", key, val);
/* Copy in key and val and add zero tags. */
memcpy(kv->key, key, keySize);
kv->key[keySize] = 0;
memcpy(kv->val, val, valSize);
kv->val[valSize] = 0;
/* Add to hash table. */
hashIx = keyHashFunc(key);
slAddHead(kh->hash+hashIx, kv);
kv->listNext = kh->list;
kh->list = kv;
}

char *getVal(struct keyHash *kh, char *key)
/* Returns val associated with key, or NULL if it doesn't exist. */
{
struct keyVal *kv;
int hashIx = keyHashFunc(key);
for (kv = kh->hash[hashIx]; kv != NULL; kv = kv->next)
    {
    if (sameString(kv->key, key))
        return kv->val;
    }
return NULL;
}


struct keyHash *readKeys(FILE *f)
/* Read in keys from current file position to blank line. */
{
struct keyHash *kh;
char line[2*1024];
int lineCount;
char *key;
int keyCount = 0;
int keySize;
char *val;

if (fgets(line, sizeof(line), f) == NULL)
    return NULL;
kh = newKeyHash();
for (;;)
    {
    ++lineCount;
    key = line;
    val = strchr(line, ' ');
    if (val == NULL)    /* Blank line. */
        break;
    keySize = val-key;
    *val++ = 0;   /* Terminate key string. */
    eraseTrailingSpaces(val);
    ++keyCount;
    addKey(kh, key, val);
    if (fgets(line, sizeof(line), f) == NULL)
        break;
    }
if (keyCount > 0)
    return kh;
else
    {
    freeKeyHash(&kh);
    return NULL;
    }
}

struct estInfo
/* Relevant info about one EST. */
    {
    struct estInfo *next;
    struct keyHash *keys;
    char *clone;
    char *author;
    char *lib;
    char *acc;
    char *dir;
    };

struct estInfo *freeEstInfoList = NULL;

void freeEstInfo(struct estInfo **pEstInfo)
/* Free up estInfo for reuse. */
{
struct estInfo *ei;
if ((ei = *pEstInfo) != NULL)
    {
    freeKeyHash(&ei->keys);
    zeroBytes(ei, sizeof(*ei));
    slAddHead(&freeEstInfoList, ei);
    *pEstInfo = NULL;
    }
}

void freeEstList(struct estInfo **pEstList)
/* Free up list of estInfo's for reuse. */
{
struct estInfo *ei, *nextEi;

for (ei = *pEstList; ei != NULL; ei = nextEi)
    {
    nextEi = ei->next;
    freeEstInfo(&ei);
    }
*pEstList = NULL;
}

boolean sameStringWithNull(char *a, char *b)
/* Returns TRUE if both strings are NULL or both are the same. */
{
if (a == b)
    return TRUE;
if (a == NULL || b == NULL)
    return FALSE;
return sameString(a,b);
}

struct estInfo *findCompatibleClone(struct estInfo *list, struct estInfo *ei)
/* Find a clone that's compatible.  (Assumes that all on list are from same
 * clone, this checks the other fields...). */
{
struct estInfo *el;
char *dir, *opDir;
char *clone = ei->clone;

if ((dir = ei->dir) == NULL)
    return NULL;
if (dir[0] == '3')
    opDir = "5'";
else if (dir[0] == '5')
    opDir = "3'";
else
    errAbort("Unknown dir %s in %s", dir, ei->acc);
for (el = list; el != NULL; el = el->next)
    {
    if ( sameStringWithNull(el->author, ei->author)
      && sameStringWithNull(el->lib, ei->lib)
      && sameStringWithNull(el->dir, opDir) )
        return el;
    }
return NULL;
}

struct estInfo *getNextInfo(struct cloIx *cx, FILE *ra)
/* Read in next info. */
{
unsigned offset;
char cloName[maxCloNameSize];
struct keyHash *kh;
struct estInfo *ei;

if (!nextIx(cx, &offset, cloName))
    return FALSE;
fseek(ra, offset, SEEK_SET);
if ((ei = freeEstInfoList) != NULL)
    {
    freeEstInfoList = ei->next;
    ei->next = NULL;
    }
else
    {
    AllocVar(ei);
    }
if ((kh = ei->keys = readKeys(ra)) == NULL)
    errAbort("Couldn't read keys for %s at %u", cloName, offset);
if ((ei->acc = getVal(kh, "acc")) == NULL)
    errAbort("No accession for %s at %u", cloName, offset);
if ((ei->clone = getVal(kh, "clo")) == NULL)
    errAbort("No clone for %s", ei->acc);
if (!sameString(ei->clone, cloName))
    errAbort("Clone names out of phase %s %s", ei->clone, cloName);
ei->author = getVal(kh, "aut");
ei->lib = getVal(kh, "lib");
ei->dir = getVal(kh, "dir");
return ei;
}

int main(int argc, char *argv[])
{
char *raName, *ixName, *outName;
FILE *ra, *out;
struct cloIx *cx;
struct estInfo *cloneList = NULL, *ei, *mate;
int pairCount = 0;
int estCount;
int cloneCount = 1;

if (argc != 4)
    usage();
raName = argv[1];
ixName = argv[2];
outName = argv[3];

ra = mustOpen(raName, "rb");
cx = openIx(ixName);
out = mustOpen(outName, "w");

while ((ei = getNextInfo(cx, ra)) != NULL)
    {
    ++estCount;
    if ((estCount&0x3fff) == 0)
        {
        putchar('.');
        fflush(stdout);
        }
    if (cloneList != NULL && !sameString(cloneList->clone, ei->clone))
        {
        freeEstList(&cloneList);
        ++cloneCount;
        }
    if ((mate = findCompatibleClone(cloneList, ei)) != NULL)
        {
        ++pairCount;
        if (ei->dir[0] == '5')
            fprintf(out, "%s %s\n", ei->acc, mate->acc);
        else
            fprintf(out, "%s %s\n", mate->acc, ei->acc);
        }
    slAddHead(&cloneList, ei);
    }
printf("Found %d pairs in %d clones in %d ests\n", pairCount, cloneCount, estCount);
return 0;
}


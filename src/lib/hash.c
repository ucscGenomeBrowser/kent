/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Hash.c - implements hashing. */
#include "common.h"
#include "localmem.h"
#include "hash.h"

bits32 hashCrc(char *string)
/* Returns a CRC value on string. */
{
unsigned char *us = (unsigned char *)string;
unsigned char c;
bits32 shiftAcc = 0;
bits32 addAcc = 0;

while ((c = *us++) != 0)
    {
    shiftAcc <<= 2;
    shiftAcc += c;
    addAcc += c;
    }
return shiftAcc + addAcc;
}

struct hashEl *hashLookup(struct hash *hash, char *name)
/* Looks for name in hash table. Returns associated element,
 * if found, or NULL if not. */
{
struct hashEl *el = hash->table[hashCrc(name)&hash->mask];
while (el != NULL)
    {
    if (strcmp(el->name, name) == 0)
        break;
    el = el->next;
    }
return el;
}

struct hashEl *hashAdd(struct hash *hash, char *name, void *val)
/* Add new element to hash table. */
{
struct hashEl *el = lmAlloc(hash->lm, sizeof(*el));
int hashVal = (hashCrc(name)&hash->mask);
int len = strlen(name);
el->name = lmAlloc(hash->lm, len+1);
strcpy(el->name, name);
el->val = val;
el->next = hash->table[hashVal];
hash->table[hashVal] = el;
return el;
}

void *hashRemove(struct hash *hash, char *name)
/* Remove item of the given name from hash table. 
 * Returns value of removed item. */
{
struct hashEl *hel;
void *ret;
struct hashEl **pBucket = &hash->table[hashCrc(name)&hash->mask];
for (hel = *pBucket; hel != NULL; hel = hel->next)
    if (sameString(hel->name, name))
        break;
if (hel == NULL)
    {
    warn("Trying to remove non-existant %s from hash", name);
    return NULL;
    }
ret = hel->val;
slRemoveEl(pBucket, hel);
return ret;
}

struct hashEl *hashAddUnique(struct hash *hash, char *name, void *val)
/* Add new element to hash table. Squawk and die if not unique */
{
if (hashLookup(hash, name) != NULL)
    errAbort("%s duplicated, aborting", name);
return hashAdd(hash, name, val);
}

struct hashEl *hashAddSaveName(struct hash *hash, char *name, void *val, char **saveName)
/* Add new element to hash table.  Save the name of the element, which is now
 * allocated in the hash table, to *saveName.  A typical usage would be:
 *    AllocVar(el);
 *    hashAddSaveName(hash, name, el, &el->name);
 */
{
struct hashEl *hel = hashAdd(hash, name, val);
*saveName = hel->name;
return hel;
}

struct hashEl *hashStore(struct hash *hash, char *name)
/* If element in hash already return it, otherwise add it
 * and return it. */
{
struct hashEl *hel;
if ((hel = hashLookup(hash, name)) != NULL)
    return hel;
return hashAdd(hash, name, NULL);
}

char  *hashStoreName(struct hash *hash, char *name)
/* If element in hash already return it, otherwise add it
 * and return it. */
{
struct hashEl *hel;
if (name == NULL)
    return NULL;
if ((hel = hashLookup(hash, name)) != NULL)
    return hel->name;
return hashAdd(hash, name, NULL)->name;
}

void *hashMustFindVal(struct hash *hash, char *name)
/* Lookup name in hash and return val.  Abort if not found. */
{
struct hashEl *hel = hashLookup(hash, name);
if (hel == NULL)
    errAbort("%s not found", name);
return hel->val;
}

void *hashFindVal(struct hash *hash, char *name)
/* Look up name in hash and return val or NULL if not found. */
{
struct hashEl *hel = hashLookup(hash, name);
if (hel == NULL)
    return NULL;
return hel->val;
}

char *hashMustFindName(struct hash *hash, char *name)
/* Return name as stored in hash table (in hel->name). 
 * Abort if not found. */
{
struct hashEl *hel = hashLookup(hash, name);
if (hel == NULL)
    errAbort("%s not found", name);
return hel->name;
}

struct hash *newHash(int powerOfTwoSize)
/* Returns new hash table. */
{
struct hash *hash = needMem(sizeof(*hash));
int memBlockPower = 16;
if (powerOfTwoSize == 0)
    powerOfTwoSize = 12;
assert(powerOfTwoSize <= 24 && powerOfTwoSize > 0);
hash->powerOfTwoSize = powerOfTwoSize;
hash->size = (1<<powerOfTwoSize);
/* Make size of memory block for allocator vary between
 * 256 bytes and 64k depending on size of table. */
if (powerOfTwoSize < 8)
    memBlockPower = 8;
else if (powerOfTwoSize < 16)
    memBlockPower = powerOfTwoSize;
hash->lm = lmInit(1<<memBlockPower);
hash->mask = hash->size-1;
hash->table = lmAlloc(hash->lm, sizeof(struct hashEl *) * hash->size);
return hash;
}

void hashTraverseEls(struct hash *hash, void (*func)(struct hashEl *hel))
/* Apply func to every element of hash with hashEl as parameter. */
{
int i;
struct hashEl *hel;
for (i=0; i<hash->size; ++i)
    {
    for (hel = hash->table[i]; hel != NULL; hel = hel->next)
	func(hel);
    }
}

void hashTraverseVals(struct hash *hash, void (*func)(void *val))
/* Apply func to every element of hash with hashEl->val as parameter. */
{
int i;
struct hashEl *hel;
for (i=0; i<hash->size; ++i)
    {
    for (hel = hash->table[i]; hel != NULL; hel = hel->next)
	func(hel->val);
    }
}

int hashElCmp(const void *va, const void *vb)
/* Compare two hashEl by name. */
{
const struct hashEl *a = *((struct hashEl **)va);
const struct hashEl *b = *((struct hashEl **)vb);
return strcmp(a->name, b->name);
}

struct hashEl *hashElListHash(struct hash *hash)
/* Return a list of all elements of hash.   Free return with hashElFreeList. */
{
int i;
struct hashEl *hel, *dupe, *list = NULL;
for (i=0; i<hash->size; ++i)
    {
    for (hel = hash->table[i]; hel != NULL; hel = hel->next)
	{
	dupe = CloneVar(hel);
	slAddHead(&list, dupe);
	}
    }
return list;
}


void hashElFree(struct hashEl **pEl)
/* Free hash el list returned from hashListAll.  (Don't use
 * this internally.) */
{
freez(pEl);
}

void hashElFreeList(struct hashEl **pList)
/* Free hash el list returned from hashListAll.  (Don't use
 * this internally. */
{
slFreeList(pList);
}


void freeHash(struct hash **pHash)
/* Free up hash table. */
{
struct hash *hash = *pHash;
if (hash == NULL)
    return;
lmCleanup(&hash->lm);
freez(pHash);
}

void freeHashAndVals(struct hash **pHash)
/* Free up hash table and all values associated with it.
 * (Just calls freeMem on each hel->val) */
{
struct hash *hash;
if ((hash = *pHash) != NULL)
    {
    hashTraverseVals(hash, freeMem);
    freeHash(pHash);
    }
}


/* Hash - a simple hash table that provides name/value pairs. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef HASH_H
#define HASH_H

struct hashEl
    {
    struct hashEl *next;
    char *name;
    void *val;
    };

struct hash
    {
    bits32 mask;
    struct hashEl **table;
    int powerOfTwoSize;
    int size;
    struct lm *lm;
    };

struct hashCookie
/* used by hashFirst/hashNext in tracking location in traversing hash */
    {
    struct hash *hash;      /* hash we are associated with */
    int idx;                /* current index in hash */
    struct hashEl *nextEl;  /* current element in hash */
    };

bits32 hashCrc(char *string);
/* Returns a CRC value on string. */

struct hashEl *hashLookup(struct hash *hash, char *name);
/* Looks for name in hash table. Returns associated element,
 * if found, or NULL if not. */

struct hashEl *hashAdd(struct hash *hash, char *name, void *val);
/* Add new element to hash table. */

void *hashRemove(struct hash *hash, char *name);
/* Remove item of the given name from hash table. 
 * Returns value of removed item. */

struct hashEl *hashAddUnique(struct hash *hash, char *name, void *val);
/* Add new element to hash table. Squawk and die if is already in table. */

struct hashEl *hashAddSaveName(struct hash *hash, char *name, void *val, char **saveName);
/* Add new element to hash table.  Save the name of the element, which is now
 * allocated in the hash table, to *saveName.  A typical usage would be:
 *    AllocVar(el);
 *    hashAddSaveName(hash, name, el, &el->name);
 */

struct hashEl *hashStore(struct hash *hash, char *name);
/* If element in hash already return it, otherwise add it
 * and return it. */

char  *hashStoreName(struct hash *hash, char *name);
/* Put name into hash table. */

char *hashMustFindName(struct hash *hash, char *name);
/* Return name as stored in hash table (in hel->name). 
 * Abort if not found. */

void *hashMustFindVal(struct hash *hash, char *name);
/* Lookup name in hash and return val.  Abort if not found. */

void *hashFindVal(struct hash *hash, char *name);
/* Look up name in hash and return val or NULL if not found. */

void hashAddInt(struct hash *hash, char *name, int val);
/* Store integer value in hash */

int hashIntVal(struct hash *hash, char *name);
/* Return integer value associated with name in a simple 
 * hash of ints. */

void hashTraverseEls(struct hash *hash, void (*func)(struct hashEl *hel));
/* Apply func to every element of hash with hashEl as parameter. */

void hashTraverseVals(struct hash *hash, void (*func)(void *val));
/* Apply func to every element of hash with hashEl->val as parameter. */

struct hashEl *hashElListHash(struct hash *hash);
/* Return a list of all elements of hash.   Free return with hashElFreeList. */

int hashElCmp(const void *va, const void *vb);
/* Compare two hashEl by name. */

void hashElFree(struct hashEl **pEl);
/* Free hash el list returned from hashListAll.  */

void hashElFreeList(struct hashEl **pList);
/* Free hash el list returned from hashListAll.  (Don't use
 * this internally. */

struct hashCookie hashFirst(struct hash *hash);
/* Return an object to use by hashNext() to traverse the hash table.
 * The first call to hashNext will return the first entry in the table. */

struct hashEl* hashNext(struct hashCookie *cookie);
/* Return the next entry in the hash table. Do not modify hash table
 * while this is being used. (see note in code if you want to fix this) */

struct hash *newHash(int powerOfTwoSize);
/* Returns new hash table. */
#define hashNew(a) newHash(a)	/* Synonym */

void freeHash(struct hash **pHash);
/* Free up hash table. */
#define hashFree(a) freeHash(a)	/* Synonym */

void freeHashAndVals(struct hash **pHash);
/* Free up hash table and all values associated with it.
 * (Just calls freeMem on each hel->val) */

#endif /* HASH_H */


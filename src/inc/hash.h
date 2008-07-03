/* Hash - a simple hash table that provides name/value pairs. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef HASH_H
#define HASH_H

struct hashEl
/* An element in a hash list. */
    {
    struct hashEl *next;
    char *name;
    void *val;
    bits32 hashVal;
    };

struct hash
    {
    struct hash *next;	/* Next in list. */
    bits32 mask;	/* Mask hashCrc with this to get it to fit table. */
    struct hashEl **table;	/* Hash buckets. */
    int powerOfTwoSize;		/* Size of table as a power of two. */
    int size;			/* Size of table. */
    struct lm *lm;	/* Local memory pool. */
    int elCount;		/* Count of elements. */
    boolean autoExpand;         /* Automatically expand hash */
    float expansionFactor;      /* Expand when elCount > size*expansionFactor */
    };

#define defaultExpansionFactor 1.0

#define hashMaxSize 28 

struct hashCookie
/* used by hashFirst/hashNext in tracking location in traversing hash */
    {
    struct hash *hash;      /* hash we are associated with */
    int idx;                /* current index in hash */
    struct hashEl *nextEl;  /* current element in hash */
    };

bits32 hashString(char *string);
/* Compute a hash value of a string. */

bits32 hashCrc(char *string);
/* Returns a CRC value on string. */

struct hashEl *hashLookup(struct hash *hash, char *name);
/* Looks for name in hash table. Returns associated element,
 * if found, or NULL if not. */

struct hashEl *hashLookupUpperCase(struct hash *hash, char *name);
/* Lookup upper cased name in hash. (Assumes all elements of hash
 * are themselves already in upper case.) */

struct hashEl *hashLookupNext(struct hashEl *hashEl);
/* Find the next occurance of name that may occur in the table multiple times,
 * or NULL if not found.  Use hashLookup to find the first occurence. */

struct hashEl *hashAdd(struct hash *hash, char *name, void *val);
/* Add new element to hash table. */

struct hashEl *hashAddN(struct hash *hash, char *name, int nameSize, void *val);
/* Add name of given size to hash (no need to be zero terminated) */

void *hashRemove(struct hash *hash, char *name);
/* Remove item of the given name from hash table. 
 * Returns value of removed item, or NULL if not in the table. */

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

void *hashOptionalVal(struct hash *hash, char *name, void *usual);
/* Look up name in hash and return val, or usual if not found. */

struct hashEl *hashAddInt(struct hash *hash, char *name, int val);
/* Store integer value in hash */

void hashIncInt(struct hash *hash, char *name);
/* Increment integer value in hash */

int hashIntVal(struct hash *hash, char *name);
/* Return integer value associated with name in a simple 
 * hash of ints. */

int hashIntValDefault(struct hash *hash, char *name, int defaultInt);
/* Return integer value associated with name in a simple 
 * hash of ints or defaultInt if not found. */

long long hashIntSum(struct hash *hash);
/* Return sum of all the ints in a hash of ints. */

void hashTraverseEls(struct hash *hash, void (*func)(struct hashEl *hel));
/* Apply func to every element of hash with hashEl as parameter. */

void hashTraverseVals(struct hash *hash, void (*func)(void *val));
/* Apply func to every element of hash with hashEl->val as parameter. */

struct hashEl *hashElListHash(struct hash *hash);
/* Return a list of all elements of hash.   Free return with hashElFreeList. */

int hashElCmp(const void *va, const void *vb);
/* Compare two hashEl by name. */

void *hashElFindVal(struct hashEl *list, char *name);
/* Look up name in hashEl list and return val or NULL if not found. */

void hashElFree(struct hashEl **pEl);
/* Free hash el list returned from hashListAll.  */

void hashElFreeList(struct hashEl **pList);
/* Free hash el list returned from hashListAll.  (Don't use
 * this internally. */

struct hashCookie hashFirst(struct hash *hash);
/* Return an object to use by hashNext() to traverse the hash table.
 * The first call to hashNext will return the first entry in the table. */

struct hashEl* hashNext(struct hashCookie *cookie);
/* Return the next entry in the hash table, or NULL if no more. Do not modify
 * hash table while this is being used. (see note in code if you want to fix
 * this) */

void *hashNextVal(struct hashCookie *cookie);
/* Return the next value in the hash table, or NULL if no more. Do not modify
 * hash table while this is being used. */

char *hashNextName(struct hashCookie *cookie);
/* Return the next name in the hash table, or NULL if no more. Do not modify
 * hash table while this is being used. */

struct hash *newHashExt(int powerOfTwoSize, boolean useLocalMem);
/* Returns new hash table. Uses local memory optionally. */
#define hashNewExt(a) newHashExt(a)	/* Synonym */

#define newHash(a) newHashExt(a, TRUE)
/* Returns new hash table using local memory. */
#define hashNew(a) newHash(a)	/* Synonym */

void hashResize(struct hash *hash, int powerOfTwoSize);
/* Resize the hash to a new size */

struct hash *hashFromSlNameList(void *list);
/* Create a hash out of a list of slNames or any kind of list where the */
/* first field is the next pointer and the second is the name. */

void freeHash(struct hash **pHash);
/* Free up hash table. */
#define hashFree(a) freeHash(a)	/* Synonym */

void freeHashAndVals(struct hash **pHash);
/* Free up hash table and all values associated with it.
 * (Just calls freeMem on each hel->val) */

void hashFreeWithVals(struct hash **pHash, void (freeFunc)());
/* Free up hash table and all values associated with it. freeFunc is a
 * function to free an entry, should take a pointer to a pointer to an
 * entry. */

void hashFreeList(struct hash **pList);
/* Free up a list of hashes. */

void hashHisto(struct hash *hash, char *fname);
/* Output bucket usage counts to a file for producing a histogram  */

struct hashEl *hashReplace(struct hash *hash, char *name, void *val);
/* Replace an existing element in hash table, or add it if not present. */

boolean hashMayRemove(struct hash *hash, char *name);
/* Remove item of the given name from hash table, if present.
 * Return true if it was present */

void hashMustRemove(struct hash *hash, char *name);
/* Remove item of the given name from hash table, or error
 * if not present */

char *hashToRaString(struct hash *hash);
/* Convert hash to string in ra format. */

int hashNumEntries(struct hash *hash);
/* count the number of entries in a hash */

#endif /* HASH_H */


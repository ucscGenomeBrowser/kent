/* Hash - a simple hash table that provides name/value pairs. 
 * The most common usage is to create a hash as so:
 *    struct hash *hash = hashNew(0);
 * to add elements to a hash as so:
 *    hashAdd(hash, name, value);
 * and to retrieve a named element as so:
 *    value = hashFindVal(hash, name);
 * The hashFindVal function will return NULL if the name does not
 * appear in the hash.  Alternatively you can use:
 *    value = hashMustFindVal(hash, name);
 * which will abort if name is not in the hash.  When done with a hash do:
 *     hashFree(&hash);
 *
 * The hash does support multiple values for the same key.  To use
 * this functionality, try the loop:
 *     struct hashEl *hel;
 *     for (hel = hashLookup(hash, name); hel != NULL; hel = hashLookupNext(hel))
 *         {
 *         value = hel->val;
 *         // Further processing here.
 * The order of elements retrieved this way will be most-recently-added first.
 * The hashLookup function is _slightly_ faster than the hashFindVal function,
 * so sometimes it is used just to test for the presence of an element in the
 * hash when not interested in the value associated with it.
 *
 * One can iterate through all the elements in a hash in three ways.  One can
 * get a list of all things in the hash as so:
 *     struct hashEl *hel, *helList = hashElListHash(hash);
 *     for (hel = helList; hel != NULL; hel = hel->next)
 *        {
 *        value = hel->val;
 *        // Further processing of value here. 
 *        }
 *     hashElFreeList(&helList);
 * One can avoid the overhead of creating a list by using an iterator object
 * and function:
 *    struct hashEl *hel;
 *    struct hashCookie cookie = hashFirst(hash);
 *    while ((hel = hashNext(&cookie)) != NULL)
 *        {
 *        value = hel->val;
 * Finally one can use hashTraverseEls or hashTraverseVals with a callback 
 * function that takes a hashEl, or the void *value as parameter respectively.
 *
 * There are various other functions involving hashes in this module as well
 * that provide various short cuts.  For information on these and additional
 * details of the functions described, please read the full hash.h file, and
 * if so inclined the hash.c file as well.
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
    int numResizes;             /* number of times resize was called */
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
 * if found, or NULL if not.  If there are multiple entries
 * for name, the last one added is returned (LIFO behavior).
 */

struct hashEl *hashLookupUpperCase(struct hash *hash, char *name);
/* Lookup upper cased name in hash. (Assumes all elements of hash
 * are themselves already in upper case.) */

struct hashEl *hashLookupNext(struct hashEl *hashEl);
/* Find the next occurance of name that may occur in the table multiple times,
 * or NULL if not found.  Use hashLookup to find the first occurrence.  Elements
 * are returned in LIFO order.
 */

struct hashEl *hashAdd(struct hash *hash, char *name, void *val);
/* Add new element to hash table.  If an item with name, already exists, a new
 * item is added in a LIFO manner.  The last item added for a given name is
 * the one returned by the hashLookup functions.  hashLookupNext must be used
 * to find the preceding entries for a name.
 */

struct hashEl *hashAddN(struct hash *hash, char *name, int nameSize, void *val);
/* Add name of given size to hash (no need to be zero terminated) */

void *hashRemove(struct hash *hash, char *name);
/* Remove item of the given name from hash table. 
 * Returns value of removed item, or NULL if not in the table.
 * If their are multiple entries for name, the last one added
 * is removed (LIFO behavior).
 */

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

void *hashFindValUpperCase(struct hash *hash, char *name);
/* Lookup upper cased name in hash and return val or return NULL if not found.
 * (Assumes all elements of hash are themselves already in upper case.) */

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

int hashElCmpWithEmbeddedNumbers(const void *va, const void *vb);
/* Compare two hashEl by name sorting including numbers within name,
 * suitable for chromosomes, genes, etc. */

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
/* Create a hash out of a list of slNames. */

struct hash *hashSetFromSlNameList(void *list);
/* Create a hashSet (hash without values) out of a list of slNames. */

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

void hashPrintStats(struct hash *hash, char *label, FILE *fh);
/* print statistic about a hash table */

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

struct hash *hashFromString(char *string);
/* parse a whitespace-separated string with tuples in the format name=val or
 * name="val" to a hash name->val */

#endif /* HASH_H */


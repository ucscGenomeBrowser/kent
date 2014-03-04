/* longList - this is a way of associating a generic list with a long key.
 * You can retrieve back the list given the key,  and add to lists associated with
 * any given tree. The list can be any sort of singly-linked object that starts with
 * a next field. 
 *
 * Under the hood this uses a balanced binary tree to handle the key/list
 * association. */

#include "common.h"
#include "localmem.h"
#include "rbTree.h"
#include "longList.h"

static int llKeyListCmp(void *va, void *vb)
/* Compare to sort two llKeyLists on key */
{
/* Point typed pointers at a and b */
struct llKeyList *a = va;
struct llKeyList *b = vb;

/* We can't just return a->key - b->key because of long/int overflow */
if (a->key > b->key)
    return 1;
else if (a->key < b->key)
    return -1;
else
    return 0;
}

struct longList *longListNew()
/* Return new empty longList */
{
struct longList *ll;
AllocVar(ll);
ll->tree = rbTreeNew(llKeyListCmp);
ll->lm = ll->tree->lm;
return ll;
}

void longListFree(struct longList **pLl)
/* Free up memory associated with longList. */
{
struct longList *ll = *pLl;
if (ll != NULL)
    {
    rbTreeFree(&ll->tree);
    freez(pLl);
    }
}

struct llKeyList *longListAdd(struct longList *ll, long key, void *item)
/* Add item to key-associated record in ll.  May make up new record if the
 * key has never been seen before.  If record already exists item will be prepended
 * to any existing items. Returns llKeyList associated with key, but most people just
 * ignore that. */
{
struct slRef *ref = lmAlloc(ll->lm, sizeof(struct slRef));
ref->val = item;
struct llKeyList find;
find.key = key;
struct llKeyList *lkl = rbTreeFind(ll->tree, &find);
if (lkl == NULL)
    {
    lmAllocVar(ll->lm, lkl);
    lkl->key = key;
    rbTreeAdd(ll->tree, lkl);
    }
slAddHead(&lkl->list, ref);
return lkl;
}

struct llKeyList *longListLookup(struct longList *ll, long key)
/* Given a key return llKeyList associated with it if any, may return NULL */
{
struct llKeyList find;
find.key = key;
return rbTreeFind(ll->tree, &find);
}

struct slRef *longListFindVal(struct longList *ll, long key)
/* Returns the list associated with this key, or NULL if key not in container. */
{
struct llKeyList *lkl = longListLookup(ll, key);
if (lkl == NULL)
    return NULL;
else
    return lkl->list;
}

struct slRef *longListMustFindVal(struct longList *ll, long key)
/* Returns the list associated with this key. Aborts if key not found in container. */
{
struct llKeyList *lkl = longListLookup(ll, key);
if (lkl == NULL)
    errAbort("Can't find %ld in longList, in longListMustFindVal", key);
return lkl->list;
}




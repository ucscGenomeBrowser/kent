/* intValTree - a binary tree with integer keys and void values.  This is based on the 
 * red/black self-balancing binary tree algorithm in the rbTree module. */

#include "common.h"
#include "localmem.h"
#include "rbTree.h"
#include "intValTree.h"

int intValCmp(void *va, void *vb)
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */
{
struct intVal *a = va;
struct intVal *b = vb;
return a->key - b->key;
}

struct rbTree *intValTreeNew()
/* Create a new, empty, tree with integer keys and void values. */
{
return rbTreeNew(intValCmp);
}

struct intVal *intValTreeAdd(struct rbTree *tree, int key, void *val)
/* Add to binary tree.  Will abort if key is already in tree. */
{
struct intVal *iv;
lmAllocVar(tree->lm, iv);
iv->key = key;
iv->val = val;
if (rbTreeAdd(tree, iv) != NULL)
    errAbort("Key %d already exists in tree", key);
return iv;
}

struct intVal *intValTreeUpdate(struct rbTree *tree, int key, void *val)
/* Add to binary tree. If key is already in tree just updates it with val. */
{
struct intVal *iv = intValTreeLookup(tree, key);
if (iv != NULL)
    iv->val = val;
else
    iv = intValTreeAdd(tree, key, val);
return iv;
}

struct intVal *intValTreeRemove(struct rbTree *tree, int key)
/* Removes given tree from key. */
{
struct intVal fullKey;
fullKey.key = key;
return rbTreeRemove(tree, &fullKey);
}

struct intVal *intValTreeLookup(struct rbTree *tree, int key)
/* Returns intVal associated with given key, or NULL if none exists. */
{
struct intVal fullKey;
fullKey.key = key;
return rbTreeFind(tree, &fullKey);
}


void *intValTreeFind(struct rbTree *tree, int key)
/* Returns value associated with given key, or NULL if none exists. */
{
struct intVal fullKey;
fullKey.key = key;
struct intVal *iv = rbTreeFind(tree, &fullKey);
if (iv == NULL)
    return NULL;
return iv->val;
}

void *intValTreeMustFind(struct rbTree *tree, int key)
/* Return value associated with given key. Aborts if none exists. */
{
struct intVal fullKey;
fullKey.key = key;
struct intVal *iv = rbTreeFind(tree, &fullKey);
if (iv == NULL)
    errAbort("%d is not in tree", key);
return iv->val;
}

void rbTreeTraverse(struct rbTree *tree, void (*doItem)(void *item));

void doAllKeys(void *item, void *context)
/* Callback function for tree traversal. */
{
int **pPt = context;
struct intVal *iv = item;
**pPt = iv->key;
*pPt += 1;
}

int *intValTreeKeys(struct rbTree *tree)
/* Returns array of keys (size is tree->n).  You freeMem this when done. */
{
int *results, *pt;
pt = AllocArray(results, tree->n);
rbTreeTraverseWithContext(tree, doAllKeys, &pt);
return results;
}

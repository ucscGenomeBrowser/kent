/* intValTree - a binary tree with integer keys and void values.  This is based on the 
 * red/black self-balancing binary tree algorithm in the rbTree module. */

#ifndef INTVALTREE_H
#define INTVALTREE_H

#ifndef RBTREE_H
#include "rbTree.h"
#endif

struct rbTree *intValTreeNew();
/* Create a new, empty, tree with integer keys and void values. */

#define intValTreeFree(a) rbTreeFree(a)
/* Free up intVal tree.  */

struct intVal
/* An integer coupled with a void value. */
    {
    int key;
    void *val;
    };

int intValCmp(void *va, void *vb);
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */

struct intVal *intValTreeAdd(struct rbTree *tree, int key, void *val);
/* Add to binary tree.  Will abort if key is already in tree. */

struct intVal *intValTreeUpdate(struct rbTree *tree, int key, void *val);
/* Add to binary tree. If key is already in tree just updates it with val. */

struct intVal *intValTreeRemove(struct rbTree *tree, int key);
/* Removes given tree from key. */

struct intVal *intValTreeLookup(struct rbTree *tree, int key);
/* Returns intVal associated with given key, or NULL if none exists. */

void *intValTreeFind(struct rbTree *tree, int key);
/* Returns value associated with given key, or NULL if none exists. */

void *intValTreeMustFind(struct rbTree *tree, int key);
/* Return value associated with given key. Aborts if none exists. */

int *intValTreeKeys(struct rbTree *tree);
/* Returns array of keys (size is tree->n).  You freeMem this when done. */

#endif /* INTVALTREE_H */


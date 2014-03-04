/* longList - this is a way of associating a generic list with a long key.
 * You can retrieve back the list given the key,  and add to lists associated with
 * any given tree. The list can be any sort of singly-linked object that starts with
 * a next field. 
 *
 * Under the hood this uses a balanced binary tree to handle the key/list
 * association. */

#ifndef LONGLISTTREE_H

struct longList
/* A long and a opaquely typed list */
    {
    struct longList *next;  /* In case this list is part of a regular list */
    struct rbTree *tree;    /* Self balancing tree with long val/list key */
    struct lm *lm;	    /* Optimized memory allocation thing */
    };

struct llKeyList
/* These are what inhabit a longList->tree.  They are generally allocated out of a lm,
 * so don't look for a free routine. */
    {
    long key;	 /* Some sort of binary key value */
    struct slRef *list;  /* Some sort of singly linked list */
    };

struct longList *longListNew();
/* Return new empty longList */

void longListFree(struct longList **pLl);
/* Free up memory associated with longList. */

struct llKeyList *longListAdd(struct longList *ll, long key, void *item);
/* Add item to key-associated record in ll.  May make up new record if the
 * key has never been seen before.  If record already exists item will be prepended
 * to any existing items. Returns llKeyList associated with key, but most people just
 * ignore that. */

struct llKeyList *longListLookup(struct longList *ll, long key);
/* Given a key return llKeyList associated with it if any, may return NULL */

struct slRef *longListFindVal(struct longList *ll, long key);
/* Returns the list associated with this key, or NULL if key not in container. */

struct slRef *longListMustFindVal(struct longList *ll, long key);
/* Returns the list associated with this key. Aborts if key not found in container. */




#endif /* LONGLISTTREE_H */


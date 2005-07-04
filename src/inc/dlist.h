/* dlist.h - Headers for generic doubly-linked list routines. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef DLIST_H
#define DLIST_H

#ifndef COMMON_H
#include "common.h"
#endif

struct dlNode
/* An element on a doubly linked list. */
    {
    struct dlNode *next;
    struct dlNode *prev;
    void *val;
    };

struct dlList
/* A doubly linked list. */
    {
    struct dlNode *head;
    struct dlNode *nullMiddle;
    struct dlNode *tail;
    };

#define dlEnd(node) (node->next == NULL)
/* True if node past end. */

#define dlStart(node) (node->prev == NULL)
/* True if node before start. */

/* Iterate on a doubly linked list as so:
    for (el = list->head; !dlEnd(el); el = el->next)
        val = el->val;
   or
    for (el = list->tail; !dlStart(el); el = el->prev)
        val = el->val;
 */

struct dlList *newDlList();
/* Return a new doubly linked list. */

#define dlListNew newDlList
/* Add object-first synonym. */

void dlListInit(struct dlList *dl);
/* Initialize list to be empty */

void dlListReset(struct dlList *dl);
/* Reset a list to the empty state (does not free values)  */

void freeDlList(struct dlList **pList);
/* Free up a doubly linked list and it's nodes (but not the node values). */
#define dlListFree freeDlList

void freeDlListAndVals(struct dlList **pList);
/* Free all values in doubly linked list and the list itself.  (Just calls
 * freeMem on all values. */
#define dlListFreeAndVals freeDlListAndVals

void dlAddBefore(struct dlNode *anchor, struct dlNode *newNode);
/* Add a node to list before anchor member. */

void dlAddAfter(struct dlNode *anchor, struct dlNode *newNode);
/* Add a node to list after anchor member. */

void dlAddHead(struct dlList *list, struct dlNode *newNode);
/* Add a node to head of list. */

void dlAddTail(struct dlList *list, struct dlNode *newNode);
/* Add a node to tail of list. */

struct dlNode *dlAddValBefore(struct dlNode *anchor, void *val);
/* Create a node containing val and add to list before anchor member. */

struct dlNode *dlAddValAfter(struct dlNode *anchor, void *val);
/* Create a node containing val and add to list after anchor member. */

struct dlNode *dlAddValHead(struct dlList *list, void *val);
/* Create a node containing val and add to head of list. */

struct dlNode *dlAddValTail(struct dlList *list, void *val);
/* Create a node containing val and add to tail of list. */

void dlRemove(struct dlNode *node);
/* Removes a node from list. Node is not freed. */

void dlRemoveHead(struct dlList *list);
/* Removes head from list. Node is not freed. */

void dlRemoveTail(struct dlList *list);
/* Remove tail from list. Node is not freed.  */

struct dlNode *dlPopHead(struct dlList *list);
/* Remove first node from list and return it. */

struct dlNode *dlPopTail(struct dlList *list);
/* Remove last node from list and return it. */

void dlDelete(struct dlNode **nodePtr);
/* Removes a node from list and frees it. */

int dlCount(struct dlList *list);
/* Return length of list. */

boolean dlEmpty(struct dlList *list);
/* Return TRUE if list is empty. */

#define dlIsEmpty(list) ((list)->head->next == NULL)
/* Return TRUE if list is empty.  Macro version of above. */

struct dlNode *dlGetBeforeHead(struct dlList *list);
/* Get the node before the head of the list */

struct dlNode *dlGetAfterTail(struct dlList *list);
/* Get the node after the tail of the list */

void dlSort(struct dlList *list, int (*compare )(const void *elem1,  const void *elem2));
/* Sort a doubly linked list with Qsort and a temporary array. 
 * The arguments to the compare function in real, non-void, life
 * are pointers to pointers of the type that is in the val field of 
 * the nodes of the list. */

void *dlListToSlList(struct dlList *dList);
/* Return slList from dlList. */

void dlCat(struct dlList *a, struct dlList *b);
/* Move items from b to end of a. */

struct dlNode *dlValInList(struct dlList *list, void *val);
/* Return node on list if any that has associated val. */

#endif /* DLIST_H */



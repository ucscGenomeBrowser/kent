/* dlist.c - Doubly-linked list routines. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
#include "common.h"
#include "dlist.h"

void dlListInit(struct dlList *dl)
/* Initialize list to be empty */
{
dl->head = (struct dlNode *)(&dl->nullMiddle);
dl->nullMiddle = NULL;
dl->tail = (struct dlNode *)(&dl->head);
}

struct dlList *newDlList()
/* Return a new doubly linked list. */
{
struct dlList *dl;
AllocVar(dl);
dl->head = (struct dlNode *)(&dl->nullMiddle);
dl->tail = (struct dlNode *)(&dl->head);
return dl;
}

void freeDlList(struct dlList **pList)
/* Free up a doubly linked list and it's nodes (but not the node values). */
{
struct dlList *list = *pList;
if (list != NULL)
    {
    struct dlNode *node, *next;
    for (node = list->head; node->next != NULL; node = next)
        {
        next = node->next;
        freeMem(node);
        }
    freez(pList);
    }
}

void freeDlListAndVals(struct dlList **pList)
/* Free all values in doubly linked list and the list itself.  (Just calls
 * freeMem on all values. */
{
struct dlList *list = *pList;
if (list != NULL)
    {
    struct dlNode *node;
    for (node = list->head; node->next != NULL; node = node->next)
        freeMem(node->val);
    freeDlList(pList);
    }
}


void dlInsertBetween(struct dlNode *before, struct dlNode *after, struct dlNode *newNode)
{
before->next = newNode; 
newNode->prev = before; 
newNode->next = after;  
after->prev = newNode; 
}

void dlAddBefore(struct dlNode *anchor, struct dlNode *newNode)
/* Add a node to list before anchor member. */
{
dlInsertBetween(anchor->prev, anchor, newNode);
}

void dlAddAfter(struct dlNode *anchor, struct dlNode *newNode)
/* Add a node to list after anchor member. */
{
dlInsertBetween(anchor, anchor->next, newNode);
}

void dlAddHead(struct dlList *list, struct dlNode *newNode)
/* Add a node to head of list. */
{
struct dlNode *head = list->head;
dlInsertBetween(head->prev, head, newNode);
}

void dlAddTail(struct dlList *list, struct dlNode *newNode)
/* Add a node to tail of list. */
{
struct dlNode *tail = list->tail;
dlInsertBetween(tail, tail->next, newNode);
}

struct dlNode *dlAddValBefore(struct dlNode *anchor, void *val)
/* Create a node containing val and add to list before anchor member. */
{
struct dlNode *node = AllocA(struct dlNode);
node->val = val;
dlAddBefore(anchor, node);
return node;
}

struct dlNode *dlAddValAfter(struct dlNode *anchor, void *val)
/* Create a node containing val and add to list after anchor member. */
{
struct dlNode *node = AllocA(struct dlNode);
node->val = val;
dlAddAfter(anchor, node);
return node;
}

struct dlNode *dlAddValHead(struct dlList *list, void *val)
/* Create a node containing val and add to head of list. */
{
struct dlNode *node = AllocA(struct dlNode);
node->val = val;
dlAddHead(list, node);
return node;
}

struct dlNode *dlAddValTail(struct dlList *list, void *val)
/* Create a node containing val and add to tail of list. */
{
struct dlNode *node = AllocA(struct dlNode);
node->val = val;
dlAddTail(list, node);
return node;
}

void dlRemove(struct dlNode *node)
/* Removes a node from list. */
{
struct dlNode *before = node->prev;
struct dlNode *after = node->next;
before->next = after;
after->prev = before;
}

void dlRemoveHead(struct dlList *list)
/* Removes head from list. */
{
dlRemove(list->head);
}

void dlRemoveTail(struct dlList *list)
/* Remove tail from list. */
{
dlRemove(list->tail);
}

struct dlNode *dlPopHead(struct dlList *list)
/* Remove first node from list and return it. */
{
struct dlNode *node = list->head;
if (node->next == NULL)
    return NULL;
dlRemove(node);
return node;
}

struct dlNode *dlPopTail(struct dlList *list)
/* Remove last node from list and return it. */
{
struct dlNode *node = list->tail;
if (node->prev == NULL)
    return NULL;
dlRemove(node);
return node;
}

int dlCount(struct dlList *list)
/* Return length of list. */
{
return slCount(list->head) - 1;
}

void dlSort(struct dlList *list, int (*compare )(const void *elem1,  const void *elem2))
/* Sort a singly linked list with Qsort and a temporary array. 
 * The arguments to the compare function in real, non-void, life
 * are pointers to pointers of the type that is in the val field of 
 * the nodes of the list. */
{
int len = dlCount(list);

if (len > 1)
    {
    /* Move val's onto an array, sort, and then put back into list. */
    void **sorter = needLargeMem(len * sizeof(sorter[0]));
    struct dlNode *node;
    int i;

    for (i=0, node = list->head; i<len; ++i, node = node->next)
        sorter[i] = node->val;
    qsort(sorter, len, sizeof(sorter[0]), compare);
    for (i=0, node = list->head; i<len; ++i, node = node->next)
        node->val = sorter[i];
    freeMem(sorter);
    }
}

boolean dlEmpty(struct dlList *list)
/* Return TRUE if list is empty. */
{
return (list->head->next == NULL);
}

void *dlListToSlList(struct dlList *dList)
/* Return slList from dlList. */
{
struct slList *list = NULL, *el;
struct dlNode *node;

for (node = dList->tail; node->prev != NULL; node = node->prev)
    {
    el = node->val;
    slAddHead(&list, el);
    }
return list;
}

void dlCat(struct dlList *a, struct dlList *b)
/* Move items from b to end of a. */
{
struct dlNode *node;
while ((node = dlPopHead(b)) != NULL)
    dlAddTail(a, node);
}

struct dlNode *dlValInList(struct dlList *list, void *val)
/* Return node on list if any that has associated val. */
{
struct dlNode *node;
for (node = list->head; !dlEnd(node); node = node->next)
    if (node->val == val)
        return node;
return NULL;
}

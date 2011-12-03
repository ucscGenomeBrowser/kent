/* ShaRes.c - implementation of shared resources 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "shaRes.h"


static void shaFreeNode(struct shaResNode *node)
/* Free a node.  (Don't look at link count, just do it.) */
{
struct shaResNode *next = node->next;
struct shaResNode *prev = node->prev;
struct shaResList *list = node->list;

if (next != NULL)
    next->prev = prev;
if (prev != NULL)
    prev->next = next;
else
    list->head = next;
list->freeData(node->data);
freeMem(node->name);
freeMem(node);
}        

void shaUnlink(struct shaResNode *node)
/* Decrement link count and free if down to zero. */
{
if (--node->links <= 0)
    shaFreeNode(node);
}

void shaLink(struct shaResNode *node)
/* Increment link count. */
{
++node->links;
}

struct shaResNode *shaNewNode(struct shaResList *list, char *name, void *data)
/* Create a new node with link count of one. */
{
struct shaResNode *nn = needMem(sizeof(*nn));
struct shaResNode *head = list->head;

/* Store the goods and what list we're on, and start with one link. */
nn->list = list;
nn->data = data;
nn->links = 1;
nn->name = cloneString(name);

/* Put us at the front of the list. */
nn->next = head;
nn->prev = NULL;
if (head != NULL)
    head->prev = nn;
list->head = nn;
return nn;
}

void shaCleanup(struct shaResList *list)
/* Free every node on list. */
{
struct shaResNode *node, *next;

for (node = list->head;node != NULL;node = next)
    {
    next = node->next;
    shaFreeNode(node);
    }
list->head = NULL;
}

void shaInit(struct shaResList *list, void (*freeData)(void *pData))
/* Start up resource list. */
{
list->head = NULL;
list->freeData = freeData;
}

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "rmskOutExtra.h"

void rmskOutExtraFree(struct rmskOutExtra **pThing)
/* Free one */
{
struct rmskOutExtra *thing = *pThing;
if (thing != NULL)
    rmskOutFree(&thing->rmsk);
freez(pThing);
}

void rmskOutExtraFreeList(struct rmskOutExtra **pList)
/* Free a list of dynamically allocated rmskOutExtra's */
{
struct rmskOutExtra *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    rmskOutExtraFree(&el);
    }
*pList = NULL;
}

void addToRmskOutExtraHash(struct hash *repHash, struct rmskOutExtra *addme)
/* Add one to the hash of lists, indexed on the rmsk->repName. */
{
struct hashEl *el = NULL;
if (!addme || !addme->rmsk)
    return;
el = hashLookup(repHash, addme->rmsk->repName);
if (el != NULL)
    {
    struct rmskOutExtra *list = el->val;    
    slAddHead(&list, addme);
    el->val = list;
    }
else
    {
    char *name = cloneString(addme->rmsk->repName);
    hashAdd(repHash, name, addme);
    } 
}

void freeRmskOutExtraHash(struct hash **pHash)
/* Free up that crazy hash. */
{
struct hash *myhash;
if ((myhash = *pHash) != NULL)
    {
    struct hashEl *elList = NULL, *el;
    elList = hashElListHash(myhash);
    for (el = elList; el != NULL; el = el->next)
	rmskOutExtraFreeList((struct rmskOutExtra **)&el->val);
    hashElFreeList(&elList);
    }
hashFree(pHash);
}


/* isxLiveVar - Manage live variable lists for intermediate code. */


#include "common.h"
#include "dlist.h"
#include "pfParse.h"
#include "pfToken.h"
#include "pfType.h"
#include "pfScope.h"
#include "pfCompile.h"
#include "isx.h"
#include "isxLiveVar.h"

#ifdef OLD
static void isxLiveList(struct dlList *iList)
/* Create list of live variables at each instruction by scanning
 * backwards. */
{
struct dlNode *node;
struct slRef *liveList = NULL;
for (node = iList->tail; !dlStart(node); node = node->prev)
    {
    struct slRef *newList = NULL, *ref;
    struct isx *isx = node->val;
    struct isxAddress *iad;

    /* Save away current live list. */
    isx->liveList = liveList;

    /* Make copy of live list minus any overwritten dests. */
    for (ref = liveList; ref != NULL; ref = ref->next)
	{
	struct isxAddress *iad = ref->val;
	if (iad != isx->dest)
	    refAdd(&newList, iad);
	}

    /* Add sources to live list */
    iad = isx->left;
    if (iad != NULL && (iad->adType == iadRealVar || iad->adType == iadTempVar))
	refAddUnique(&newList, iad);
    iad = isx->right;
    if (iad != NULL && (iad->adType == iadRealVar || iad->adType == iadTempVar))
	refAddUnique(&newList, iad);

    /* Flip to new live list */
    liveList = newList;
    }
slFreeList(&liveList);
}
#endif /* OLD */

#ifdef EXAMPLE
z = 12;              {z}
y = z + 2;           {z,y}
x = y + z;           {z,x}
i = 0;               {i,z,x}
goto cond;           {i,z,x}
startLoop:           {i,z,x}  [i,z,x]
a = call x;          {i,z,a,x}[i,z,x,a]
b = call a,x;        {i,z,b}  [i,z,x,b]
    call b;          {i,z}    [i,z,x]
cond:
i = i + 1;           {i,z}    [i,z,x]
startLoop if (i<z);  {z}      [i,z,x]
endLoop:             {}       [i,z,x]
z = z + 3;           {}       []

x = 9                {x}
z = 12               {x,z}
y =  2               {x,y,z}
b = 0                {x,y,z}
falseCase if (z < y) {x,y,z}
startCond:     {x,y,z}
case trueCase: {x,z}
a = z          {x,z}
goto endCond;  {x}
case falseCase:{x,y}
a = y          {x,y}
b = a+6        {x,a}
endCond:       {x}
call x         {}

#endif /* EXAMPLE */

static struct isxLiveVar *isxLiveVarNew(struct isxAddress *iad, int pos)
/* Creat new live list entry */
{
struct isxLiveVar *live;
AllocVar(live);
live->var = iad;
live->useCount = 1;
live->usePos[0] = pos;
return live;
}

struct isxLiveVar *isxLiveVarFind(struct isxLiveVar *liveList, 
	struct isxAddress *iad)
/* Return liveVar associated with address, or NULL if none. */
{
struct isxLiveVar *live;
for (live = liveList; live != NULL; live = live->next)
    {
    if (live->var == iad)
        return live;
    }
return NULL;
}

static struct isxLiveVar *isxLiveVarAdd(struct isxLiveVar *liveList,
	struct isxAddress *iad, int pos)
/* Add live var to list and return new list. */
{
struct isxLiveVar *live = isxLiveVarFind(liveList, iad);
if (live == NULL)
    {
    live = isxLiveVarNew(iad, pos);
    slAddHead(&liveList, live);
    }
else
    {
    live->useCount += 1;
    live->usePos[1] = live->usePos[0];
    live->usePos[0] = pos;
    }
return liveList;
}

struct condStack
/* A stack of conditionals */
    {
    struct condStack *next;	/* Next in stack. */
    struct isxLiveVar *liveList;	/* Live list at this node. */
    struct dlNode *node;	/* Node where we pushed liveList */
    struct isxLiveVar *condLiveList;	/* Live list at start of condition. */
    };

static void pushCond(struct condStack **pStack, struct isxLiveVar *liveList,
	struct dlNode *node)
/* Alloc and push conditional stack entry */
{
struct condStack *cond;
AllocVar(cond);
cond->liveList = liveList;
cond->node = node;
slAddHead(pStack, cond);
}

static void popCond(struct condStack **pStack)
/* Pop and free cond stack entry */
{
struct condStack *cond = *pStack;
assert(cond != NULL);
*pStack = cond->next;
freeMem(cond);
}

static void foldInCaseLive(struct condStack *ls, struct isxLiveVar *liveList)
/* Fold the liveList into the condLiveList. */
{
struct isxLiveVar *live, *next;
uglyf("foldInCaseLive, %d\n", slCount(liveList));
for (live = liveList; live != NULL; live = live->next)
    {
    struct isxLiveVar *condLive = isxLiveVarFind(ls->condLiveList, live->var);
    if (condLive == NULL)
         {
	 /* If var first appears in this case, just clone it. */
	 condLive = CloneVar(live);
	 slAddHead(&ls->condLiveList, condLive);
	 }
    else
         {
	 /* If var is in another case as well, then make useCount and
	  * positions reflect the soonest and heaviest use in any particular
	  * case. */
	 if (live->useCount > 1)
	     {
	     if (condLive->useCount > 1)
	         condLive->usePos[1] = min(condLive->usePos[1],live->usePos[1]);
	     else
	         condLive->usePos[1] = live->usePos[1];
	     }
	 condLive->usePos[0] = min(condLive->usePos[0], live->usePos[0]);
	 condLive->useCount = max(condLive->useCount, live->useCount);
	 }
    }
}


struct loopStack
/* A stack of loops */
    {
    struct loopStack *next;	/* Next in stack. */
    struct isxLiveVar *liveList;	/* Live list at this node. */
    struct dlNode *node;	/* Node where we pushed liveList */
    int loopCount;		/* How many times we've gone through loop. */
    struct isxLoopInfo *loopy;  /* Additional loop info */
    };

static void pushLoop(struct loopStack **pStack, struct isxLiveVar *liveList,
	struct dlNode *node, struct isxLoopInfo *loopy)
/* Alloc and push conditional stack entry */
{
struct loopStack *loop;
AllocVar(loop);
loop->liveList = liveList;
loop->node = node;
loop->loopy = loopy;
slAddHead(pStack, loop);
}

static void popLoop(struct loopStack **pStack)
/* Pop and free loop stack entry */
{
struct loopStack *loop = *pStack;
assert(loop != NULL);
*pStack = loop->next;
freeMem(loop);
}


static struct isxAddress *loopyAddress(struct isxLoopInfo *loopy)
/* Create reference to a build-in operator call */
{
struct isxAddress *iad;
AllocVar(iad);
iad->adType = iadLoopInfo;
iad->valType = ivJump;
iad->val.loopy = loopy;
return iad;
}

static struct isxLoopInfo *getLoopyAtEnd(struct dlNode *node, struct isx *isx,
	struct loopStack *parent)
/* Get loopInfo.  Create it and hang it on isx->left if it doesn't already
 * exist */
{
struct isxAddress *iad = isx->left;
struct isxLoopInfo *loopy;
if (iad == NULL)
    {
    AllocVar(loopy);
    loopy->end = node;
    isx->left = loopy->iad = loopyAddress(loopy);
    if (parent != NULL)
        {
	slAddTail(&parent->loopy->children, loopy);
	}
    }
else
    {
    loopy = iad->val.loopy;
    }
return loopy;
}

static void setLoopyAtStart(struct isxLoopInfo *loopy, struct dlNode *node)
/* Set loopy start, and count number of instructions. */
{
if (loopy->start == NULL)
    {
    int count = 1;
    struct isx *isx = node->val;
    isx->left = loopy->iad;
    loopy->start = node;
    for ( ; node != loopy->end; node=node->next)
	count += 1;
    loopy->instructionCount = count;
    }
loopy->iteration += 1;
}

void isxLiveList(struct dlList *iList)
/* Create list of live variables at each instruction by scanning
 * backwards. Handles loops and conditionals appropriately. */
{
struct dlNode *node;
struct isxLiveVar *liveList = NULL;
struct condStack *condStack = NULL, *cond;
struct loopStack *loopStack = NULL, *loop;
for (node = iList->tail; !dlStart(node); node = node->prev)
    {
    struct isxLiveVar *newList = NULL, *live;
    struct isx *isx = node->val;
    struct isxAddress *iad;
    struct isxLoopInfo *loopy;

    /* Save away current live list. */
    isx->liveList = liveList;
    switch (isx->opType)
        {
	case poLoopEnd:
	    loopy = getLoopyAtEnd(node, isx, loopStack);
	    pushLoop(&loopStack, liveList, node, loopy);
	    break;
	case poCondEnd:
	    pushCond(&condStack, liveList, node);
	    break;
	case poCondCase:
	    foldInCaseLive(condStack, liveList);
	    liveList = condStack->liveList;
	    break;
	case poCondStart:
	    liveList = condStack->condLiveList;
	    condStack->condLiveList = NULL;
	    popCond(&condStack);
	    break;
	case poLoopStart:
	    setLoopyAtStart(loopStack->loopy, node);
	    if (loopStack->loopCount > 0)
	        popLoop(&loopStack);
	    else
	        {
		loopStack->loopCount = 1;
		node = loopStack->node;
		}
	    break;
	}

    /* Make copy of live list minus any overwritten dests. */
    for (live = liveList; live != NULL; live = live->next)
	{
	struct isxAddress *iad = live->var;
	if (iad != isx->dest)
	    {
	    struct isxLiveVar *newLive = CloneVar(live);
	    newLive->usePos[0] += 1;
	    newLive->usePos[1] += 1;
	    slAddHead(&newList, newLive);
	    }
	}
    slReverse(&newList);

    /* Add sources to live list */
    iad = isx->left;
    if (iad != NULL && (iad->adType == iadRealVar || iad->adType == iadTempVar))
	newList = isxLiveVarAdd(newList, iad, 1);
    iad = isx->right;
    if (iad != NULL && (iad->adType == iadRealVar || iad->adType == iadTempVar))
	newList = isxLiveVarAdd(newList, iad, 1);

    /* Flip to new live list */
    liveList = newList;
    }
assert(loopStack == NULL);
assert(condStack == NULL);
isxLiveVarFreeList(&liveList);
}


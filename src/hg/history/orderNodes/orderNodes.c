#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: orderNodes.c,v 1.7 2006/02/17 01:13:42 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orderNodes - orders the ancestor nodes in  an element tree\n"
  "usage:\n"
  "   orderNodes elementTreeFile outGroupElems outTree\n"
  "arguments:\n"
  "   elementTreeFile      name of file containing element tree\n"
  "   outGroupElems        the element list of the outgroup\n"
  "   outLen               the length of the branch to the out group\n"
  "   outTree              name of file containing ordered element tree\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


struct possibleEdge *findEdge(struct possibleEdge *list, struct element *e, boolean doNeg)
{
for(; list; list = list->next)
    {
    if ((list->element == e) && (list->doFlip == doNeg))
	break;
    //if (sameString(eleName(e), eleName(list->element)))
	//errAbort("same string but not same element");
    }

return list;
}

void addPair( struct possibleEdge **list, struct element *e2,   boolean doNeg, int count)
{
struct possibleEdge *edge;
char *name;

if ((edge = findEdge(*list, e2, doNeg)) == NULL)
    {
    //printf("adding edge from -- to %s flip %d\n",eleName(e2), doNeg);
    AllocVar(edge);
    edge->count = count;
    edge->element = e2;
    edge->doFlip = doNeg;
    slAddHead(list, edge);
    }
else
    {
    edge->count += count;
    }
}

void addPairProb( struct possibleEdge **list, struct element *e2,   boolean doNeg, int count, int totalCount)
{
struct possibleEdge *edge;
char *name;

if ((edge = findEdge(*list, e2, doNeg)) == NULL)
    {
    //printf("adding edge from -- to %s flip %d\n",eleName(e2), doNeg);
    AllocVar(edge);
    edge->count = (double)count * 65535 / totalCount;
    edge->element = e2;
    edge->doFlip = doNeg;
    slAddHead(list, edge);
    }
else
    {
    printf("edge count was %d ",edge->count);
    edge->count = (((double)count * 65535 / totalCount ) + edge->count)/2;
    printf("now %d\n",edge->count);
    //errAbort("shouldn't find edge in addPairProb");
    }
if (edge->count < 0)
    errAbort("edge count less than 0");
}

char *eleFullName2(struct element *e, boolean doNeg)
{
 static char buffer[512];

if ( doNeg)
    //safef(buffer,sizeof buffer, "-%s.%s.%s",e->species,e->name, e->version);
    safef(buffer,sizeof buffer, "-%s.%s",e->name,e->version);
else
    safef(buffer,sizeof buffer, "%s.%s",e->name,e->version);
return buffer;
}

void setPair( struct element *e1, struct element *e2,  boolean doNeg)
{
struct possibleEdge *edge = NULL;
char *name;
boolean flipE1 = e1->isFlipped ^ doNeg;
boolean flipE2 = e2->isFlipped ^ doNeg;
struct possibleEdge **list;

list = &e1->up.next;
if (flipE1)
    list = &e1->up.prev;

if ((edge = findEdge(*list, e1, flipE1)) == NULL)
    {
    //printf("setting edge from %s to %s flip %d\n",eleName(e1),eleName(e2), flipE2);
    AllocVar(edge);
    edge->count = 1;
    edge->element = e2;
    edge->doFlip = flipE2;
    slAddHead(list, edge);
    }
else
    {
    errAbort("setting pair twice");
    }
}



void setAllPairs(struct genome *g)
{
struct element *e;
struct hash *h;

for(e = g->elements; e->next; e = e->next)
    {
    setPair(e, e->next, FALSE);
    setPair(e->next, e, TRUE);
    }
}

void setLeafNodePairs(struct phyloTree *node )
{
if (node->numEdges)
    {
    int ii;

    for(ii=0; ii < node->numEdges; ii++)
	setLeafNodePairs(node->edges[ii]);
    }
else
    {
    struct genome *g = node->priv;

    setAllPairs(g);
    }
}


void doOwn(struct possibleEdge *p, struct possibleEdge **outList, boolean doFlip, struct element *e)
{
//struct element *nextParent = e->parent;
//if (nextParent == NULL)
    //errAbort("next in hash doesn't have parent");

if ((findEdge(*outList, p->element, p->doFlip)) != NULL)
    errAbort("in doOwn , already on list");
if (e == p->element)
    errAbort("adding edge to itself");
if (*outList != NULL)
    errAbort("own edge should be alone");
addPair(outList, p->element, p->doFlip, p->count);
}

void doUpList(struct possibleEdge *inList, struct possibleEdge **outList, struct element *e)
{
struct possibleEdge *p;

for(p = inList; p ; p = p->next)
    {
    struct element *nextParent = p->element->parent;

    if (nextParent == NULL)
	errAbort("next in hash doesn't have parent");

    if ((findEdge(*outList, p->element, p->doFlip)) != NULL)
	errAbort("in doUp , already on list");
    if (e == p->element->parent)
	continue;
    //errAbort("n doUpadding edge to itself");
//if (*outList != NULL)
    //errAbort("in doUp edge should be alone");

    addPair(outList, nextParent, p->doFlip, p->count);
    }
}

int getTotalCount(struct possibleEdge **inList)
{
struct possibleEdge *p;
int totalCount = 0;
struct possibleEdge *edge;
struct possibleEdge *prev;

prev = NULL;
for(p = *inList; p ; p = p->next)
    totalCount += p->count;

return totalCount;
}

void purgeList(struct possibleEdge **inList)
{
struct possibleEdge *p;
struct possibleEdge *edge;
struct possibleEdge *prev;

prev = NULL;
for(p = *inList; p ; p = p->next)
    {
    if (p->doFlip)
	edge = p->element->calced.next;
    else
	edge = p->element->calced.prev;

    if (edge == NULL)
	{
	prev = p;
	}
    else 
	{
	if (prev == NULL)
	    *inList = p->next;
	else
	    prev->next = p->next;
	}
    }
}

void doUpProbs(struct possibleEdge *inList, struct possibleEdge **outList)
{
struct possibleEdge *p;
int totalCount = 0;
struct possibleEdge *edge;

printf("in doUpProb before check\n");
totalCount = getTotalCount(&inList);
if (totalCount == 0)
    return;

printf("in doUpProbs\n");
for(p = inList; p ; p = p->next)
    {
    struct element *nextParent = p->element->parent;

    if (nextParent == NULL)
	errAbort("next in hash doesn't have parent");

    printf("adding %s\n",eleName(nextParent));
    addPairProb(outList, nextParent, p->doFlip, p->count, totalCount);
    }

/*
totalCount = getTotalCount(*outList);
for(p = *outList; p ; p = p->next)
    if (totalCount)
	p->count = (double)p->count * 65535/ totalCount;
    else
	p->count = 0;
	*/

}

void doDownList(struct possibleEdge *inList, struct possibleEdge **outList,  int childNum, struct element *e)
{

struct possibleEdge *p;

for(p = inList; p ; p = p->next)
    {
    struct possibleEdge *edge = p;
    struct element *ne = edge->element;
    struct element *nextChild = NULL;

    if (ne->numEdges ==1)
	{
	if (sameString(edge->element->edges[0]->species, e->species))
	    nextChild = edge->element->edges[0];
	else
	    return ;
	    //errAbort("single child isn't right");
	}
    else
	{
	if (sameString(edge->element->edges[0]->species, e->species))
	    nextChild = edge->element->edges[0];
	else if (sameString(edge->element->edges[1]->species, e->species))
	    nextChild = edge->element->edges[1];
	else
	    errAbort("neither child is right\n");
	}

    if (nextChild == NULL)
	errAbort("next in hash doesn't have child");

    //printf("looking at %s\n",eleName(edge->element));
    //printf("child is %s\n",eleName(edge->element->edges[childNum]));

    if (!sameString(e->species, nextChild->species))
	errAbort("not same species");
    //printf("setting edge from %s to %s w %g p %g\n",eleName(e), eleName(nextChild), weight, edge->count);
    addPair(outList, nextChild, p->doFlip, p->count);
    //addPair(*setHash, nextChild, edge->prob * weight , *next->name == '-');
    }
}

void doDownProbs(struct possibleEdge *inList, struct possibleEdge **outList,  int childNum, struct element *e)
{
int totalCount = 0;
struct possibleEdge *p;

totalCount = getTotalCount(&inList);
if (totalCount == 0)
    return;

for(p = inList; p ; p = p->next)
    {
    struct possibleEdge *edge = p;
    struct element *ne = edge->element;
    struct element *nextChild = NULL;

    if (ne->numEdges ==1)
	{
	if (sameString(edge->element->edges[0]->species, e->species))
	    nextChild = edge->element->edges[0];
	else
	    return ;
	    //errAbort("single child isn't right");
	}
    else
	{
	if (sameString(edge->element->edges[0]->species, e->species))
	    nextChild = edge->element->edges[0];
	else if (sameString(edge->element->edges[1]->species, e->species))
	    nextChild = edge->element->edges[1];
	else
	    errAbort("neither child is right\n");
	}

    if (nextChild == NULL)
	errAbort("next in hash doesn't have child");

    //printf("looking at %s\n",eleName(edge->element));
    //printf("child is %s\n",eleName(edge->element->edges[childNum]));

    if (!sameString(e->species, nextChild->species))
	errAbort("not same species");
    //printf("setting edge from %s to %s w %g p %g\n",eleName(e), eleName(nextChild), weight, edge->count);
    //addPair(outList, nextChild, p->doFlip, p->count);
    addPairProb(outList, nextChild, p->doFlip, p->count, totalCount);
    //addPair(*setHash, nextChild, edge->prob * weight , *next->name == '-');
    }

/*
totalCount = getTotalCount(*outList);
for(p = *outList; p ; p = p->next)
    if (totalCount)
	p->count = (double)p->count * 65535/ totalCount;
    else
	p->count = 0;
	*/
}

void calcUpNodes(struct phyloTree *node)
{
struct genome *g = node->priv;
int ii;
//double totalLen = 0.0;

//for(ii=0; ii < node->numEdges; ii++)
 //   totalLen += node->edges[ii]->ident->length;

for(ii=0; ii < node->numEdges; ii++)
    //calcUpNodes(node->edges[ii], (totalLen - node->edges[ii]->ident->length)/totalLen);
    calcUpNodes(node->edges[ii]);
    //calcUpNodes(node->edges[ii], 1.0 / node->numEdges);

if (node->numEdges)
    {
    struct element *e;

    for(e=g->elements; e; e = e->next)
	{
	struct element *parent = e->parent;
	//double useWeight = weight;

	//if (node->parent->numEdges == 1)
	    //useWeight /= parent->numEdges;

	if (e->numEdges == 1)
	    {
	    if (e->calced.next == NULL)
		doUpList(e->edges[0]->up.next, &e->up.next, e);
	    else
		doOwn(e->calced.next, &e->up.next, FALSE, e);

	    if (e->calced.prev == NULL)
		doUpList(e->edges[0]->up.prev, &e->up.prev, e);
	    else
		doOwn(e->calced.prev, &e->up.prev, TRUE, e);
	    }
	else
	    {
	    if (e->calced.next == NULL)
		{
		doUpList(e->edges[0]->up.next, &e->up.next, e);
		doUpList(e->edges[1]->up.next, &e->up.next, e);
		}
	    else
		doOwn(e->calced.next, &e->up.next, FALSE, e);
	    if (e->calced.prev == NULL)
		{
		doUpList(e->edges[0]->up.prev, &e->up.prev, e);
		doUpList(e->edges[1]->up.prev, &e->up.prev, e);
		}
	    else
		doOwn(e->calced.prev, &e->up.prev, TRUE, e);
	    }
	//doUpHash(e->up.nextHash, &parent->up.nextHash, useWeight);
	//doUpHash(e->up.prevHash, &parent->up.prevHash, useWeight);
	}
    }
}

void calcDownNodes(struct phyloTree *node)
{
struct genome *g = node->priv;
int ii;
//double totalLen = 0.0;

if (node->parent == NULL)
    {
    struct element *e;

    for(e=g->elements; e; e = e->next)
	{
	//printf("topnode working on %s\n",eleName(e));
	if (e->numEdges == 1)
	    {
	    if (e->calced.prev == NULL)
		doUpList(e->edges[0]->up.prev, &e->down[0].prev, e);
	    else
		doOwn(e->calced.prev, &e->down[0].prev, TRUE, e);
	    if (e->calced.next == NULL)
		doUpList(e->edges[0]->up.next, &e->down[0].next, e);
	    else
		doOwn(e->calced.next, &e->down[0].next, FALSE, e);
	    }
	else
	    {
	    if (e->calced.prev == NULL)
		{
		doUpList(e->edges[1]->up.prev, &e->down[0].prev, e);
		doUpList(e->edges[0]->up.prev, &e->down[1].prev, e);
		}
	    else
		{
		doOwn(e->calced.prev, &e->down[0].prev, TRUE, e);
		doOwn(e->calced.prev, &e->down[1].prev, TRUE, e);
		}

	    if (e->calced.next == NULL)
		{
		doUpList(e->edges[1]->up.next, &e->down[0].next, e);
		doUpList(e->edges[0]->up.next, &e->down[1].next, e);
		}
	    else
		{
		doOwn(e->calced.next, &e->down[0].next, FALSE, e);
		doOwn(e->calced.next, &e->down[1].next, FALSE, e);
		}
	    }
	}
    }
else if (node->numEdges)
    {
    struct element *e;

    for(e=g->elements; e; e = e->next)
	{
	struct element *parent = e->parent;
	//double useUpWeight = weight;
	//double useDownWeight = weight;
	int num;

	//if (node->parent->numEdges == 1)
	//    useDownWeight /= parent->numEdges;
	//if (node->numEdges == 1)
	//    useUpWeight /= e->numEdges;

	//printf("working on %s\n",eleName(e));
	num = 0;
	if (e == parent->edges[1])
	    num = 1;

	if (e->numEdges == 1)
	    {
	    if (e->calced.prev == NULL)
		doDownList(parent->down[num].prev, &e->down[0].prev, num, e);
	    else
		doOwn(e->calced.prev, &e->down[0].prev, TRUE, e);
	    if (e->calced.next == NULL)
		doDownList(parent->down[num].next, &e->down[0].next, num, e);
	    else
		doOwn(e->calced.next, &e->down[0].next, FALSE, e);
	    }
	else
	    {
	    if (e->calced.prev == NULL)
		{
		doUpList(e->edges[0]->up.prev,&e->down[1].prev, e);
		doDownList(parent->down[num].prev, &e->down[1].prev, num, e);
	    
		doUpList(e->edges[1]->up.prev,&e->down[0].prev, e);
		doDownList(parent->down[num].prev,&e->down[0].prev, num, e);
		}
	    else
		{
		doOwn(e->calced.prev, &e->down[0].prev, TRUE, e);
		doOwn(e->calced.prev, &e->down[1].prev, TRUE, e);
		}

	    if (e->calced.next == NULL)
		{
		doUpList(e->edges[0]->up.next,&e->down[1].next, e);
		doDownList(parent->down[num].next, &e->down[1].next, num, e);
	    
		doUpList(e->edges[1]->up.next,&e->down[0].next, e);
		doDownList(parent->down[num].next,&e->down[0].next, num, e);
		}
	    else
		{
		doOwn(e->calced.next, &e->down[0].next, FALSE, e);
		doOwn(e->calced.next, &e->down[1].next, FALSE, e);
		}
	    }
	}
    } 
for(ii=0; ii < node->numEdges; ii++)
    {
    calcDownNodes(node->edges[ii]);
    }
//for(ii=0; ii < node->numEdges; ii++)
    //totalLen += node->edges[ii]->ident->length;

}

void chooseMedian3(struct possibleEdge *left, struct possibleEdge *right, struct possibleEdge *top, struct possibleEdge **median) 
{
struct possibleEdge *p;
struct possibleEdge *edge1, *edge2;
struct possibleEdge *newEdge;

if (*median != NULL)
    errAbort("should be null list");

for(p = left; p ; p = p->next)
    {
    AllocVar(newEdge);
    //printf("adding %s\n",eleName(p->element));
    newEdge->element = p->element;
    newEdge->doFlip = p->doFlip;
    newEdge->count = p->count;
    slAddHead(median, newEdge);

    //printf("looking for %s with %d\n",eleName(p->element), p->count);
    if ((edge1 = findEdge(right, p->element, p->doFlip)) != NULL)
	{
    //printf("lfound %s with %d\n",eleName(p->element), edge1->count);
	edge1->element = NULL;
	
	if (edge1->count > p->count)
	    newEdge->count = edge1->count;
    //printf("chose %s with %d\n",eleName(p->element), newEdge->count);

	}
    if ((edge2 = findEdge(top, p->element, p->doFlip)) != NULL)
	{
	//printf("found on top %s with %d\n",eleName(edge2->element),edge2->count);
	edge2->element = NULL;
	if (edge2->count > newEdge->count)
	    newEdge->count = edge2->count;
	//if ((edge2->count < newEdge->count) && (((edge1 == NULL) ||  (edge2->count > edge1->count)) ))
	 //   newEdge->count = edge2->count;
    //printf("chose %s with %d\n",eleName(p->element), newEdge->count);
	}
    }

for(p = right; p ; p = p->next)
    {
    if (p->element != NULL)
	{
    //printf("at %s\n",eleName(p->element));
	if ((edge1 = findEdge(*median, p->element, p->doFlip)) != NULL)
	    errAbort("already on list");
	AllocVar(newEdge);
	newEdge->element = p->element;
	newEdge->doFlip = p->doFlip;
	newEdge->count = p->count;
	slAddHead(median, newEdge);

	if ((edge2 = findEdge(top, p->element, p->doFlip)) != NULL)
	    {
	    //printf("atfound on top %s\n",eleName(edge2->element));
	    edge2->element = NULL;
	    if (edge2->count > newEdge->count)
		newEdge->count = edge2->count;
	    }

	}
    }

for(p = top; p ; p = p->next)
    {
    if (p->element != NULL)
	{
    //printf("at %s\n",eleName(p->element));
    if ((edge1 = findEdge(*median, p->element, p->doFlip)) != NULL)
	errAbort("aatlready on list");
	AllocVar(newEdge);
	newEdge->element = p->element;
	newEdge->doFlip = p->doFlip;
	newEdge->count = p->count;
	slAddHead(median, newEdge);
	}
    }
}

void chooseMedian2(struct possibleEdge *left, struct possibleEdge *right, struct possibleEdge **median) 
{
struct possibleEdge *p;
struct possibleEdge *edge;
struct possibleEdge *newEdge;

for(p = left; p ; p = p->next)
    {
    if (( findEdge(*median, p->element, p->doFlip)) != NULL)
	errAbort("already on list");
    AllocVar(newEdge);
    newEdge->element = p->element;
    newEdge->doFlip = p->doFlip;
    newEdge->count = p->count;
    slAddHead(median, newEdge);

    if ((edge = findEdge(right, p->element, p->doFlip)) != NULL)
	{
    //printf("pfound %s with %d\n",eleName(p->element), edge->count);
	edge->element = NULL;
	
	if (edge->count > p->count)
	    newEdge->count = edge->count;
	}
    }
for(p = right; p ; p = p->next)
    {
    if (p->element != NULL)
	{
	struct possibleEdge *edge1;
    if ((edge1 = findEdge(*median, p->element, p->doFlip)) != NULL)
	errAbort("already on list");
	AllocVar(newEdge);
	newEdge->element = p->element;
	newEdge->doFlip = p->doFlip;
	newEdge->count = p->count;
	slAddHead(median, newEdge);
	}
    }
}

void calcMix(struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e;
int ii;
int totalCount;
struct possibleEdge *p = NULL;
//double totalLen = 0.0;
//struct possibleEdge *leftListPrev = NULL;
//struct possibleEdge *topListNext = NULL;
//struct possibleEdge *rightListNext = NULL;
//struct possibleEdge *leftListNext = NULL;

if (node->numEdges == 0)
    return;

if (node->parent == NULL)
    {
    for(e=g->elements; e; e = e->next)
	{
	//printf("topnode working on %s\n",eleName(e));
	if (e->numEdges == 1)
	    {
	    if (e->edges[0]->calced.next)
		doUpProbs(e->edges[0]->calced.next, &e->mix.next);
	    else
		doUpProbs(e->edges[0]->up.next, &e->mix.next);

	    if (e->edges[0]->calced.prev)
		doUpProbs(e->edges[0]->calced.prev, &e->mix.prev);
	    else
		doUpProbs(e->edges[0]->up.prev, &e->mix.prev);
	    }
	else
	    {
	    struct possibleEdge *rightListPrev = NULL;
	    struct possibleEdge *leftListPrev = NULL;
	    struct possibleEdge *rightListNext = NULL;
	    struct possibleEdge *leftListNext = NULL;

	    printf("at topnode in calcMix\n");
	    if (e->edges[1]->calced.prev)
		doUpProbs(e->edges[1]->calced.prev, &rightListPrev);
	    else
		doUpProbs(e->edges[1]->up.prev, &rightListPrev);
	    if (e->edges[0]->calced.prev)
		doUpProbs(e->edges[0]->calced.prev, &leftListPrev);
	    else
		doUpProbs(e->edges[0]->up.prev, &leftListPrev);
	    chooseMedian2(leftListPrev, rightListPrev,  &e->mix.prev);
	    if (( rightListPrev || leftListPrev) && (e->mix.prev == NULL))
		errAbort("no mix prev");

	    if (e->edges[1]->calced.next)
		doUpProbs(e->edges[1]->calced.next, &rightListNext);
	    else
		doUpProbs(e->edges[1]->up.next, &rightListNext);
	    if (e->edges[0]->calced.next)
		doUpProbs(e->edges[0]->calced.next, &leftListNext);
	    else
		doUpProbs(e->edges[0]->up.next, &leftListNext);
	    if (!( rightListNext || leftListNext))
		printf("right and left next 0\n");
	    chooseMedian2(leftListNext, rightListNext, &e->mix.next);
	    if (( rightListNext || leftListNext) && (e->mix.next == NULL))
		errAbort("no mix next");
	    printf("out topnode in calcMix\n");
	    }
	}
    }
else if (node->numEdges)
    {

    for(e=g->elements; e; e = e->next)
	{
	struct element *parent = e->parent;
	int num;
	struct possibleEdge *topListPrev = NULL;
	struct possibleEdge *rightListPrev = NULL;
	struct possibleEdge *leftListPrev = NULL;
	struct possibleEdge *topListNext = NULL;
	struct possibleEdge *rightListNext = NULL;
	struct possibleEdge *leftListNext = NULL;

	num = 0;
	if (e == parent->edges[1])
	    num = 1;

	if (e->numEdges == 1)
	    {
	    if (parent->calced.prev)
		doDownProbs(parent->calced.prev, &topListPrev, num, e);
	    else
		doDownProbs(parent->down[num].prev, &topListPrev, num, e);
	    if (e->edges[0]->calced.prev)
		doUpProbs(e->edges[0]->calced.prev, &rightListPrev);
	    else
		doUpProbs(e->edges[0]->up.prev, &rightListPrev);
	    chooseMedian2(topListPrev, rightListPrev,  &e->mix.prev);

	    doDownProbs(parent->down[num].next, &topListNext, num, e);
	    doUpProbs(e->edges[0]->up.next, &rightListNext);
	    chooseMedian2(topListNext, rightListNext,  &e->mix.next);

	    //doDownList(parent->down[num].prev, &e->mix.prev, num, e);
	    //doUpList(e->edges[0]->up.prev, &e->mix.prev);
	    //doDownList(parent->down[num].next, &e->mix.next, num, e);
	    //doUpList(e->edges[0]->up.next, &e->mix.next);
	    }
	else
	    {
	    if (e->edges[0]->calced.prev)
		doUpProbs(e->edges[0]->calced.prev,&rightListPrev);
	    else
		doUpProbs(e->edges[0]->up.prev,&rightListPrev);

	    if (parent->calced.prev)
		doDownProbs(parent->calced.prev, &topListPrev, num, e);
	    else
		doDownProbs(parent->down[num].prev, &topListPrev, num, e);

	    if (e->edges[1]->calced.prev)
		doUpProbs(e->edges[1]->calced.prev,&leftListPrev);
	    else
		doUpProbs(e->edges[1]->up.prev,&leftListPrev);

	    chooseMedian3(topListPrev, rightListPrev, leftListPrev,  &e->mix.prev);
	    if ((topListPrev || rightListPrev || leftListPrev) && (e->mix.prev == NULL))
		errAbort("no mix prev");

	    /*
	    doUpProbs(e->edges[0]->up.next,&rightListNext);
	    doDownProbs(parent->down[num].next, &topListNext, num, e);
	    doUpProbs(e->edges[1]->up.next,&leftListNext);
	    */
	    if (e->edges[0]->calced.next)
		{
		printf("edge 0 is calced\n");
		doUpProbs(e->edges[0]->calced.next,&rightListNext);
		}
	    else
		doUpProbs(e->edges[0]->up.next,&rightListNext);

	    if (parent->calced.next)
		doDownProbs(parent->calced.next, &topListNext, num, e);
	    else
		doDownProbs(parent->down[num].next, &topListNext, num, e);

	    if (e->edges[1]->calced.next)
		doUpProbs(e->edges[1]->calced.next,&leftListNext);
	    else
		doUpProbs(e->edges[1]->up.next,&leftListNext);
	    chooseMedian3(topListNext, rightListNext, leftListNext,  &e->mix.next);

	    if ((topListNext || rightListNext || leftListNext) && (e->mix.next == NULL))
		errAbort("no mix next");

	    //doUpList(e->edges[0]->up.prev,&e->mix.prev);
	    //doDownList(parent->down[num].prev, &e->mix.prev, num, e);
	    //doUpList(e->edges[1]->up.prev,&e->mix.prev);
	    //doUpList(e->edges[0]->up.next,&e->mix.next);
	    //doDownList(parent->down[num].next, &e->mix.next, num, e);
	    //doUpList(e->edges[1]->up.next,&e->mix.next);
	    }
	}
    } 

for(e=g->elements; e; e = e->next)
    {
    totalCount = getTotalCount(&e->mix.prev);
//    printf("neg total %d element %s\n",totalCount,eleName(e));
    for(p = e->mix.prev; p ; p = p->next)
	{
	if (totalCount)
	    p->count = (double)p->count * 65535/ totalCount;
	else
	    p->count = 0;
//	printf("count %d\n",p->count);
	if (p->count > 65535)
	    errAbort("too big");
	}


    totalCount = getTotalCount(&e->mix.next);
 //   printf("pos total %d element %s\n",totalCount,eleName(e));
    for(p = e->mix.next; p ; p = p->next)
	{
	if (totalCount)
	    p->count = (double)p->count * 65535/ totalCount;
	else
	    p->count = 0;
//	printf("count %d\n",p->count);
	if (p->count > 65535)
	    errAbort("too big");
	}
    }


for(ii=0; ii < node->numEdges; ii++)
    {
    calcMix(node->edges[ii]);
    }
}

void printTransSub(FILE *f, struct adjacency *adj, char *name)
{
struct possibleEdge *p;
int totalCount;
struct element *prevE;
boolean prevFlip;

totalCount = 0;
for(p= adj->next ; p ; p = p->next)
    totalCount += p->count;

prevE = NULL;
prevFlip = -1;
for(p= adj->next ; p ; p = p->next)
    {
    if ((prevE == p->element) && (prevFlip == p->doFlip))
	errAbort("repeated element");
    prevE = p->element;
    prevFlip = p->doFlip;
    fprintf(f, "%s\t%s\t%d\t%g\n",name, eleFullName(p->element, p->doFlip ^ p->element->isFlipped), p->count, (double)p->count/totalCount);
    }

totalCount = 0;
for(p= adj->prev ; p ; p = p->next)
    totalCount += p->count;
prevE = NULL;
prevFlip = -1;
for(p= adj->prev ; p ; p = p->next)
    {
    if ((prevE == p->element) && (prevFlip == p->doFlip))
	errAbort("repeated element");
    prevE = p->element;
    prevFlip = p->doFlip;
    fprintf(f, "-%s\t%s\t%d\t%g\n",name, eleFullName(p->element, p->doFlip ^ p->element->isFlipped), p->count, (double)p->count/totalCount);
    }

}

void printTrans(FILE *f, struct genome *g, char *which)
{
struct element *e = g->elements;
char name[512];

fprintf(f,which);
fprintf(f, ">%s\n",g->name);

for(; e ; e = e->next)
    {
    safef(name, sizeof name, "%s", eleFullName2(e, FALSE));

    if (sameString(which, "up"))
	printTransSub(f, &e->up, name);
    else if (sameString(which, "down0"))
	{
	printTransSub(f, &e->down[0], name);
	}
    else if (sameString(which, "down1"))
	{
	printTransSub(f, &e->down[1], name);
	}
    else if (sameString(which, "mix"))
	{
	printTransSub(f, &e->mix, name);
	}
    }
}

void printNodes(FILE *f, struct phyloTree *node, char *which)
{
struct genome *g = node->priv;
int ii;

for(ii=0; ii < node->numEdges; ii++)
    printNodes(f, node->edges[ii], which);

printTrans(f, g, which);
}

void printOrderedNode(FILE *f, struct genome *g)
{
struct element *e, *saveE;
boolean inFront;

for(e=g->elements; e; e = e->next)
    if (e->calced.prev == NULL)
	break;

if (e)
    {
    saveE = e;
    inFront = TRUE;

    for(e = e->next; e; e = e->next)
	if (e->calced.prev == NULL)
	    errAbort("list isn't fully ordered");
    }

else
    {
    for(e=g->elements; e; e = e->next)
	if (e->calced.next == NULL)
	    break;

    saveE = e;
    inFront = FALSE;

    for(e = e->next; e; e = e->next)
	if (e->calced.prev == NULL)
	    errAbort("list isn't fully ordered");
    }


fprintf(f, ">%s\n",g->name);


g->elements = saveE;
saveE->next = NULL;
for(e = saveE ; e ; )
    {
    struct possibleEdge *p;

    if (!inFront)
	{
	fprintf(f, "-");
	e->isFlipped = TRUE;
	}
    fprintf(f, "%s ",eleName(e));
    if (inFront == TRUE)
	p = e->calced.next;
    else
	p = e->calced.prev;

    if (!p)
	break;

    inFront = !p->doFlip ; //? (inFront ? FALSE : TRUE) : inFront;
    e = p->element;
    e->next = NULL;
    slAddHead(&g->elements, e);
    }
slReverse(&g->elements);
fprintf(f,"\n");
}

void printOrderedTree(FILE *f, struct phyloTree *node)
{
int ii;
struct genome *g = node->priv;

if (node->numEdges)
    printOrderedNode(f, g);
else
    ;//outElems(f, node);

for(ii=0; ii < node->numEdges; ii++)
    printOrderedTree(f, node->edges[ii]);
}


void clearNodes(struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e;
int ii;

if (node->numEdges == 0)
    return ;

for(ii=0; ii < node->numEdges; ii++)
    clearNodes(node->edges[ii]);

for(e = g->elements; e; e = e->next)
    {
    e->up.next = e->up.prev = NULL;
    e->down[0].next = e->down[0].prev = NULL;
    e->down[1].next = e->down[1].prev = NULL;
    e->mix.next = e->mix.prev = NULL;
    }
}

void findBestEdge(struct phyloTree *node, struct possibleEdge **pEdge, int *bestYet, boolean *neg, struct element **pEvent)
{
struct possibleEdge *edge = NULL;
int ii;
struct genome *g = node->priv;
struct element *e;

if (node->numEdges == 0)
    return;

for(ii=0; ii < node->numEdges; ii++)
    findBestEdge(node->edges[ii], pEdge, bestYet, neg, pEvent);

//if (node->parent == 0)
    //printf("findBestEdge at root\n");
for(e=g->elements; e; e = e->next)
    {
//if (node->parent == 0)
    //printf("looking at element %s\n",eleName(e));
    if ((e->calced.prev == NULL) && (e->mix.prev ))
    {
    purgeList(&e->mix.prev);
    for(edge = e->mix.prev; edge; edge = edge->next)
	{
//if (node->parent == 0)
    //printf("  elem %s count %d bestYest %d\n",eleName(edge->element),edge->count,*bestYet);

	if (edge->count > *bestYet)
	    {
	    *bestYet = edge->count;
	    *pEdge = edge;
	    *pEvent = e;
	    *neg = TRUE;
	    }
	}
    }
    if ((e->calced.next == NULL) && (e->mix.next))
    {
    purgeList(&e->mix.next);
    for(edge = e->mix.next; edge; edge = edge->next)
	{
//if (node->parent == 0)
    //printf("  +count %d bestYest %d\n",edge->count,*bestYet);
	if (edge->count > *bestYet)
	    {
	    *bestYet = edge->count;
	    *pEdge = edge;
	    *pEvent = e;
	    *neg = FALSE;
	    }
	}
    }
    }
}


void setEdge(struct element *e, struct possibleEdge *edge, boolean doFlip, int count)
{
struct possibleEdge **list;
struct possibleEdge *newEdge;

printf("setting edge %d\n",count);
printf("%c%s %c%s\n", doFlip ? '-' : ' ',eleName(e), edge->doFlip ? '-' : ' ',eleName(edge->element));
//if (e->parent == NULL)
    //printf("setting edge at root\n");
edge->count = count;
edge->next = 0;
if (doFlip)
    {
    if (e->calced.prev != NULL)
	errAbort("calced edge already set");
    e->calced.prev = edge;
    }
else 
    {
    if (e->calced.next != NULL)
	errAbort("calced edge already set");
    e->calced.next = edge;
    }

AllocVar(newEdge);
newEdge->element = e;
newEdge->count = count;
newEdge->doFlip = doFlip ? FALSE : TRUE ;
if (!edge->doFlip)
    {
    if (edge->element->calced.prev != NULL)
	errAbort("calced edge already set");
printf("-%s %c%s\n",eleName(edge->element), newEdge->doFlip ? '-' : ' ',eleName(newEdge->element));
    edge->element->calced.prev = newEdge;
    }
else 
    {
    if (edge->element->calced.next != NULL)
	errAbort("calced edge already set");
printf("%s %c%s\n",eleName(edge->element), newEdge->doFlip ? '-' : ' ',eleName(newEdge->element));
    edge->element->calced.next = newEdge;
    }
}

void orderNodes(char *treeFile, char *outFile)
{
struct phyloTree *node = eleReadTree(treeFile);
FILE *f = mustOpen(outFile, "w");
struct possibleEdge *edge;
struct element *bestElement;
int best;
int totalCount;
boolean doFlip;
//double branchLen = atof(lenString);
//struct genome *g = getGenome(outGroup, outGroup);

setLeafNodePairs(node);
//setAllPairs(g);

for(;;)
    {
    clearNodes(node);
    calcUpNodes(node);
    printNodes(f, node, "up");
    calcDownNodes(node);
    printNodes(f, node, "down0");
    printNodes(f, node, "down1");
    calcMix(node);

    printNodes(f, node, "mix");

    edge = NULL;
    best = 0;
    findBestEdge(node, &edge, &best, &doFlip, &bestElement);
    if (edge == NULL)
	{
	printf("breaqking\n");
	break;
	}

    if (doFlip)
	totalCount = getTotalCount( &bestElement->up.prev);
    else
	totalCount = getTotalCount( &bestElement->up.next);

    printf("best adjacency %c%s to %c%s count %d\n",doFlip ? '-' : ' ', 
	eleName(bestElement),edge->doFlip ? '-' : ' ',eleName(edge->element),best);
    setEdge(bestElement, edge, doFlip, totalCount);
    }

printOrderedTree(f, node);
outElementTrees(f, node);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

//verboseSetLevel(2);
orderNodes(argv[1], argv[2]);
return 0;
}

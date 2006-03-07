#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: orderNodes.c,v 1.12 2006/03/07 22:03:28 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orderNodes - orders the ancestor nodes in an element tree\n"
  "usage:\n"
  "   orderNodes elementTreeFile outTree\n"
  "arguments:\n"
  "   elementTreeFile      name of file containing element tree\n"
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
    //errAbort("found edge twice");
    edge->count += count;
    }
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
setPair(e, g->elements, FALSE);
setPair(g->elements, e, TRUE);
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
	{
	//errAbort("n doUpadding edge to itself");
	//continue;
	}

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

void doDownList(struct possibleEdge *inList, struct possibleEdge **outList,  int childNum, struct element *e)
{

struct possibleEdge *p;

for(p = inList; p ; p = p->next)
    {
    struct possibleEdge *edge = p;
    struct element *ne = edge->element;
    struct element *nextChild = NULL;

    if (ne->numEdges == 0)
	continue;

    if (ne->numEdges ==1)
	{
	if (sameString(edge->element->edges[0]->species, e->species))
	    nextChild = edge->element->edges[0];
	else
	    {
	    errAbort("single child isn't right");
	    return ;
	    }
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

    if (!sameString(e->species, nextChild->species))
	errAbort("not same species");
    addPair(outList, nextChild, p->doFlip, p->count);
    }
}


void calcUpNodes(struct phyloTree *node)
{
struct genome *g = node->priv;
int ii;

for(ii=0; ii < node->numEdges; ii++)
    calcUpNodes(node->edges[ii]);

if (node->numEdges)
    {
    struct element *e;

    for(e=g->elements; e; e = e->next)
	{
	struct element *parent = e->parent;

	if (e->numEdges == 0)
	    errAbort("element should have children");

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
	int num;

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
}

void chooseMedian3(struct possibleEdge *left, struct possibleEdge *right, struct possibleEdge *top, struct possibleEdge **median) 
{
struct possibleEdge *p;
struct possibleEdge *edge1, *edge2;
struct possibleEdge *newEdge;

if (*median != NULL)
    errAbort("should be null list");

if (!((slCount(left) == 1) ||  (slCount(right) == 1) || (slCount(top) ==1)))
    errAbort("breaks infinite sites 1 must be 1");
if (slCount(left) + slCount(right) + slCount(top) < 3)
    errAbort("breaks infinite sites < 3");
//if (slCount(left) + slCount(right) + slCount(top) > 4)
    //warn("breaks infinite sites > 4");

p = left;
if ((left->element != top->element) || (left->doFlip!= top->doFlip))
    {
    p = right;
    if (slCount(right) != 1)
	;//errAbort("fon");
	}
else
    if (slCount(left) != 1)
	;//errAbort("fon");


#ifdef NOWNOW
p = left;
if (slCount(left) == 2)
    {
    p=right;
    }
    /*
    if (slCount(right) == 2)
	{
	p=top;
	warn("choosing top");
	}
    }
    */

else if (slCount(top) == 1)
    if ((left->element != top->element) || (left->doFlip!= top->doFlip))
	{
	p = right;
	errAbort("here");
	}
#endif

/*
if ((p == right) && (slCount(right) != 1))
    if ((p->element != left->element) || (p->doFlip!= left->doFlip))
	{
	errAbort("here");
	p=right->next;
	}
	*/

/*
AllocVar(newEdge);
newEdge->element = p->element;
newEdge->doFlip = p->doFlip;
newEdge->count = p->count;
slAddHead(median, newEdge);
*/
slAddHead(median, p);
}

void chooseMedian2(struct possibleEdge *left, struct possibleEdge *right, struct possibleEdge **median) 
{
struct possibleEdge *p;
struct possibleEdge *edge;
struct possibleEdge *newEdge;

if (!((slCount(left) == 1) ||  (slCount(right) == 1)))
    errAbort("median2 breaks infinite sites 1 must be 1");
//if (slCount(right) + slCount(left) > 3)
    //warn("median2 infinites sites break > 3");
if (slCount(right) + slCount(left) < 2)
    errAbort("median2 infinites sites break < 2");
//if ((slCount(left) == 1) && (slCount(right) == 1) && (left->element != right->element))
    //warn("median2 : right != left");

p = left;
if (slCount(p) != 1)
    p = right;

AllocVar(newEdge);
newEdge->element = p->element;
newEdge->doFlip = p->doFlip;
newEdge->count = p->count;
slAddHead(median, newEdge);
}

void calcMix(struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e;
int ii;
int totalCount;
struct possibleEdge *p = NULL;

if (node->numEdges == 0)
    return;

if (node->parent == NULL)
    {
    for(e=g->elements; e; e = e->next)
	{
	if (e->numEdges == 1)
	    {
	    doUpList(e->edges[0]->up.next, &e->mix.next,e);
	    doUpList(e->edges[0]->up.prev, &e->mix.prev,e);
	    }
	else
	    {
	    struct possibleEdge *rightListPrev = NULL;
	    struct possibleEdge *leftListPrev = NULL;
	    struct possibleEdge *rightListNext = NULL;
	    struct possibleEdge *leftListNext = NULL;

	    doUpList(e->edges[0]->up.prev, &leftListPrev,e);
	    doUpList(e->edges[1]->up.prev, &rightListPrev,e);
	    chooseMedian2(leftListPrev, rightListPrev,  &e->mix.prev);

	    if (( rightListPrev || leftListPrev) && (e->mix.prev == NULL))
		errAbort("no mix prev");

	    doUpList(e->edges[0]->up.next, &leftListNext,e);
	    doUpList(e->edges[1]->up.next, &rightListNext,e);
	    chooseMedian2(leftListNext, rightListNext, &e->mix.next);

	    if (( rightListNext || leftListNext) && (e->mix.next == NULL))
		errAbort("no mix next");
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
	    doUpList(e->edges[0]->up.prev, &rightListPrev, e);
	    doDownList(parent->down[num].prev, &topListPrev, num, e);

	    chooseMedian2( rightListPrev,topListPrev,  &e->mix.prev);

	    doUpList(e->edges[0]->up.next, &rightListNext, e);
	    doDownList(parent->down[num].next, &topListNext, num, e);

	    chooseMedian2( rightListNext, topListNext, &e->mix.next);
	    }
	else
	    {
	    doUpList(e->edges[0]->up.prev,&rightListPrev, e);
	    doUpList(e->edges[1]->up.prev,&leftListPrev, e);
	    doDownList(parent->down[num].prev, &topListPrev, num, e);

	    chooseMedian3(leftListPrev, rightListPrev, topListPrev,  &e->mix.prev);

	    if ((topListPrev || rightListPrev || leftListPrev) && (e->mix.prev == NULL))
		errAbort("no mix prev");

	    doUpList(e->edges[0]->up.next,&rightListNext, e);
	    doUpList(e->edges[1]->up.next,&leftListNext, e);
	    doDownList(parent->down[num].next, &topListNext, num, e);

	    chooseMedian3(leftListNext, rightListNext, topListNext,  &e->mix.next);

	    if ((topListNext || rightListNext || leftListNext) && (e->mix.next == NULL))
		errAbort("no mix next");

	    }
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

void reOrderNode(FILE *f, struct genome *g)
{
struct element *e, *saveE = NULL;
boolean inFront = TRUE;

saveE =  g->breaks->element;
if (!sameString(saveE->name, "START"))
    {
    saveE =  g->breaks->next->element;
    if (!sameString(saveE->name, "START"))
	errAbort("no START");
    }

inFront = TRUE;
g->elements = NULL;
for(e = saveE ; e ; )
    {
    struct possibleEdge *p;

    if (!inFront)
	e->isFlipped = TRUE;

    if (inFront == TRUE)
	p = e->calced.next;
    else
	p = e->calced.prev;

    if (!p)
	{
	warn("end without END");
	break;
	}

    inFront = !p->doFlip ; //? (inFront ? FALSE : TRUE) : inFront;
    e = p->element;
    if (sameString("END", e->name))
	break;

    e->next = NULL;
    slAddHead(&g->elements, e);
    }
slReverse(&g->elements);
}


void reOrderTree(FILE *f, struct phyloTree *node)
{
int ii;
struct genome *g = node->priv;

if (node->numEdges)
    reOrderNode(f, g);
else
    {
    removeStartStop(g);
    }

for(ii=0; ii < node->numEdges; ii++)
    reOrderTree(f, node->edges[ii]);
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
    /*
    e->up.next = NULL;
    e->up.prev  = NULL;
    e->down[0].next = NULL;
    e->down[0].prev  = NULL;
    e->down[1].next  = NULL;
    e->down[1].prev  = NULL;
    e->mix.next  = NULL;
    e->mix.prev  = NULL;
    */
    if(e->up.next) slFreeList(&e->up.next);
    if(e->up.prev ) slFreeList(&e->up.prev );
    if(e->down[0].next) slFreeList(&e->down[0].next);
    if(e->down[0].prev ) slFreeList(&e->down[0].prev );
    if(e->down[1].next ) slFreeList(&e->down[1].next );
    if(e->down[1].prev ) slFreeList(&e->down[1].prev );
    if(e->mix.next ) slFreeList(&e->mix.next );
    if(e->mix.prev ) slFreeList(&e->mix.prev );

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

for(e=g->elements; e; e = e->next)
    {
    if ((e->calced.prev == NULL) && (e->mix.prev ))
    {
    purgeList(&e->mix.prev);
    for(edge = e->mix.prev; edge; edge = edge->next)
	{
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

AllocVar(newEdge);
newEdge->count = count;
newEdge->doFlip = edge->doFlip;
newEdge->element = edge->element;

if (doFlip)
    {
    if (e->calced.prev != NULL)
	errAbort("calced edge already set");
    e->calced.prev = newEdge;
    }
else 
    {
    if (e->calced.next != NULL)
	errAbort("calced edge already set");
    e->calced.next = newEdge;
    }

AllocVar(newEdge);
newEdge->element = e;
newEdge->count = count;
newEdge->doFlip = doFlip ? FALSE : TRUE ;
if (!edge->doFlip)
    {
    if (edge->element->calced.prev != NULL)
	errAbort("calced edge already set");
    edge->element->calced.prev = newEdge;
    }
else 
    {
    if (edge->element->calced.next != NULL)
	errAbort("calced edge already set");
    edge->element->calced.next = newEdge;
    }
}

void orderNodes(char *treeFile, char *outFile)
{
struct phyloTree *node = eleReadTree(treeFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct possibleEdge *edge;
struct element *bestElement;
int best;
int totalCount;
boolean doFlip;

setLeafNodePairs(node);
//setAllPairs(g);

for(;;)
    {
    clearNodes(node);
    calcUpNodes(node);
    //printNodes(f, node, "up");
    calcDownNodes(node);
    //printNodes(f, node, "down0");
    //printNodes(f, node, "down1");
    calcMix(node);

    //printNodes(f, node, "mix");

    edge = NULL;
    best = 0;
    findBestEdge(node, &edge, &best, &doFlip, &bestElement);
    if (edge == NULL)
	{
	//printf("breaqking\n");
	break;
	}

    if (doFlip)
	totalCount = getTotalCount( &bestElement->up.prev);
    else
	totalCount = getTotalCount( &bestElement->up.next);

    printf("best adjacency %c%s to %c%s count %d\n",doFlip ? '-' : ' ', 
	eleName(bestElement),edge->doFlip ? '-' : ' ',eleName(edge->element),best);
    setEdge(bestElement, edge, doFlip, totalCount);
    putchar('.');
    fflush(stdout);
    }

reOrderTree(f, node);
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

#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: orderNodes.c,v 1.4 2006/02/10 15:57:15 braney Exp $";

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
    if ((list->element == e) && (list->doFlip == doNeg))
	break;

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
    edge->count = (((double)count * 65535 / totalCount ) + edge->count)/2;
    //errAbort("shouldn't find edge in addPairProb");
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


void doUpList(struct possibleEdge *inList, struct possibleEdge **outList)
{
struct possibleEdge *p;

for(p = inList; p ; p = p->next)
    {
    struct element *nextParent = p->element->parent;

    if (nextParent == NULL)
	errAbort("next in hash doesn't have parent");

    addPair(outList, nextParent, p->doFlip, p->count);
    }
}

void doUpProbs(struct possibleEdge *inList, struct possibleEdge **outList)
{
struct possibleEdge *p;
int totalCount = 0;

for(p = inList; p ; p = p->next)
    totalCount += p->count;

for(p = inList; p ; p = p->next)
    {
    struct element *nextParent = p->element->parent;

    if (nextParent == NULL)
	errAbort("next in hash doesn't have parent");

    addPairProb(outList, nextParent, p->doFlip, p->count, totalCount);
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

for(p = inList; p ; p = p->next)
    totalCount += p->count;

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

if (node->parent)
    {
    struct element *e;

    for(e=g->elements; e; e = e->next)
	{
	struct element *parent = e->parent;
	//double useWeight = weight;

	//if (node->parent->numEdges == 1)
	    //useWeight /= parent->numEdges;

	doUpList(e->up.next, &parent->up.next);
	doUpList(e->up.prev, &parent->up.prev);
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
	    }
	else
	    {
	    doUpList(e->edges[1]->up.prev, &e->down[0].prev);
	    doUpList(e->edges[0]->up.prev, &e->down[1].prev);

	    doUpList(e->edges[1]->up.next, &e->down[0].next);
	    doUpList(e->edges[0]->up.next, &e->down[1].next);
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
	    doDownList(parent->down[num].prev, &e->down[0].prev, num, e);
	    doDownList(parent->down[num].next, &e->down[0].next, num, e);
	    }
	else
	    {
	    doUpList(e->edges[0]->up.prev,&e->down[1].prev);
	    doDownList(parent->down[num].prev, &e->down[1].prev, num, e);
	
	    doUpList(e->edges[1]->up.prev,&e->down[0].prev);
	    doDownList(parent->down[num].prev,&e->down[0].prev, num, e);

	    doUpList(e->edges[0]->up.next,&e->down[1].next);
	    doDownList(parent->down[num].next, &e->down[1].next, num, e);
	
	    doUpList(e->edges[1]->up.next,&e->down[0].next);
	    doDownList(parent->down[num].next,&e->down[0].next, num, e);
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

for(p = left; p ; p = p->next)
    {
    AllocVar(newEdge);
    newEdge->element = p->element;
    newEdge->doFlip = p->doFlip;
    newEdge->count = p->count;
    slAddHead(median, newEdge);

    if ((edge1 = findEdge(right, p->element, p->doFlip)) != NULL)
	{
	edge1->element = NULL;
	
	if (edge1->count > p->count)
	    newEdge->count = edge1->count;

	if ((edge2 = findEdge(top, p->element, p->doFlip)) != NULL)
	    {
	    edge2->element = NULL;
	    if ((edge2->count < newEdge->count) && (edge2->count > edge1->count)) 
		newEdge->count = edge2->count;
	    }
	}
    }

for(p = right; p ; p = p->next)
    {
    if (p->element != NULL)
	{
	AllocVar(newEdge);
	newEdge->element = p->element;
	newEdge->doFlip = p->doFlip;
	newEdge->count = p->count;
	slAddHead(median, newEdge);

	if ((edge2 = findEdge(top, p->element, p->doFlip)) != NULL)
	    {
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
    AllocVar(newEdge);
    newEdge->element = p->element;
    newEdge->doFlip = p->doFlip;
    newEdge->count = p->count;
    slAddHead(median, newEdge);

    if ((edge = findEdge(right, p->element, p->doFlip)) != NULL)
	{
	edge->element = NULL;
	
	if (edge->count > p->count)
	    newEdge->count = p->count;
	}
    }
for(p = right; p ; p = p->next)
    {
    if (p->element != NULL)
	{
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
int ii;
//double totalLen = 0.0;
//struct possibleEdge *leftListPrev = NULL;
//struct possibleEdge *topListNext = NULL;
//struct possibleEdge *rightListNext = NULL;
//struct possibleEdge *leftListNext = NULL;

if (node->parent == NULL)
    {
    struct element *e;

    for(e=g->elements; e; e = e->next)
	{
	//printf("topnode working on %s\n",eleName(e));
	if (e->numEdges == 1)
	    {
	    }
	else
	    {
	    struct possibleEdge *topListPrev = NULL;
	    struct possibleEdge *rightListPrev = NULL;
	    struct possibleEdge *leftListPrev = NULL;
	    struct possibleEdge *topListNext = NULL;
	    struct possibleEdge *rightListNext = NULL;
	    struct possibleEdge *leftListNext = NULL;

	    doUpProbs(e->edges[1]->up.prev, &rightListPrev);
	    doUpProbs(e->edges[0]->up.prev, &leftListPrev);
	    chooseMedian2(leftListPrev, rightListPrev,  &e->mix.prev);

	    doUpProbs(e->edges[1]->up.next, &rightListNext);
	    doUpProbs(e->edges[0]->up.next, &leftListNext);
	    chooseMedian2(leftListNext, rightListNext, &e->mix.next);
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
	    doDownProbs(parent->down[num].prev, &topListPrev, num, e);
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
	    doUpProbs(e->edges[0]->up.prev,&rightListPrev);
	    doDownProbs(parent->down[num].prev, &topListPrev, num, e);
	    doUpProbs(e->edges[1]->up.prev,&leftListPrev);
	    chooseMedian3(topListPrev, rightListPrev, leftListPrev,  &e->mix.prev);

	    doUpProbs(e->edges[0]->up.next,&rightListNext);
	    doDownProbs(parent->down[num].next, &topListNext, num, e);
	    doUpProbs(e->edges[1]->up.next,&leftListNext);
	    chooseMedian3(topListNext, rightListNext, leftListNext,  &e->mix.next);

	    //doUpList(e->edges[0]->up.prev,&e->mix.prev);
	    //doDownList(parent->down[num].prev, &e->mix.prev, num, e);
	    //doUpList(e->edges[1]->up.prev,&e->mix.prev);
	    //doUpList(e->edges[0]->up.next,&e->mix.next);
	    //doDownList(parent->down[num].next, &e->mix.next, num, e);
	    //doUpList(e->edges[1]->up.next,&e->mix.next);
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

totalCount = 0;
for(p= adj->next ; p ; p = p->next)
    totalCount += p->count;

for(p= adj->next ; p ; p = p->next)
    {
    fprintf(f, "%s\t%s\t%d\t%g\n",name, eleFullName(p->element, p->doFlip ^ p->element->isFlipped), p->count, (double)p->count/totalCount);
    }

totalCount = 0;
for(p= adj->prev ; p ; p = p->next)
    totalCount += p->count;
for(p= adj->prev ; p ; p = p->next)
    {
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

void findBestEdge(struct phyloTree *node, struct possibleEdge **pEdge, int *bestYet, boolean *neg, struct element **pEvent)
{
struct possibleEdge *edge = NULL;
int ii;
struct genome *g = node->priv;
struct element *e;

for(ii=0; ii < node->numEdges; ii++)
    findBestEdge(node->edges[ii], pEdge, bestYet, neg, pEvent);

for(e=g->elements; e; e = e->next)
    {
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


void orderNodes(char *treeFile, char *outFile)
{
struct phyloTree *node = eleReadTree(treeFile);
FILE *f = mustOpen(outFile, "w");
struct possibleEdge *edge;
struct element *bestElement;
int best;
boolean doFlip;
//double branchLen = atof(lenString);
//struct genome *g = getGenome(outGroup, outGroup);

setLeafNodePairs(node);
//setAllPairs(g);

calcUpNodes(node);
calcDownNodes(node);
calcMix(node);

printNodes(f, node, "up");
printNodes(f, node, "down0");
printNodes(f, node, "down1");
printNodes(f, node, "mix");

findBestEdge(node, &edge, &best, &doFlip, &bestElement);
printf("best adjacency %c%s to %c%s count %d\n",doFlip ? '-' : ' ', 
    eleName(bestElement),edge->doFlip ? '-' : ' ',eleName(edge->element),best);

if (doFlip)
    {

    }


//printElementTrees(node, 0);
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

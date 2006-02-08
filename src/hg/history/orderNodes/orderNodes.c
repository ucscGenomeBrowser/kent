#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: orderNodes.c,v 1.3 2006/02/08 20:34:31 braney Exp $";

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

struct possibleEdge
{
    double prob;
    struct element *element;
};

void addPair( struct hash *hash, struct element *e2, double val,  boolean doNeg)
{
struct possibleEdge *edge;
char *name;

/*
if (e1->up.nextHash == NULL)
    e1->up.nextHash = newHash(5);
if (e1->up.prevHash == NULL)
    e1->up.prevHash = newHash(5);

hash = e1->up.nextHash;
if (doPrev)
    hash = e1->up.prevHash;
    */

if (val > 1.0)
    errAbort("prob greater than 1");
name = eleFullName(e2, doNeg );
//printf("checking for %s ",name);
if ((edge = hashFindVal(hash, name)) == NULL)
    {
    //printf("creating\n");
    AllocVar(edge);
    edge->prob = val;
    edge->element = e2;
    hashAdd(hash, name, edge);
    }
else
    {
    //printf("found\n");
    if (edge->element != e2)
	errAbort("not the same element");

    edge->prob += val;
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

void setPair( struct element *e1, struct element *e2, double val, boolean doNeg)
{
struct possibleEdge *edge = NULL;
char *name;
boolean flipE1 = e1->isFlipped ^ doNeg;
boolean flipE2 = e2->isFlipped ^ doNeg;
struct hash *hash;

if (e1->up.nextHash == NULL)
    e1->up.nextHash = newHash(5);
if (e1->up.prevHash == NULL)
    e1->up.prevHash = newHash(5);

hash = e1->up.nextHash;
if (flipE1)
    hash = e1->up.prevHash;

name = eleFullName(e2, doNeg );

if ((edge = hashFindVal(hash, name)) == NULL)
    {
    AllocVar(edge);
    edge->prob = val;
    edge->element = e2;
    hashAdd(hash, name, edge);
    }
else
    {
    if (edge->prob != val)
	errAbort("existing value doesn't match");
    }
}



void setAllPairs(struct genome *g)
{
struct element *e;
struct hash *h;

for(e = g->elements; e->next; e = e->next)
    {
    setPair(e, e->next, 1.0, FALSE);
    setPair(e->next, e, 1.0, TRUE);

    //hashStore(e->adj.up.prevHash, eleFullName(e), e->next);
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
    //correctAllPairs(g);
    }
}


void doUpHash(struct hash *hash, struct hash **parentHash, double weight)
{
struct hashCookie cookie;
struct hashEl *next;

if (hash != NULL)
    {
    cookie = hashFirst(hash);
    while(next = hashNext(&cookie))
	{
	struct possibleEdge *edge = next->val;
	struct element *nextParent = edge->element->parent;

	if (nextParent == NULL)
	    errAbort("next in hash doesn't have parent");

	//printf("looking at %s\n",eleName(edge->element));
	//printf("parent is %s\n",eleName(edge->element->parent));
	if (*parentHash == NULL)
	    *parentHash = newHash(4);

	addPair(*parentHash, nextParent, edge->prob * weight , *next->name == '-');
	}
    }
}

void doDownHash(struct hash *hash, struct hash **setHash, double weight, int childNum, struct element *e)
{
struct hashCookie cookie;
struct hashEl *next;

if (hash != NULL)
    {
    cookie = hashFirst(hash);
    while(next = hashNext(&cookie))
	{
	struct possibleEdge *edge = next->val;
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
	if (*setHash == NULL)
	    *setHash = newHash(4);

	if (!sameString(e->species, nextChild->species))
	    errAbort("not same species");
	printf("setting edge from %s to %s w %g p %g\n",eleName(e), eleName(nextChild), weight, edge->prob);
	addPair(*setHash, nextChild, edge->prob * weight , *next->name == '-');
	}
    }
}

void doSibHash(struct hash *hash, struct hash **setHash, double weight, int childNum)
{
struct hashCookie cookie;
struct hashEl *next;

if (hash != NULL)
    {
    cookie = hashFirst(hash);
    while(next = hashNext(&cookie))
	{
	struct possibleEdge *edge = next->val;
	struct element *nextParent = edge->element->parent;
	struct element *nextChild = edge->element->parent;

	if (nextParent == NULL)
	    errAbort("next in hash doesn't have parent");
	if (nextChild == NULL)
	    errAbort("next in hash doesn't have child");

	if (*setHash == NULL)
	    *setHash = newHash(4);

	addPair(*setHash, nextChild, edge->prob * weight , *next->name == '-');
	}
    }
}

void calcUpNodes(struct phyloTree *node, double weight)
{
struct genome *g = node->priv;
int ii;
double totalLen = 0.0;

for(ii=0; ii < node->numEdges; ii++)
    totalLen += node->edges[ii]->ident->length;

for(ii=0; ii < node->numEdges; ii++)
    //calcUpNodes(node->edges[ii], (totalLen - node->edges[ii]->ident->length)/totalLen);
    calcUpNodes(node->edges[ii], 1.0 / node->numEdges);

if (node->parent)
    {
    struct element *e;

    for(e=g->elements; e; e = e->next)
	{
	struct element *parent = e->parent;
	double useWeight = weight;

	if (node->parent->numEdges == 1)
	    useWeight /= parent->numEdges;

	doUpHash(e->up.nextHash, &parent->up.nextHash, useWeight);
	doUpHash(e->up.prevHash, &parent->up.prevHash, useWeight);
	}
    }
}

void calcDownNodes(struct phyloTree *node, double weight)
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
	    doUpHash(e->edges[1]->up.prevHash, &e->down[0].prevHash, weight);
	    doUpHash(e->edges[0]->up.prevHash, &e->down[1].prevHash, weight);

	    doUpHash(e->edges[1]->up.nextHash, &e->down[0].nextHash, weight);
	    doUpHash(e->edges[0]->up.nextHash, &e->down[1].nextHash, weight);
	    }
	}
    }
else if (node->numEdges)
    {
    struct element *e;

    for(e=g->elements; e; e = e->next)
	{
	struct element *parent = e->parent;
	double useUpWeight = weight;
	double useDownWeight = weight;
	int num;

	if (node->parent->numEdges == 1)
	    useDownWeight /= parent->numEdges;
	if (node->numEdges == 1)
	    useUpWeight /= e->numEdges;

	//printf("working on %s\n",eleName(e));
	num = 0;
	if (e == parent->edges[1])
	    num = 1;

	if (e->numEdges == 1)
	    {
	    doDownHash(parent->down[num].prevHash, &e->down[0].prevHash, useDownWeight, num, e);
	    doDownHash(parent->down[num].nextHash, &e->down[0].nextHash, useDownWeight, num, e);
	    }
	else
	    {
	    doUpHash(e->edges[0]->up.prevHash,&e->down[1].prevHash, useUpWeight);
	    doDownHash(parent->down[num].prevHash, &e->down[1].prevHash, useDownWeight, num, e);
	
	    doUpHash(e->edges[1]->up.prevHash,&e->down[0].prevHash, useUpWeight);
	    doDownHash(parent->down[num].prevHash,&e->down[0].prevHash, useDownWeight, num, e);

	    doUpHash(e->edges[0]->up.nextHash,&e->down[1].nextHash, useUpWeight);
	    doDownHash(parent->down[num].nextHash, &e->down[1].nextHash, useDownWeight, num, e);
	
	    doUpHash(e->edges[1]->up.nextHash,&e->down[0].nextHash, useUpWeight);
	    doDownHash(parent->down[num].nextHash,&e->down[0].nextHash, useDownWeight, num, e);
	    }
	}
    } 
for(ii=0; ii < node->numEdges; ii++)
    {
    calcDownNodes(node->edges[ii], 1.0 / node->numEdges);
    }
//for(ii=0; ii < node->numEdges; ii++)
    //totalLen += node->edges[ii]->ident->length;

}

void printTransSub(FILE *f, struct adjacency *adj, char *name)
{
struct hashEl *next;
struct hashCookie cookie;
//char name[512];

if (adj->nextHash != NULL)
    {
    //safef(name, sizeof name, "%s", eleFullName2(e, FALSE));
    cookie = hashFirst(adj->nextHash);
    while(next = hashNext(&cookie))
	{
	struct possibleEdge *edge = next->val;
	fprintf(f, "%s\t%s\t%g\n",name, eleFullName(edge->element, (*next->name == '-') ^ edge->element->isFlipped), edge->prob);
	}
    }

if (adj->prevHash != NULL)
    {
    cookie = hashFirst(adj->prevHash);
    while(next = hashNext(&cookie))
	{
	struct possibleEdge *edge = next->val;
	fprintf(f, "-%s\t%s\t%g\n",name, eleFullName(edge->element, (*next->name == '-') ^ edge->element->isFlipped), edge->prob);
	}
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

void orderNodes(char *treeFile, char *outFile)
{
struct phyloTree *node = eleReadTree(treeFile);
FILE *f = mustOpen(outFile, "w");
//double branchLen = atof(lenString);
//struct genome *g = getGenome(outGroup, outGroup);

setLeafNodePairs(node);
//setAllPairs(g);

calcUpNodes(node, 0.0);
calcDownNodes(node, 1.0);

printNodes(f, node, "up");
printNodes(f, node, "down0");
printNodes(f, node, "down1");

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

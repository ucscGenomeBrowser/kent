#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: orderNodes.c,v 1.2 2006/01/31 06:36:46 braney Exp $";

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
    if (edge->element != e2)
	errAbort("not the same edge");

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


void calcDownNodes(struct phyloTree *node, struct genome *out)
{
int ii;
struct element *e;

for(e=g->elements; e; e = e->next)
    {
    struct hashCookie cookie;
    struct hashEl *next;
    struct element *parent = e->parent;
    struct element *nextParent;


    if (e->up.nextHash != NULL)
	{
	cookie = hashFirst(e->up.nextHash);
	while(next = hashNext(&cookie))
	    {
	    struct possibleEdge *edge = next->val;
	    struct element *nextParent = edge->element->parent;

	    printf("ooking at %s->%s\n",eleFullName(e,FALSE), next->name);

	    if (nextParent == NULL)
		errAbort("next in hash doesn't have parent");

	    if (parent->up.nextHash == NULL)
		parent->up.nextHash = newHash(4);
	    addPair(parent->up.nextHash, nextParent, edge->prob * weight, *next->name == '-');
	    }
	}

    if (e->up.prevHash != NULL)
	{
	cookie = hashFirst(e->up.prevHash);
	while(next = hashNext(&cookie))
	    {
	    struct possibleEdge *edge = next->val;
	    struct element *nextParent = edge->element->parent;

	    printf("tooking at %s\n",next->name);

	    if (nextParent == NULL)
		errAbort("next in hash doesn't have parent");

	    if (parent->up.prevHash == NULL)
		parent->up.prevHash = newHash(5);

	    addPair(parent->up.prevHash, nextParent, edge->prob * weight, *next->name == '-');
	    }
	}
    }

for(ii=0; ii < node->numEdges; ii++)
    {
    calcDownNodes(node->edges[ii], node->priv);
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
	struct hashCookie cookie;
	struct hashEl *next;
	struct element *parent = e->parent;
	struct element *nextParent;


	if (e->up.nextHash != NULL)
	    {
	    cookie = hashFirst(e->up.nextHash);
	    while(next = hashNext(&cookie))
		{
		struct possibleEdge *edge = next->val;
		struct element *nextParent = edge->element->parent;

		printf("ooking at %s->%s\n",eleFullName(e,FALSE), next->name);

		if (nextParent == NULL)
		    errAbort("next in hash doesn't have parent");

		if (parent->up.nextHash == NULL)
		    parent->up.nextHash = newHash(4);
		addPair(parent->up.nextHash, nextParent, edge->prob * weight, *next->name == '-');
		}
	    }

	if (e->up.prevHash != NULL)
	    {
	    cookie = hashFirst(e->up.prevHash);
	    while(next = hashNext(&cookie))
		{
		struct possibleEdge *edge = next->val;
		struct element *nextParent = edge->element->parent;

		printf("tooking at %s\n",next->name);

		if (nextParent == NULL)
		    errAbort("next in hash doesn't have parent");

		if (parent->up.prevHash == NULL)
		    parent->up.prevHash = newHash(5);

		addPair(parent->up.prevHash, nextParent, edge->prob * weight, *next->name == '-');
		}
	    }
	}
    }
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

fprintf(f, ">%s\n",g->name);

for(; e ; e = e->next)
    {
    safef(name, sizeof name, "%s", eleFullName2(e, FALSE));

    if (sameString(which, "up"))
	printTransSub(f, &e->up, name);
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

void orderNodes(char *treeFile, char *outGroup, char *lenString , char *outFile)
{
struct phyloTree *node = eleReadTree(treeFile);
FILE *f = mustOpen(outFile, "w");
double branchLen = atof(lenString);
struct genome *g = getGenome(outGroup, outGroup);

setLeafNodePairs(node);
setAllPairs(g);

calcUpNodes(node, 0.0);
calcDownNodes(node, g);

printNodes(f, node, "up");

//printElementTrees(node, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();

//verboseSetLevel(2);
orderNodes(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

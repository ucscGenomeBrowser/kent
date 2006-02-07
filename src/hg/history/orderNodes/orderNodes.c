#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: orderNodes.c,v 1.1 2006/01/23 03:49:06 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "orderNodes - orders the ancestor nodes in  an element tree\n"
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

void setPair(struct genome *g, struct element *e1, struct element *e2, double val, boolean doNeg)
{
double *pval;
char *name;
struct hash *h;
boolean flipSecond = FALSE;

if (e1->isFlipped)
    flipSecond = TRUE;

name = eleFullName(e1, doNeg ^ flipSecond);
if ((h = hashFindVal(g->pairOrders, name)) == NULL)
    {
    h = newHash(5);
    hashAdd(g->pairOrders, name, h);
    }

name = eleFullName(e2, doNeg ^ flipSecond);
if ((pval = hashFindVal(h, name)) == NULL)
    {
    AllocVar(pval);
    *pval = val;
    hashAdd(h, name, pval);
    }
else
    {
    if (*pval != val)
	errAbort("existing value doesn't match");
    }
}

void setAllPairs(struct genome *g)
{
struct element *e = g->elements;
struct hash *h;

if (g->pairOrders == NULL)
    g->pairOrders = newHash(5);

for(; e->next; e = e->next)
    {
    setPair(g, e, e->next, 1.0, FALSE);
//    setPair(g, e->next, e, 1.0, TRUE);
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

void mergeChildHash(struct hash *groupHash, struct hash *childHash, double bLen)
{
struct hashCookie cookie = hashFirst(childHash);
struct hashEl *next;

while(next = hashNext(&cookie))
    {
    double *pval;
    double childVal;

    childVal = *(double *)next->val;
    if ((pval = hashFindVal(groupHash, next->name)) == NULL)
	{
	AllocVar(pval);
	*pval = 0.0;
	hashAdd(groupHash, next->name, pval);
	}
    *pval += childVal * bLen;
    }
}

void mergeHash(struct hash *groupHash, struct hash *childHash, double bLen)
{
struct hashCookie cookie = hashFirst(childHash);
struct hashEl *next;

while(next = hashNext(&cookie))
    {
    struct hash *h;

    if ((h = hashFindVal(groupHash, next->name)) == NULL)
	{
	h = newHash(5);
	hashAdd(groupHash, next->name, h);
	}

    mergeChildHash(h, next->val, bLen);
    }
}

void calcNodes(struct phyloTree *node)
{
if (node->numEdges)
    {
    struct genome *g = node->priv;
    int ii;
    double totalLen = 0.0;

    for(ii=0; ii < node->numEdges; ii++)
	{
	totalLen += node->edges[ii]->ident->length;
	calcNodes(node->edges[ii]);
	}

    g->pairOrders = newHash(5);

    for(ii=0; ii < node->numEdges; ii++)
	{
	struct genome *og = node->edges[ii]->priv;
	double bLen = node->edges[ii]->ident->length / totalLen; 

	mergeHash(g->pairOrders, og->pairOrders, bLen);
	}
    }
}

void printTransSub(FILE *f, char *name, struct hash *h)
{
struct hashCookie cookie = hashFirst(h);
struct hashEl *next;

while(next = hashNext(&cookie))
    fprintf(f, "%s\t%s\t%g\n",name, next->name, *(double *)next->val);
}

void printTrans(FILE *f, struct genome *g)
{
struct hashCookie cookie = hashFirst(g->pairOrders);
struct hashEl *next;

fprintf(f, ">%s\n",g->name);
while(next = hashNext(&cookie))
    {
    printTransSub(f, next->name, next->val);
    }
}

void printNodes(FILE *f, struct phyloTree *node)
{
struct genome *g = node->priv;
int ii;

for(ii=0; ii < node->numEdges; ii++)
    printNodes(f, node->edges[ii]);

printTrans(f, g);
}

void orderNodes(char *treeFile, char *outFile)
{
struct phyloTree *node = eleReadTree(treeFile);
FILE *f = mustOpen(outFile, "w");

setLeafNodePairs(node);
calcNodes(node);

printNodes(f, node);

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

#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: synthElemTree.c,v 1.1 2006/01/19 00:13:36 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "synthElemTree - synthesize an element tree\n"
  "usage:\n"
  "   synthElemTree outTree\n"
  "arguments:\n"
  "   outTree      name of file containing element tree\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int NumInitialElements = 5;
int MaxGeneration = 40000;
int MaxGenomes = 3;

void speciate(struct genome **list, struct genome *g)
{
struct genome *g1, *g2;
struct phyloTree *t1, *t2;
struct element *p;
char buffer[512];

AllocVar(t1);
AllocVar(t1->ident);
safef(buffer, sizeof(buffer), "%s0",g->name);
t1->ident->name = cloneString(buffer);
t1->ident->length = 1 + random() % 7;
AllocVar(g1);
slAddHead(list, g1);
g1->node = t1;
t1->priv = g1;
g1->name = t1->ident->name;
//printf("adding %s to list \n",g1->name);
//g1->parent = g;
phyloAddEdge(g->node, t1);

AllocVar(t2);
AllocVar(t2->ident);
safef(buffer, sizeof(buffer), "%s1",g->name);
t2->ident->name = cloneString(buffer);
t2->ident->length = 1 + random() % 7;
AllocVar(g2);
slAddHead(list, g2);
g2->node = t2;
t2->priv = g2;
g2->name = t2->ident->name;
//printf("adding %s to list \n",g2->name);
//g1->parent = g;
phyloAddEdge(g->node, t2);

//g1->next = g2;
//g2->next = g->next;

for(p = g->elements; p; p=p->next)
    {
    struct element *e;

    AllocVar(e);
    slAddHead(&g1->elements, e);
    e->species = g1->name;
    e->name = p->name;
    safef(buffer, sizeof(buffer), "%s0",p->version);
    e->version = cloneString(buffer);
    eleAddEdge(p, e);

    AllocVar(e);
    slAddHead(&g2->elements, e);
    e->species = g2->name;
    e->name = p->name;
    safef(buffer, sizeof(buffer), "%s1",p->version);
    e->version = cloneString(buffer);
    eleAddEdge(p, e);
    }

slReverse(&g1->elements);
slReverse(&g2->elements);
}

void duplicate(struct genome **list, struct genome *g)
{
struct genome *g1;
struct phyloTree *t1;
struct element *p;
char buffer[512];
int r = random() % slCount(g->elements);
int didIt = 0;

AllocVar(t1);
AllocVar(t1->ident);
safef(buffer, sizeof(buffer), "%sD",g->name);
t1->ident->name = cloneString(buffer);
t1->ident->length = 1 + random() % 7;
AllocVar(g1);
slAddHead(list, g1);
g1->node = t1;
t1->priv = g1;
g1->name = t1->ident->name;
//printf("adding %s to list \n",g1->name);
//g1->parent = g;
phyloAddEdge(g->node, t1);

didIt = 0;
for(p = g->elements; p; p=p->next)
    {
    struct element *e;

    if (r-- == 0)
	{
	didIt = 1;
	AllocVar(e);
	slAddHead(&g1->elements, e);
	e->species = g1->name;
	e->name = p->name;
	safef(buffer, sizeof(buffer), "%s.0",p->version);
	e->version = cloneString(buffer);
	eleAddEdge(p, e);

	AllocVar(e);
	slAddHead(&g1->elements, e);
	e->species = g1->name;
	e->name = p->name;
	safef(buffer, sizeof(buffer), "%s.1",p->version);
	e->version = cloneString(buffer);
	eleAddEdge(p, e);
	}
    else
	{
	AllocVar(e);
	slAddHead(&g1->elements, e);
	e->species = g1->name;
	e->name = p->name;
	safef(buffer, sizeof(buffer), "%s",p->version);
	e->version = cloneString(buffer);
	eleAddEdge(p, e);
	}
    }
if (!didIt)
    errAbort("didn't do it");

slReverse(&g1->elements);
}


void synthElemTree(char *outFile)
{
FILE *f = mustOpen(outFile,"w");
struct phyloTree *root = NULL;
struct genome *g;
int ii;
char name[512];
struct genome *gList;
int generation = 0;

AllocVar(root);
AllocVar(root->ident);
root->ident->name = cloneString("R");

AllocVar(g);
root->priv = g;
g->node = root;
g->name = root->ident->name;

for (ii=0; ii < NumInitialElements; ii++)
    {
    safef(name, sizeof(name), "%d",ii);
    newElement(g, cloneString(name), cloneString("0"));
    }
gList = g;
slReverse(&g->elements);

for (; generation < MaxGeneration ;generation++)
    {
    struct genome *prevGenome = NULL;
    struct genome *nextG = NULL;
    struct genome *nextList = NULL;

    //printf("gen %d\n",generation);
    for(g = gList ; g;   g = nextG)
	{
	int r = random();
	struct genome *newG = NULL;

	nextG = g->next;
	g->next = 0;

	if ((r % 5) == 0)
	    {
	    speciate(&nextList, g);
	    }
	else if ((r % 3) == 0) 
	    {
	    duplicate(&nextList, g);
	    }
	else
	    slAddHead(&nextList, g);

	}
    gList = nextList;

    if (slCount(gList) >= MaxGenomes)
	break;
    }

//printElementTrees(root, 0);
phyloPrintTreeNoDups(root, stdout);
outElementTrees(f, root);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

//verboseSetLevel(2);
srandom(getpid());
synthElemTree(argv[1]);
return 0;
}

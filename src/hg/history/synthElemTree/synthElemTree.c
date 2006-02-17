#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: synthElemTree.c,v 1.3 2006/02/17 01:15:00 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "synthElemTree - synthesize an element tree\n"
  "usage:\n"
  "   synthElemTree outTree\n"
  "arguments:\n"
  "   outTree      name of file containing element tree\n"
  "options\n"
  "   -NumElements    number of elements in initial genome\n"
  "   -MaxGeneration  max number of generations to run\n"
  "   -MaxGenomes     max number of genomes to generate\n"
  "   -SpeciesWt      divided by total weight to get probablity a node will speciate\n"
  "   -DupWt          divided by total weight to get probablity a node will have a duplication\n"
  "   -DelWt          divided by total weight to get probablity a node will have a deletion\n"
  "   -InverseWt      divided by total weight to get probablity a node will have an inversion\n"
  "   -NoWt           divided by total weight to get probablity a node will just clock branch length\n"
  );
}

static struct optionSpec options[] = {
    {"NumElements", OPTION_INT},
    {"MaxGeneration", OPTION_INT},
    {"MaxGenomes", OPTION_INT},
    {"SpeciesWt", OPTION_INT},
    {"DelWt", OPTION_INT},
    {"DupWt", OPTION_INT},
    {"NoWt", OPTION_INT},
    {"InverseWt", OPTION_INT},
   {NULL, 0},
};

int OutLength = 8;
int NumElements = 5;
int MaxGeneration = 40000;
int MaxGenomes = 50;
int SpeciesWt = 0;
int DupWt = 0;
int DelWt = 0;
int NoWt = 0;
int InverseWt = 0;


void speciate(struct genome **list, struct genome *g)
{
struct genome *g1, *g2;
struct phyloTree *t1, *t2;
struct element *p;
char buffer[512];

AllocVar(t1);
AllocVar(t1->ident);
safef(buffer, sizeof(buffer), "%s0",g->name);
//safef(buffer, sizeof(buffer), "%s",nextGenome());
t1->ident->name = cloneString(buffer);
t1->ident->length = 0; //1 + random() % 7;
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
//safef(buffer, sizeof(buffer), "%s",nextGenome());
t2->ident->name = cloneString(buffer);
t2->ident->length = 0; //1 + random() % 7;
AllocVar(g2);
slAddHead(list, g2);
g2->node = t2;
t2->priv = g2;
g2->name = t2->ident->name;
phyloAddEdge(g->node, t2);

for(p = g->elements; p; p=p->next)
    {
    struct element *e;

    AllocVar(e);
    slAddHead(&g1->elements, e);
    e->species = g1->name;
    e->name = p->name;
    e->isFlipped = p->isFlipped;
    //safef(buffer, sizeof(buffer), "%s0",p->version);
    safef(buffer, sizeof(buffer), "%s",nextVersion());
    e->version = cloneString(buffer);
    eleAddEdge(p, e);

    AllocVar(e);
    slAddHead(&g2->elements, e);
    e->species = g2->name;
    e->name = p->name;
    e->isFlipped = p->isFlipped;
    //safef(buffer, sizeof(buffer), "%s1",p->version);
    safef(buffer, sizeof(buffer), "%s",nextVersion());
    e->version = cloneString(buffer);
    eleAddEdge(p, e);
    }

slReverse(&g1->elements);
slReverse(&g2->elements);
}

void invert(struct genome **list, struct genome *g)
{
struct genome *g1;
struct phyloTree *t1;
struct element *p;
char buffer[512];
int r = random() % slCount(g->elements);
int n = 1;//random() % (slCount(g->elements) - r ) + 1;
int didIt = 0;

AllocVar(t1);
AllocVar(t1->ident);
safef(buffer, sizeof(buffer), "%sI",g->name);
//safef(buffer, sizeof(buffer), "%s",nextGenome());
t1->ident->name = cloneString(buffer);
printf("inverting %d to %d in %s\n",r,r+n, buffer);
t1->ident->length = 0; //1 + random() % 7;
AllocVar(g1);
slAddHead(list, g1);
g1->node = t1;
t1->priv = g1;
g1->name = t1->ident->name;
phyloAddEdge(g->node, t1);

didIt = 0;
for(p = g->elements; p; p=p->next)
    {
    struct element *e;

    AllocVar(e);
    slAddHead(&g1->elements, e);
    e->species = g1->name;
    e->name = p->name;
    e->isFlipped = p->isFlipped;
    safef(buffer, sizeof(buffer), "%s",nextVersion());
    //safef(buffer, sizeof(buffer), "%s",p->version);
    e->version = cloneString(buffer);
    eleAddEdge(p, e);

    if ((r <= 0) && (r > -n))
	{
	didIt = 1;
	e->isFlipped ^= TRUE;
	}
    r--;
    }
if (!didIt)
    errAbort("didn't do it");

slReverse(&g1->elements);
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
//safef(buffer, sizeof(buffer), "%s",nextGenome());
t1->ident->name = cloneString(buffer);
t1->ident->length = 0; //1 + random() % 7;
AllocVar(g1);
slAddHead(list, g1);
g1->node = t1;
t1->priv = g1;
g1->name = t1->ident->name;
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
	e->isFlipped = p->isFlipped;
	//safef(buffer, sizeof(buffer), "%s.0",p->version);
	safef(buffer, sizeof(buffer), "%s",nextVersion());
	e->version = cloneString(buffer);
	eleAddEdge(p, e);

	AllocVar(e);
	slAddHead(&g1->elements, e);
	e->species = g1->name;
	e->isFlipped = p->isFlipped;
	e->name = p->name;
	safef(buffer, sizeof(buffer), "%s",nextVersion());
	//safef(buffer, sizeof(buffer), "%s.1",p->version);
	e->version = cloneString(buffer);
	eleAddEdge(p, e);
	}
    else
	{
	AllocVar(e);
	slAddHead(&g1->elements, e);
	e->species = g1->name;
	e->name = p->name;
	e->isFlipped = p->isFlipped;
	safef(buffer, sizeof(buffer), "%s",nextVersion());
	//safef(buffer, sizeof(buffer), "%s",p->version);
	e->version = cloneString(buffer);
	eleAddEdge(p, e);
	}
    }
if (!didIt)
    errAbort("didn't do it");

slReverse(&g1->elements);
}

void delete(struct genome **list, struct genome *g)
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
//safef(buffer, sizeof(buffer), "%s",nextGenome());
t1->ident->name = cloneString(buffer);
t1->ident->length = 0; //1 + random() % 7;
AllocVar(g1);
slAddHead(list, g1);
g1->node = t1;
t1->priv = g1;
g1->name = t1->ident->name;
phyloAddEdge(g->node, t1);

didIt = 0;
for(p = g->elements; p; p=p->next)
    {
    struct element *e;

    if (r-- == 0)
	{
	didIt = 1;
	printf("deleteing %s\n",eleName(p));
	}
    else
	{
	AllocVar(e);
	slAddHead(&g1->elements, e);
	e->species = g1->name;
	e->name = p->name;
//	safef(buffer, sizeof(buffer), "%s",p->version);
	safef(buffer, sizeof(buffer), "%s",nextVersion());
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
struct phyloTree *uroot = NULL;
struct phyloTree *node = NULL;
struct genome *g;
int ii;
char name[512];
struct genome *gList = NULL;
int generation = 0;

AllocVar(root);
AllocVar(root->ident);
root->ident->name = cloneString("U");

AllocVar(g);
root->priv = g;
g->node = root;
g->name = root->ident->name;

for (ii=0; ii < NumElements; ii++)
    {
    safef(name, sizeof(name), "%d",ii);
    newElement(g, cloneString(name), cloneString(nextVersion()));
    }
slReverse(&g->elements);

speciate(&gList, g);

g = gList->next;
node = g->node;
node->ident->name = g->name = cloneString("Out");
node->ident->length = OutLength;
gList->next = 0;

//gList = g = g->next;
g = gList;
node = g->node;
node->ident->name = g->name = cloneString("R");

g = root->priv;
node->ident->name = g->name = cloneString("U");

for (; generation < MaxGeneration ;generation++)
    {
    struct genome *prevGenome = NULL;
    struct genome *nextG = NULL;
    struct genome *nextList = NULL;
    int weight = SpeciesWt + NoWt + DupWt + InverseWt + DelWt;

    for(g = gList ; g;   g = nextG)
	{
	int r = random() % weight;
	struct genome *newG = NULL;
	struct phyloTree *node = g->node;

	node->ident->length += 1;

	nextG = g->next;
	g->next = 0;

	if ((r -= SpeciesWt) < 0)
	    {
	    speciate(&nextList, g);
	    }
	else if ((r -= DelWt) < 0)
	    {
	    delete(&nextList, g);
	    }
	else if ((r -= DupWt) < 0)
	    {
	    duplicate(&nextList, g);
	    }
	else if ((r -= InverseWt) < 0)
	    {
	    invert(&nextList, g);
	    }
	else if ((r -= NoWt) < 0)
	    {
	    slAddHead(&nextList, g);
	    }
	}
    gList = nextList;

    if (slCount(gList) >= MaxGenomes)
	break;
    }

for(g = gList ; g; g = g->next)
    g->node->ident->length += 1;

phyloPrintTreeNoDups(root, stdout);
outElementTrees(f, root);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

NumElements = optionInt( "NumElements", 0);
MaxGeneration = optionInt( "MaxGeneration", 400000);
MaxGenomes = optionInt( "MaxGenomes", 0);
SpeciesWt = optionInt( "SpeciesWt", 0);
DelWt = optionInt( "DelWt", 0);
DupWt = optionInt( "DupWt", 0);
NoWt = optionInt( "NoWt", 0);
InverseWt = optionInt( "InverseWt", 0);

if (0 == SpeciesWt + DupWt + InverseWt + DelWt)
    errAbort("must specify at least one weight");

//verboseSetLevel(2);
srandom(getpid());
synthElemTree(argv[1]);
return 0;
}

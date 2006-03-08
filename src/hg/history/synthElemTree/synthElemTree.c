#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: synthElemTree.c,v 1.7 2006/03/08 22:39:45 braney Exp $";

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
  "   -trace          leave version strings the same from gen to gen unless dup'ed\n"
  "   -seed=num       use num as seed to random number generator\n"
  "   -InfSites       use infinite sites model\n"
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
    {"trace", OPTION_BOOLEAN},
    {"InfSites", OPTION_BOOLEAN},
    {"NumElements", OPTION_INT},
    {"seed", OPTION_INT}, 
    {"MaxGeneration", OPTION_INT}, 
    {"MaxGenomes", OPTION_INT},
    {"SpeciesWt", OPTION_INT},
    {"DelWt", OPTION_INT},
    {"DupWt", OPTION_INT},
    {"NoWt", OPTION_INT},
    {"InverseWt", OPTION_INT},
   {NULL, 0},
};

boolean Trace = FALSE;
boolean InfSites = FALSE;
struct hash *InfHash = NULL;
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
t1->ident->name = cloneString(buffer);
t1->ident->length = 0; 
AllocVar(g1);
slAddHead(list, g1);
g1->node = t1;
t1->priv = g1;
g1->name = t1->ident->name;
phyloAddEdge(g->node, t1);

AllocVar(t2);
AllocVar(t2->ident);
safef(buffer, sizeof(buffer), "%s1",g->name);
t2->ident->name = cloneString(buffer);
t2->ident->length = 0; 
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
    if (Trace)
	safef(buffer, sizeof(buffer), "%s",p->version);
    else
	safef(buffer, sizeof(buffer), "%s",nextVersion());
    e->version = cloneString(buffer);
    eleAddEdge(p, e);

    AllocVar(e);
    slAddHead(&g2->elements, e);
    e->species = g2->name;
    e->name = p->name;
    e->isFlipped = p->isFlipped;
    if (Trace)
	safef(buffer, sizeof(buffer), "%s",p->version);
    else
	safef(buffer, sizeof(buffer), "%s",nextVersion());
    e->version = cloneString(buffer);
    eleAddEdge(p, e);
    }

slReverse(&g1->elements);
slReverse(&g2->elements);
}

char * checkAfter(struct element *e)
{
char buffer[512];

if (e->isFlipped)
    safef(buffer, sizeof(buffer), "+%s",e->name);
else
    safef(buffer, sizeof(buffer), "%s+",e->name);
if (hashLookup(InfHash, buffer))
    {
    return NULL;
    }
return cloneString(buffer);
}

char * checkBefore(struct element *e)
{
char buffer[512];

if (e->isFlipped)
    safef(buffer, sizeof(buffer), "%s+",e->name);
else
    safef(buffer, sizeof(buffer), "+%s",e->name);
if (hashLookup(InfHash, buffer))
    {
    return NULL;
    }

return cloneString(buffer);
}

boolean checkInf(struct element *e1, struct element *e2, struct element *e3, struct element *e4)
{
char *ptr1, *ptr2, *ptr3, *ptr4;

if (ptr1 = checkAfter(e1))
    if (ptr2 = checkBefore(e2))
	if (ptr3 = checkAfter(e3))
	    if (ptr4 = checkBefore(e4))
		{
		hashAdd(InfHash, ptr1, NULL);
		hashAdd(InfHash, ptr2, NULL);
		hashAdd(InfHash, ptr3, NULL);
		hashAdd(InfHash, ptr4, NULL);

		return FALSE;
		}

//if (ptr1) freez(&ptr1);
//if (ptr2) freez(&ptr2);
//if (ptr3) freez(&ptr3);
//if (ptr4) freez(&ptr4);

return TRUE;
}

boolean checkInf6(struct element *e1, struct element *e2, struct element *e3, struct element *e4, struct element *e5, struct element *e6)
{
char *ptr1, *ptr2, *ptr3, *ptr4, *ptr5, *ptr6;

if (ptr1 = checkAfter(e1))
    if (ptr2 = checkBefore(e2))
	if (ptr3 = checkAfter(e3))
	    if (ptr4 = checkBefore(e4))
		if (ptr5 = checkAfter(e5))
		    if (ptr6 = checkBefore(e6))
		{
		hashAdd(InfHash, ptr1, NULL);
		hashAdd(InfHash, ptr2, NULL);
		hashAdd(InfHash, ptr3, NULL);
		hashAdd(InfHash, ptr4, NULL);
		hashAdd(InfHash, ptr5, NULL);
		hashAdd(InfHash, ptr6, NULL);

		return FALSE;
		}

//if (ptr1) freez(&ptr1);
//if (ptr2) freez(&ptr2);
//if (ptr3) freez(&ptr3);
//if (ptr4) freez(&ptr4);

return TRUE;
}


void copyDownElements(struct genome *parent, struct genome *child)
{
struct element *pe, *ce;
char buffer[512];

for(pe = parent->elements; pe ; pe = pe->next)
    {
    AllocVar(ce);
    slAddHead(&child->elements, ce);
    ce->species = child->name;
    ce->name = pe->name;
    ce->isFlipped = pe->isFlipped;
    if (Trace)
	safef(buffer, sizeof(buffer), "%s",pe->version);
    else
	safef(buffer, sizeof(buffer), "%s",nextVersion());
    ce->version = cloneString(buffer);
    eleAddEdge(pe, ce);
    }
slReverse(&child->elements);
}

struct genome *newGenome(struct genome *parent, char type)
{
struct phyloTree *t1;
struct genome *g1;
char buffer[512];

AllocVar(t1);
AllocVar(t1->ident);
safef(buffer, sizeof(buffer), "%s%c",parent->name,type);
t1->ident->name = cloneString(buffer);
t1->ident->length = 0; 
AllocVar(g1);
g1->node = t1;
t1->priv = g1;
g1->name = t1->ident->name;
phyloAddEdge(parent->node, t1);

return g1;
}

boolean invert(struct genome **list, struct genome *g)
{
struct genome *g1;
struct phyloTree *t1;
struct element *p, *afterStart, *pEnd, *prev, *start;
struct element *e1 , *e2, *e3, *e4;
int ii;
char buffer[512];
int r = 1 + random() % (slCount(g->elements) - 2);
int n = random() % (slCount(g->elements) - 1 - r  ) ;

assert(r > 0);

if (InfHash)
    {
    int ii;

    for(ii=0, p = g->elements; (ii < r - 1) && p; ii++,p=p->next)
	;
    e1 = p;
    e2 = p->next;

    for(; (ii < r + n  ) && p; ii++,p=p->next)
	;

    e3 = p;
    e4 = p->next;

    if (checkInf(e1, e2, e3, e4))
	return FALSE;
    }

g1 = newGenome(g, 'I');
slAddHead(list, g1);

copyDownElements(g, g1);

prev = NULL;
for(ii=0, p = g1->elements; ii < r  ; prev = p, ii++, p=p->next)
    ;

start = p;
for(; ii < r + n  ;  ii++, p=p->next)
    ;

afterStart = p->next;
p->next = NULL;

slReverse(&start);
prev->next = start;
prev = NULL;
for(p = start; p ; prev = p,p = p->next)
    p->isFlipped ^= TRUE;
prev->next = afterStart;

/*
printf("new list\n");
for(p = g1->elements; p; p = p->next)
    {
    if (p->isFlipped)
	printf("-");
    printf("%s ",p->name);
    }
printf("\n");
*/

return TRUE;
}

struct element *getElementCopy(struct element *list, int r, int n)
{
int ii;
struct element *copy = NULL;
struct element *p, *e;
char buffer[512];

for(ii=0, p = list; (ii < r ) && p; ii++,p=p->next)
    ;

if  ( p == list)
    errAbort("bad bad");
for(; (ii < r + n + 1  ) && p; ii++,p=p->next)
    {
    AllocVar(e);
    e->genome = p->genome;
    e->species = p->species;
    e->isFlipped = p->isFlipped;
    e->name = p->name;
    if (Trace)
	{
	safef(buffer, sizeof(buffer), "%s.1",p->version);
	e->version = cloneString(buffer);
	safef(buffer, sizeof(buffer), "%s.0",p->version);
	p->version = cloneString(buffer);
	}
    else
	e->version = cloneString(nextVersion());
    e->parent = p->parent;
    slAddHead(&copy, e);
    }

slReverse(&copy);
return copy;
}

boolean duplicate(struct genome **list, struct genome *g)
{
int ii;
struct genome *g1;
struct phyloTree *t1;
struct element *p, *end, *e1, *e2, *e3, *e4, *e5, *e6;
char buffer[512];
int r = 1 + random() % (slCount(g->elements) - 2);
int n = random() % (slCount(g->elements) - 1 - r  ) ;
int t = random() % (slCount(g->elements) - 2);
int didIt = 0;
struct element *eList;
int count;

for (count = 0; (count < 100) && (t >= r - 1) && ( t < r + n + 1); count++)
    t = 1 + random() % (slCount(g->elements) - 2);
if (count == 100)
    return FALSE;

if (InfSites)
    {
    int ii;

    for(ii=0, p = g->elements; (ii < r - 1) && p; ii++,p=p->next)
	;
    e1 = p;
    e2 = p->next;

    for(; (ii < r + n  ) && p; ii++,p=p->next)
	;

    e3 = p;
    e4 = p->next;

    for(ii=0, p = g->elements; (ii < t - 1) && p; ii++,p=p->next)
	;
    e5 = p;
    e6 = p->next;

    if (checkInf6(e1, e2, e3, e4, e5, e6))
	return FALSE;
    }

g1 = newGenome(g, 'D');
slAddHead(list, g1);
copyDownElements(g, g1);

eList = getElementCopy(g1->elements, r, n);
for(ii=0, p = g1->elements; (ii < t - 1) && p; ii++,p=p->next)
    ;
end = p->next;
p->next = eList;
while(eList->next)
    eList = eList->next;
eList->next = end;

return TRUE;
}

boolean delete(struct genome **list, struct genome *g)
{
struct genome *g1;
struct phyloTree *t1;
struct element *p, *del;
char buffer[512];
int r;
int n;
int didIt = 0;
int ii;

if (slCount(g->elements) == 2)
    return TRUE;

r = 1 + random() % (slCount(g->elements) - 2);
n = random() % (slCount(g->elements) - 1 - r  ) ;

g1 = newGenome(g, 'X');
slAddHead(list, g1);
copyDownElements(g, g1);

for(ii=0, p = g1->elements; (ii < r - 1) && p; ii++,p=p->next)
    ;
del = p;
for(; (ii < r + n  ) && p; ii++,p=p->next)
	;
del->next = p->next;

return TRUE;
#ifdef NOTNOW
AllocVar(t1);
AllocVar(t1->ident);
safef(buffer, sizeof(buffer), "%sD",g->name);
t1->ident->name = cloneString(buffer);
t1->ident->length = 0; 
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

    if ((r <= 0) && ( r >= -n))
	{
	didIt = 1;
	}
    else
	{
	AllocVar(e);
	slAddHead(&g1->elements, e);
	e->species = g1->name;
	e->name = p->name;
	safef(buffer, sizeof(buffer), "%s",nextVersion());
	e->version = cloneString(buffer);
	eleAddEdge(p, e);
	e->genome = p->genome;
	e->species = p->species;
	e->isFlipped = p->isFlipped;
	e->name = p->name;
	e->version = cloneString(nextVersion());
	e->parent = p->parent;
	}
    r--;
    }
if (!didIt)
    errAbort("didn't do it");
#endif

slReverse(&g1->elements);
}


void synthElemTree(char *outFile)
{
boolean firstTime = TRUE;
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

newElement(g, cloneString("START"), NULL);
for (ii=0; ii < NumElements; ii++)
    {
    safef(name, sizeof(name), "%d",ii);
    newElement(g, cloneString(name), cloneString(nextVersion()));
    }
newElement(g, cloneString("END"), NULL);
slReverse(&g->elements);

speciate(&gList, g);

g = gList->next;
node = g->node;
node->ident->name = g->name = cloneString("Out");
node->ident->length = OutLength;
gList->next = 0;

g = gList;
node = g->node;
node->ident->name = g->name = cloneString("R");
firstTime = TRUE;

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
	boolean didIt = TRUE;

	node->ident->length += 1;

	nextG = g->next;
	g->next = 0;

	if (slCount(g->elements) == 2)
	    continue;

	if (firstTime || (r -= SpeciesWt) < 0)
	    {
	    firstTime = FALSE;
	    speciate(&nextList, g);
	    }
	else if ((r -= DelWt) < 0)
	    {
	    didIt = delete(&nextList, g);
	    }
	else if ((r -= DupWt) < 0)
	    {
	    didIt = duplicate(&nextList, g);
	    }
	else if ((r -= InverseWt) < 0)
	    {
	    didIt = invert(&nextList, g);
	    }
	else if ((r -= NoWt) < 0)
	    {
	    slAddHead(&nextList, g);
	    }

	if (!didIt)
	    slAddHead(&nextList, g);
	}
    gList = nextList;

    if ( countLeaves(root) >= MaxGenomes)
	break;
    }

for(g = gList ; g; g = g->next)
    g->node->ident->length += 1;

removeAllStartStop(root);
outElementTrees(f, root);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int seed = time(NULL);
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
seed = optionInt( "seed", 0);
Trace = optionExists("trace");
InverseWt = optionInt( "InverseWt", 0);
InfSites = optionExists( "InfSites");
if (InfSites)
    InfHash = newHash(5);

if (0 == SpeciesWt + DupWt + InverseWt + DelWt)
    errAbort("must specify at least one weight");

srandom(seed);
synthElemTree(argv[1]);
return 0;
}

#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"
#include "dystring.h"

static char const rcsid[] = "$Id: elTreeBuild.c,v 1.3 2006/01/13 17:52:11 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "elTreeBuild - reconstruct element lists for species nodes given a rooted \n"
  "     tree with branch lengths and element lists on the leaves, an outgroup \n"
  "     element list with branch length, and one or more distance matrices \n"
  "usage:\n"
  "   elTreeBuild speciesTree outGroup elements distances outfile\n"
  "arguments:\n"
  "   speciesTree    file with species tree in Newick format\n"
  "   outGroup       outgroup name\n"
  "   elements       file with lists of elements for each species and outgroup\n"
  "   distances      file with distance matrices\n"
  "   outfile        output file with element tree\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};



void assignElements(struct phyloTree *tree, struct hash *genomeHash)
{
int ii;
struct genome *genome;

for(ii=0; ii < tree->numEdges; ii++)
    assignElements(tree->edges[ii], genomeHash);

if (tree->numEdges == 0)
    {
    if ((genome = hashFindVal(genomeHash, tree->ident->name)) == NULL)
	errAbort("can't find element list for %s\n",tree->ident->name);

    tree->priv = (void *)genome;
    genome->node = tree;
    }
}

struct phyloTree *findDupNode(struct phyloTree *node, double dist,  double *distLeft)
{
if (node->ident->length > dist)
    {
    *distLeft = dist;
    return node;
    }

if (node->parent == NULL)
    {
    struct phyloTree *newRoot;

    AllocVar(newRoot);
    phyloAddEdge(newRoot, node);
    node->parent = newRoot;
    node->ident->length = dist;
    *distLeft = 0;
    return newRoot;
    }

//printf("find up\n");
dist -= node->ident->length;
return findDupNode(node->parent, dist, distLeft);
}

void nameNodes(struct phyloTree *node)
{
int ii;
struct dyString *dy = newDyString(512);
struct genome *genome;

for(ii=0; ii < node->numEdges; ii++)
    nameNodes(node->edges[ii]);

if (node->numEdges && (node->ident->name == NULL))
    {
    dyStringPrintf(dy, "[");
    for(ii=0; ii < node->numEdges; ii++)
	{
	dyStringPrintf(dy, "%s",node->edges[ii]->ident->name);
	if (ii < node->numEdges - 1)
	    dyStringPrintf(dy, "+");
	}
    dyStringPrintf(dy, "]");

    node->ident->name = cloneString(dy->string);
    }

if ((genome = node->priv) == NULL)
    {
    //printf("new genome %s\n",node->ident->name);
    AllocVar(genome);
    genome->elementHash = newHash(5);
    genome->node = node;
    genome->name = node->ident->name;
    node->priv = genome;
    }
}


void fixDistances(struct element *root, struct element *e, double inDist, struct hash *distEleHash, struct distance **distanceList)
{
char buffer[512];
struct eleDistance *list;

//printf("fixDistaqnces: %s %g\n",eleName(e), inDist);
safef(buffer, sizeof(buffer), "%s.%s.%s", e->species, e->name, e->version);
//printf("looking for %s\n",buffer);

for (list = hashFindVal(distEleHash, buffer); list; list = list->next) 
    {
    struct element *other = NULL;
    struct distance *d2 = list->distance;
    double dist;

    if (d2->new)
	continue;
    if (d2->used)
	continue;
    d2->used = 1;
    if ((d2->e1->genome->node == NULL) )
	continue;
    if ((d2->e2->genome->node == NULL) )
	continue;

    //printf("found distance %g subDist %g\n",d2->distance,dist);
    dist = d2->distance - inDist;
    if (e == d2->e1)
	{
	other = d2->e2;
	if (other->parent)
	    {
	    printf("alread used\n");
	    continue;
	    }
	//printf("other second\n");
	}
    else if (e == d2->e2)
	{
	other = d2->e1;
	if (other->parent)
	    {
	    printf("alread used\n");
	    continue;
	    }
	//printf("other first\n");
	}
    else
	errAbort("not matching elements");
//printf("qwok dist %g: %s %s %x %x\n",d2->distance,eleName(d2->e1),eleName(d2->e2),d2->e1->parent,d2->e2->parent);
    //printf("new dist %g\n",dist);
    setElementDist(other, root, dist, distanceList);
    }
}


void assignElemNums(struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e;
int ii;

for(ii=0, e = g->elements; e ; ii++, e= e->next)
    e->count = ii;

for(ii=0; ii < node->numEdges; ii++)
    assignElemNums(node->edges[ii]);
}

void outElems(FILE *f, struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e;
int ii;

for(ii = 0, e = g->elements; e; ii++,e = e->next)
    ;
fprintf(f, ">%s %d %d %g\n",g->name, ii, node->numEdges, node->ident->length);

for(e = g->elements; e; e = e->next)
    {
    if (e->isFlipped)
	fprintf(f, "-");
    fprintf(f,"%s.%s %d ",e->name,e->version, (e->parent) ? e->parent->count + 1 : 0 );
    }
fprintf(f,"\n");

for(ii=0; ii < node->numEdges; ii++)
    outElems(f, node->edges[ii]);
}

void outElementTrees(FILE *f, struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e;
int ii;

assignElemNums(node);
outElems(f, node);
}

void fillParents(struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e = g->elements;
int ii;
char buffer[512];

for(ii=0; ii < node->numEdges; ii++)
    fillParents(node->edges[ii]);

if (node->parent == NULL)
    return;

for(e=g->elements; e ; e = e->next)
    if (!e->parent)
	{
	struct genome *pg = node->parent->priv;
	struct element *e1;

	//printf("filling parent for %s\n",eleName(e));
	AllocVar(e1);
	e1->genome = pg;
	e1->species = pg->name;
	e1->name = e->name;
	safef(buffer, sizeof(buffer), "%s(%s)",e->species,e->version); 
	e1->version = cloneString(buffer);
	//e1->version = e->version;
	eleAddEdge(e1, e);
	slAddHead(&pg->elements, e1);
	}

}

void checkTree(struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e = g->elements;
int ii;
struct phyloTree *parent;

for(ii=0; ii < node->numEdges; ii++)
    checkTree(node->edges[ii]);

if ((parent = node->parent) != NULL)
    {
    for(; e ; e= e->next)
	{
	for(ii=0; ii < e->numEdges; ii++)
	    {
	    struct genome *pg = e->parent->genome;
	    struct phyloTree *pNode = e->parent->genome->node;

	    if ( parent != pNode)
		{
		printf("checkTree: at node %s: element %s parent %s isn't node parent %s\n",node->ident->name,eleName(e),pNode->ident->name, parent->ident->name);
		}
	    }
	}
    }
}


void elTreeBuild(char *speciesTreeName,char *outGroupName,char *genomeFileName,
    char *distanceFileName, char *outFileName)
{
FILE *f = mustOpen(outFileName, "w");
struct lineFile *lf = lineFileOpen(speciesTreeName, FALSE);
struct phyloTree *speciesTree = phyloReadTree(lf);
struct phyloTree *node, *newRoot = speciesTree;
struct hash *elementHash;
struct distance *distanceList;
struct genome *genomeList, *genome;
struct distance *d;
int numSpecies = 0;
struct hash *genomeHash = newHash(5);
char **genomeNames;
int ii;
int numExpect;
double *distanceArray;
double dist1, dist2;
char buffer[512];

lineFileClose(&lf);
nameNodes(speciesTree);

genome = genomeList = readGenomes(genomeFileName);
for(genome=genomeList; genome; genome=genome->next)
    numSpecies++;

genomeNames = (char **)needMem(numSpecies * sizeof(char *));
for(genome=genomeList, ii=0; genome; ii++,genome=genome->next)
    {
    hashAdd(genomeHash, genome->name, (void *)genome);
    genomeNames[ii] = genome->name;
    }

assignElements(speciesTree, genomeHash);

for(; genome; genome = genome->next)
    {
    if (( node = phyloFindName(speciesTree, genome->name)) == NULL)
	errAbort("can't find leave node for %s\n",genome->name);
    }

distanceList = readDistances(distanceFileName, genomeHash);

/*
for(d= distanceList; d; d=d->next)
    if (!sameString(d->e1->species,d->e2->species))
	{
	printf("del\n");
	d->used = 1;
	}
	*/


for(d = distanceList; d ; d = d->next)
    {
    struct phyloTree *node1, *node2, *mergeNode;
    double dist;
    double distLeft1, distLeft2;
    double distTook;
    char buffer[512];

    if (d->used)
	continue;

    node1 = d->e1->genome->node;
    node2 = d->e2->genome->node;

    if (!(node1 && node2))
	continue;

    dist = d->distance;

 //   printf("dist %g: %s %s\n",d->distance,d->e1->genome->name,d->e2->genome->name);
    if (node1 == node2)
	mergeNode = node1;
    else
	{
	phyloClearTreeMarks(newRoot);
	phyloMarkUpTree(node1);
	if ((mergeNode = phyloFindMarkUpTree(node2)) == NULL)
	    errAbort("can't find merge node");

	for(;node1 != mergeNode; node1 = node1->parent)
	    dist -= node1->ident->length;
	for(;node2 != mergeNode; node2 = node2->parent)
	    dist -= node2->ident->length;

	if (dist < 0.0)
	    errAbort("mergeNode is too far %g\n",dist);
//	printf("mergnoe %g\n",dist);

	}

    for(node = mergeNode; node->parent && ((dist/2) >= node->ident->length); node=node->parent)
	dist -= node->ident->length*2;

 //   printf("outloop dist %g\n",dist);
    if (dist == 0.0)
	{
	}
    else if (node->parent == NULL)
	{
	//printf("new UR %s.%s.%s and %s.%s.%s\n",d->e1->species, d->e1->name, d->e1->version,d->e2->species, d->e2->name, d->e2->version);

	AllocVar(newRoot);
	AllocVar(newRoot->ident);
	safef(buffer, sizeof(buffer), "UR_%s",node->ident->name) ;
	newRoot->ident->name = cloneString(buffer);
	phyloAddEdge(newRoot, node);
	node->parent = newRoot;
	node->ident->length = dist/2;
 //   phyloDebugTree(newRoot, stdout);
	}
    else
	{
	struct phyloTree *newNode;

	AllocVar(newNode);
	AllocVar(newNode->ident);
	safef(buffer, sizeof(buffer), "D_%s",node->ident->name) ;
	newNode->ident->name = cloneString(buffer);
	phyloDeleteEdge(node->parent, node);
	phyloAddEdge(node->parent, newNode);
	phyloAddEdge( newNode, node);

	//newNode->parent = node->parent;
	//node->parent = newNode;
//	printf("new Node old dist was %g\n",node->ident->length);
	newNode->ident->length = dist/2;
	node->ident->length -= dist/2;
//	printf("new dist  %g\n",newNode->ident->length + node->ident->length);
	}

    }
nameNodes(newRoot);
phyloDebugTree(newRoot, stdout);
// second pass
//printf("second pass\n");
do
    {
    extern int sortByDistance(const void *p1, const void *p2);
    //printf("re-sorting\n");
    slSort(&distanceList, sortByDistance);
    for(d = distanceList; d ; d = d->next)
	{
	if (!d->used)
//	printf("%s %s at %g\n",cloneString(eleName(d->e1)),cloneString(eleName(d->e2)),d->distance);
	d->new = 0;
	}
    for(d = distanceList; d ; d = d->next)
	{
	struct element *topE1, *topE2;
	struct phyloTree *node1, *node2, *mergeNode;
	double dist;
	double distLeft1, distLeft2;
	double distTook;

	if (d->used)
	    continue;
	d->used = 1;
	node1 = d->e1->genome->node;
	node2 = d->e2->genome->node;

	if (!(node1 && node2))
	    continue;

	if (d->e1 == d->e2)
	    {
	    errAbort("same element\n");
	    continue;
	    }

	//printf("dist %g: %s %s used %x %x\n",d->distance,eleName(d->e1),eleName(d->e2),d->e1->parent, d->e2->parent);
	if (d->e1->parent || d->e2->parent)
	    {
	    errAbort("not happen");
	    continue;
	    }

	dist = d->distance;

	topE1 = d->e1;
	topE2 = d->e2;
	distTook = 0.0;
	dist1 = dist2 = 0.0;
	if (node1 == node2)
	    {
	    mergeNode = node1;
	    }
	else
	    {
	    phyloClearTreeMarks(newRoot);
	    phyloMarkUpTree(node1);
	    if ((mergeNode = phyloFindMarkUpTree(node2)) == NULL)
		errAbort("can't find merge node");

//	    if ((mergeNode == node1) || (mergeNode == node2))
//		{
		//doit = TRUE;
//		printf("mergeNode is parent\n");
	//	continue;
//		}
	    for(;node1 != mergeNode; node1 = node1->parent)
		{
		struct genome *g = node1->priv;
		struct element *e;
//		printf("merge node %s\n",mergeNode->ident->name);
		dist1 += node1->ident->length;
		if (node1 == d->e1->genome->node)
		    continue;
		AllocVar(e);
		e->genome = g;
		safef(buffer, sizeof(buffer), "%s(%s)",topE1->species,topE1->version); 
		e->species = g->name;
		e->name = d->e1->name;
		e->version = cloneString(buffer);
//		printf("add node1 element %s\n",eleName(e));
		//e->version = d->e1->version;
		eleAddEdge(e, topE1);
		slAddHead(&g->elements, e);
		topE1 = e;
		}
	    for(;node2 != mergeNode; node2 = node2->parent)
		{
		struct genome *g = node2->priv;
		struct element *e;
		dist2 += node2->ident->length;
		if (node2 == d->e2->genome->node)
		    continue;
		AllocVar(e);
//		printf("add node2 element %s\n",g->name);
		e->genome = g;
		e->species = g->name;
		safef(buffer, sizeof(buffer), "%s(%s)",topE2->species,topE2->version); 
		e->name = d->e2->name;
		e->version = cloneString(buffer);
		eleAddEdge(e, topE2);
		slAddHead(&g->elements, e);
		topE2 = e;
		}

	    dist -= dist1 + dist2;
	    if (dist < 0.0)
		errAbort("smergeNode is too far %g\n",dist);
	    if (dist > 0.0)
		{
		struct genome *g = mergeNode->priv;
		struct element *e;
		AllocVar(e);
		e->genome = g;
		safef(buffer, sizeof(buffer), "%s(%s)",topE1->species,topE1->version); 
		e->species = g->name;
		e->name = d->e1->name;
		e->version = cloneString(buffer);
//		printf("extra add node1 element %s\n",eleName(e));
		//e->version = d->e1->version;
		eleAddEdge(e, topE1);
		slAddHead(&g->elements, e);
		topE1 = e;
		}
	    ////printf("mergnoe %g\n",dist);

	    }

	for(node = mergeNode; node->parent && ((dist/2) >= node->ident->length); node=node->parent)
	    {
	    struct genome *g = node->priv;
	    struct element *e;

	    distTook += node->ident->length, dist -= node->ident->length*2;

	    //if (((dist1 != 0.0) || (dist2 != 0.0)) && ( node == mergeNode))
	    if ( node == mergeNode)
		continue;
	    AllocVar(e);
	    e->genome = g;
	    e->species = g->name;
	    e->name = d->e1->name;
	    //e->version = d->e1->version;
	    safef(buffer, sizeof(buffer), "%s(%s)",topE1->species,topE1->version); 
	    e->version = cloneString(buffer);
	    //printf("adding %s\n",eleName(e));
	    eleAddEdge(e, topE1);
	    slAddHead(&g->elements, e);
	    topE1 = e;

	    AllocVar(e);
	    e->genome = g;
	    e->species = g->name;
	    e->name = d->e2->name;
	    e->version = d->e2->version;
	    eleAddEdge(e, topE2);
	    slAddHead(&g->elements, e);
	    topE2 = e;

	    }

	//printf("dis2t %g: %s %s used %x %x\n",d->distance,d->e1->genome->name,d->e2->genome->name,d->e1->parent, d->e2->parent);
	//printf("outloop distTook %g d1 %g d2 %g\n",distTook,dist1,dist2);
	if (dist == 0.0)
	    {
	    struct genome *genome = node->priv;
	    struct element *e;
	    struct eleDistance *ed, *list;
	    char buffer[512];
	    extern struct hash *distEleHash;

	    //printf("found dup node\n");

	    AllocVar(e);
	    e->genome = genome;
	    e->species = genome->name;
	    if (!sameString(d->e1->name, d->e2->name))
		errAbort("merging elements with different names");

	    e->name = d->e1->name;

	    if (sameString(topE1->species, topE2->species))
		safef(buffer, sizeof(buffer), "%s(%s+%s)",topE1->species,topE1->version,topE2->version); 
	    else
		safef(buffer, sizeof(buffer), "(%s(%s)+%s(%s))",topE1->species,topE1->version,topE2->species,topE2->version); 
	    e->version = cloneString(buffer);

	    //printf("new element %s.%s.%s genome %s\n",e->species, e->name, e->version,genome->name);

	    slAddHead(&genome->elements, e);
	    eleAddEdge(e, topE1);
	    eleAddEdge(e, topE2);

	    //printf("now add\n");
	    /* now add new distances to table */
	    fixDistances(e, d->e1, distTook + dist1, distEleHash, &distanceList);
	    fixDistances(e, d->e2, distTook + dist2, distEleHash, &distanceList);

	    //printf("breaking\n");
	    break;
	    }
	else
	    errAbort("not finding dup node %g",dist);
	}
    } while(d);

    fillParents(newRoot);
    printElementTrees(newRoot, 0);

    outElementTrees(f, newRoot);

    checkTree(newRoot);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();

elTreeBuild(argv[1],argv[2],argv[3],argv[4], argv[5]);
return 0;
}

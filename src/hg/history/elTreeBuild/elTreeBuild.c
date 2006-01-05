#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: elTreeBuild.c,v 1.1 2006/01/05 16:57:00 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "elTreeBuild - reconstruct element lists for species nodes given a rooted \n"
  "     tree with branch lengths and element lists on the leaves, an outgroup \n"
  "     element list with branch length, and one or more distance matrices \n"
  "usage:\n"
  "   elTreeBuild speciesTree outGroup elements distances\n"
  "arguments:\n"
  "   speciesTree    file with species tree in Newick format\n"
  "   outGroup       outgroup name\n"
  "   elements       file with lists of elements for each species and outgroup\n"
  "   distances      file with distance matrices\n"
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

printf("find up\n");
dist -= node->ident->length;
return findDupNode(node->parent, dist, distLeft);
}

void elTreeBuild(char *speciesTreeName,char *outGroupName,char *genomeFileName,char *distanceFileName)
{
//FILE *f = mustOpen(outFileName, "w");
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

lineFileClose(&lf);

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

for(d = distanceList; d ; d = d->next)
    {
    struct phyloTree *node1, *node2, *mergeNode;
    double dist;
    double distLeft1, distLeft2;
    double distTook;

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
	    errAbort("mergeNode is too far\n");
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
//	printf("new UR\n");
	AllocVar(newRoot);
	AllocVar(newRoot->ident);
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
phyloDebugTree(newRoot, stdout);
// second pass
printf("second pass\n");
for(d = distanceList; d ; d = d->next)
    {
    struct phyloTree *node1, *node2, *mergeNode;
    double dist;
    double distLeft1, distLeft2;
    double distTook;

    node1 = d->e1->genome->node;
    node2 = d->e2->genome->node;

    if (!(node1 && node2))
	continue;

    printf("dist %g: %s %s used %d %d\n",d->distance,d->e1->genome->name,d->e2->genome->name,d->e1->used, d->e2->used);
    if (d->e1->used || d->e2->used)
	continue;

    dist = d->distance;

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
	    errAbort("mergeNode is too far\n");
	////printf("mergnoe %g\n",dist);

	}

    for(node = mergeNode; node->parent && ((dist/2) >= node->ident->length); node=node->parent)
	dist -= node->ident->length*2;

    //printf("outloop dist %g\n",dist);
    if (dist == 0.0)
	{
	struct genome *genome;
	struct element *e;

	printf("found dup node\n");
	if ((genome = node->priv) == NULL)
	    {
	    printf("new genome\n");
	    AllocVar(genome);
	    genome->elementHash = newHash(5);
	    genome->node = node;
	    genome->name = node->ident->name;
	    node->priv = genome;
	    }

	AllocVar(e);
	e->genome = genome;
	e->species = genome->name;
	e->name = d->e1->name;
	e->version = d->e1->version;

	slAddHead(&genome->elements, e);
	eleAddEdge(e, d->e1);
	eleAddEdge(e, d->e2);

	d->e1->used = 1;
	d->e2->used = 1;
	}
    else
	errAbort("not finding dup node");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();

elTreeBuild(argv[1],argv[2],argv[3],argv[4]);
return 0;
}

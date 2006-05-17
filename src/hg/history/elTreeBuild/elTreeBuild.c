#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"
#include "dystring.h"

static char const rcsid[] = "$Id: elTreeBuild.c,v 1.9 2006/05/17 15:12:32 braney Exp $";

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
  "options:\n"
  "   -names         genomes are created using special names\n"
  );
}

static struct optionSpec options[] = {
   {"names", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean SpecialNames;

void assignElements(struct phyloTree *tree, struct hash *genomeHash)
{
int ii;
struct genome *genome;

for(ii=0; ii < tree->numEdges; ii++)
    assignElements(tree->edges[ii], genomeHash);

tree->ident->length = round(tree->ident->length);
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

if (node->numEdges) // && (node->ident->name == NULL))
    {
    if (SpecialNames)
	{
	dyStringPrintf(dy, "%s",node->edges[0]->ident->name);
	dy->string[strlen(dy->string) - 1 ] = 0;
	//printf("building name with child %s got %s\n",node->edges[0]->ident->name, dy->string);
	}
    else
	{
	dyStringPrintf(dy, "[");
	for(ii=0; ii < node->numEdges; ii++)
	    {
	    dyStringPrintf(dy, "%s",node->edges[ii]->ident->name);
	    if (ii < node->numEdges - 1)
		dyStringPrintf(dy, "+");
	    }
	dyStringPrintf(dy, "]");
	}

    node->ident->name = cloneString(dy->string);
    }

if ((genome = node->priv) == NULL)
    {
    AllocVar(genome);
    genome->elementHash = newHash(5);
    genome->node = node;
    node->priv = genome;
    }

genome->name = node->ident->name;
//if (!sameString(genome->name, node->ident->name))
 //   errAbort("not same strings");
}


char buffer[16*1024];

boolean isAncestor(struct phyloTree *node1,struct phyloTree *node2)
{
if (node1 == node2)
    return TRUE;

if (node2->parent == NULL)
    return FALSE;

return isAncestor(node1, node2->parent);
}

void fixDistances(struct element *root, struct element *e, double inDist, 
    struct hash *distHash, struct hash *distElemHash, struct distance **distanceList)
{
//char buffer[512];
struct eleDistance *list;

if (distElemHash == NULL)
    return;
//printf("fixDistaqnces: %s %g\n",eleName(e), inDist);
safef(buffer, sizeof(buffer), "%s.%s.%s", e->species, e->name, e->version);
//printf("looking for %s\n",buffer);

for (list = hashFindVal(distElemHash, buffer); list; list = list->next) 
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
	//errAbort("e1 is null %s",d2->e1->genome->name);
	continue;
    if ((d2->e2->genome->node == NULL) )
	//errAbort("e2 is null %s",d2->e2->genome->name);
	continue;

    //printf("found distance %g subDist %g\n",d2->distance,dist);
    if (d2->e1 == d2->e2)
	errAbort("distance with same element");

    dist = d2->distance - inDist;
    if (e == d2->e1)
	{
	other = d2->e2;
	if (other->parent)
	    {
	    //printf("alread used\n");
	    continue;
	    }
	//printf("other second\n");
	}
    else if (e == d2->e2)
	{
	other = d2->e1;
	if (other->parent)
	    {
	    //printf("alread used\n");
	    continue;
	    }
	//printf("other first\n");
	}
    else
	errAbort("not matching elements");
    /*
    if (isAncestor(other->genome->node, root->genome->node))
	{
	printf("isAncestor2k\n");
	//dist += 2 * root->genome->node->ident->length;
	//continue;
	}

    if (isAncestor(root->genome->node, other->genome->node))
	{
	printf("isAncestor\n");
	//dist += 2 * root->genome->node->ident->length;
	//continue;
	}
	*/

//printf("qwok dist %g: %s %s %x %x\n",d2->distance,eleName(d2->e1),eleName(d2->e2),d2->e1->parent,d2->e2->parent);
//printf("new dist %g: %s %s %x %x\n",dist,eleName(other),eleName(root),d2->e1->parent,d2->e2->parent);
    setElementDist(other, root, dist, distanceList, &distHash, &distElemHash);
    }
}


boolean fillParents(struct phyloTree *node, int depth, int wantDepth, struct hash *distHash, struct hash *distElemHash, struct distance **pdistanceList)
{
struct genome *g = node->priv;
struct element *e = g->elements;
int ii;
boolean did = FALSE;
//char buffer[512];

///printf("fillParents %d want %d\n",depth,wantDepth);

if (depth < wantDepth)
    {
    for(ii=0; ii < node->numEdges; ii++)
	if (fillParents(node->edges[ii], depth + 1, wantDepth, distHash, distElemHash, pdistanceList))
	    return TRUE;
    return FALSE;
    }

if (node->parent == NULL)
    return FALSE;

///printf("in there\n");
for(e=g->elements; e ; e = e->next)
    {
    if (e->parent == NULL)
	{
	struct genome *pg = node->parent->priv;
	struct element *e1;

	did = TRUE;
	//printf("filling parent for %s\n",eleName(e));
	//safef(buffer, sizeof(buffer), "%s(%s)",e->species,e->version); 
	//e1 = newElement(pg, e->name, cloneString(nextVersion()));
	e1 = newElement(pg, e->name, cloneString(e->version));
	eleAddEdge(e1, e);
	    ///printf("bringing up %s to %s.%s\n",eleName(e),pg->name,buffer);
	    fixDistances(e1, e, node->ident->length, distHash, distElemHash, pdistanceList);
	    return TRUE;
	}
    }
    return FALSE;
}

void checkTree(struct phyloTree *node)
{
struct genome *g = node->priv;
struct element *e = g->elements;
int ii;
struct phyloTree *parent;

for(ii=0; ii < node->numEdges; ii++)
    checkTree(node->edges[ii]);

if (node->parent)
    {
    if (SpecialNames)
	{
	if (node->parent->numEdges == 1)
	    {
	    if ((node->ident->name[strlen(node->ident->name) - 1 ] != 'D') )
		printf("bad treed %s \n",node->ident->name);
	    }
	else if ((node->ident->name[strlen(node->ident->name) - 1 ] != '0') &&(node->ident->name[strlen(node->ident->name) - 1 ] != '1') )
	    printf("bad treey %s\n",node->ident->name);
	}
    }

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



void initElementDist(struct phyloTree *node, struct hash *hash)
{
int ii;

for(ii=0; ii < node->numEdges; ii++)
    initElementDist(node->edges[ii], hash);

if (node->numEdges == 0)
    {
    struct genome *g = node->priv;
    struct element *e;
    char buffer[512];
    struct eleDistance *list;

    for(e=g->elements; e; e= e->next)
	{
	safef(buffer, sizeof(buffer), "%s.%s.%s", e->species, e->name, e->version);
	list = hashFindVal(hash, buffer);

	e->dist = list;
	}
    }
}


void elTreeBuild(char *speciesTreeName,char *outGroupName,char *genomeFileName,
    char *distanceFileName, char *outFileName)
{
int wantDepth = 40;
extern int sortByDistance(const void *p1, const void *p2);
FILE *f = mustOpen(outFileName, "w");
struct lineFile *lf = lineFileOpen(speciesTreeName, FALSE);
struct phyloTree *speciesTree = phyloReadTree(lf);
struct phyloTree *node, *newRoot = speciesTree;
struct hash *elementHash;
struct distance *distanceList;
struct genome *genomeList, *genome, *g;
struct distance *d;
int numSpecies = 0;
struct hash *genomeHash = newHash(5);
char **genomeNames;
int ii;
int numExpect;
double *distanceArray;
double dist1, dist2;
//char buffer[512];
struct hash *distHash = NULL;
struct hash *distElemHash = NULL;

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
	errAbort("can't find leaf node for %s\n",genome->name);
    }

distanceList = readDistances(distanceFileName, genomeHash,
    &distHash, &distElemHash);

if (SpecialNames)
    removeXs(genomeList, 'I');
for(d = distanceList; d ; d = d->next)
    {
    struct phyloTree *node1, *node2, *mergeNode;
    double dist;
    double distLeft1, distLeft2;
    double distTook;
    //char buffer[512];

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

	if (dist < -0.004)
	    errAbort("mergeNode is too far %g\n",dist);
//	printf("mergnoe %g\n",dist);

	}

    for(node = mergeNode; node->parent && ((dist/2) >= node->ident->length); node=node->parent)
	dist -= node->ident->length*2;

    dist = round( dist) ;

    if (dist == 0.0)
	{
	}
    else if (node->parent == NULL)
	{
//	printf("new UR %s.%s.%s and %s.%s.%s\n",d->e1->species, d->e1->name, d->e1->version,d->e2->species, d->e2->name, d->e2->version);

	AllocVar(newRoot);
	AllocVar(newRoot->ident);
	if (SpecialNames)
	    {
	    safef(buffer, sizeof(buffer), "%s",node->ident->name) ;
	    buffer[strlen(buffer) - 1] = 0;
	    }
	else
	    safef(buffer, sizeof(buffer), "UR_%s",node->ident->name) ;
	newRoot->ident->name = cloneString(buffer);
	AllocVar(g);
	g->name = newRoot->ident->name;
	newRoot->priv = g;
	g->node = newRoot;
	phyloAddEdge(newRoot, node);
	node->parent = newRoot;
	node->ident->length = dist/2;
	//printf("dist %g\n",node->ident->length);
	}
    else
	{
	struct phyloTree *newNode;

	AllocVar(newNode);
	AllocVar(newNode->ident);
	if (SpecialNames)
	    {
	    safef(buffer, sizeof(buffer), "%s",node->ident->name) ;
	    buffer[strlen(buffer) - 1] = 0;
	    }
	else
	    safef(buffer, sizeof(buffer), "D_%s",node->ident->name) ;
	newNode->ident->name = cloneString(buffer);
	AllocVar(g);
	g->name = newNode->ident->name;
	newNode->priv = g;
	g->node = newNode;

	phyloDeleteEdge(node->parent, node);
	phyloAddEdge(node->parent, newNode);
	phyloAddEdge( newNode, node);

	newNode->ident->length = node->ident->length - dist/2;
	node->ident->length = dist/2;
	//printf("new dup dist %g\n",node->ident->length);
//	printf("new dist  %g\n",newNode->ident->length + node->ident->length);
	}

    }
if (SpecialNames)
    {
    nameNodes(newRoot);
    checkTree(newRoot);
 //   phyloDebugTree(newRoot, stdout);
    }

//phyloDebugTree(newRoot, stdout);
//if (SpecialNames)
    //exit(0);

//slSort(&distanceList, sortByDistance);
//initElementDist(newRoot, distElemHash);
//growUpTree(newRoot);


// second pass
//printf("second pass\n");
top:
//printf("back at top\n");
do
    {
    //printf("re-sorting\n");
    slSort(&distanceList, sortByDistance);
    for(d = distanceList; d ; d = d->next)
	{
	if (!d->used)
//	printf("%s %s at %g\n",cloneString(eleName(d->e1)),cloneString(eleName(d->e2)),d->distance);
	d->new = 0;
	}
    //printElementTrees(newRoot, 0);
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
	    {
	    //printf("node1 %s node 2 %s\n",d->e1->genome->name,d->e2->genome->name);
	    //printf("null nodes\n");
	    continue;
	    }

	if (d->e1 == d->e2)
	    {
	    errAbort("same element\n");
	    continue;
	    }

	if (d->e1->genome->node->parent != d->e2->genome->node->parent)
	    {
	    d->used = 0;
	    //printf("not same parent\n");
	    //printf("%s %s\n",d->e1->genome->node->parent->ident->name,d->e2->genome->node->parent->ident->name);
//	printf("5dist %g: %s %s used %x %x\n",d->distance,eleName(d->e1),eleName(d->e2),d->e1->parent, d->e2->parent);
	    continue;
	    }
	else if (d->e1->genome->node->ident->length + d->e2->genome->node->ident->length != d->distance )
	    {
	    d->used = 0;
	    //printf("not right distance \n");
	    continue;
	/*
	    struct genome *g = node1->parent->priv;
	    struct element *e;

	printf("4dist %g: %s %s used %x %x\n",d->distance,eleName(d->e1),eleName(d->e2),d->e1->parent, d->e2->parent);
	    printf("same parents but not right distance");
	    safef(buffer, sizeof(buffer), "%s(%s)",d->e1->species,d->e1->version); 
	    e = newElement(g, d->e1->name, cloneString(buffer));
	    eleAddEdge(e, d->e1);
	    fixDistances(e, d->e1, node1->ident->length, distHash, distElemHash, &distanceList);
	    printf("add new elemtn %s to %s\n",buffer, g->name);
	    d->e1 = e;


	    safef(buffer, sizeof(buffer), "%s(%s)",d->e2->species,d->e2->version); 
	    e = newElement(g, d->e2->name, cloneString(buffer));
	    eleAddEdge(e, d->e2);
	    fixDistances(e, d->e2, node2->ident->length, distHash, distElemHash, &distanceList);
	    printf("add new elemtn %s to %s\n",buffer, g->name);
	    d->e2 = e;


	    d->used = 0;
	    d->distance -= 2 * node1->ident->length;
	    printf("breaking\n");
	    break;
	    */
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

	    if ((node1 == mergeNode) || (node2 == mergeNode))
		{
		//printf("merging merged node\n");
		d->used = 0;
		continue;
		//mergeNode = mergeNode->parent;
		}

	    for(;node1 != mergeNode; node1 = node1->parent)
		{
		struct genome *g = node1->priv;
		struct element *e;
//		printf("merge node %s\n",mergeNode->ident->name);
		dist1 += node1->ident->length;
		if (node1 == d->e1->genome->node)
		    continue;
		//safef(buffer, sizeof(buffer), "%s(%s)",topE1->species,topE1->version); 
		//safef(buffer, sizeof(buffer), "%s0",topE2->version); 
		//e = newElement(g, d->e1->name, cloneString(nextVersion()));
		e = newElement(g, d->e1->name, cloneString(d->e1->version));
		//printf("add node1 element %s\n",g->name);
		eleAddEdge(e, topE1);
		topE1 = e;
		}
	    for(;node2 != mergeNode; node2 = node2->parent)
		{
		struct genome *g = node2->priv;
		struct element *e;
		dist2 += node2->ident->length;
		if (node2 == d->e2->genome->node)
		    continue;
		//safef(buffer, sizeof(buffer), "%s(%s)",topE2->species,topE2->version); 
		//safef(buffer, sizeof(buffer), "%s1",topE2->version); 
		//e = newElement(g, d->e2->name, cloneString(nextVersion()));
		e = newElement(g, d->e2->name, cloneString(d->e2->version));
		//printf("add node2 element %s\n",g->name);
		eleAddEdge(e, topE2);
		topE2 = e;
		}

	    dist -= dist1 + dist2;
	    if (dist < 0.0)
		errAbort("smergeNode is too far %g\n",dist);
	    if (dist > 0.0)
		{
		struct genome *g = mergeNode->priv;
		struct element *e;
		//safef(buffer, sizeof(buffer), "%s(%s)",topE1->species,topE1->version); 
		if (!sameString(d->e1->name, d->e2->name))
		    errAbort("should be same name");
		//e = newElement(g, d->e1->name, cloneString(nextVersion()));
		e = newElement(g, d->e1->name, cloneString(d->e1->version));
		//printf("extra add node1 element %s\n",eleName(e));
		eleAddEdge(e, topE1);
		topE1 = e;
		}

	    }

	for(node = mergeNode; node->parent && ((dist/2) >= node->ident->length); node=node->parent)
	    {
	    struct genome *g = node->priv;
	    struct element *e;

	    if (((dist1 != 0.0) || (dist2 != 0.0)))
		errAbort("joining off species node %g %g\n", dist1, dist2);
	    distTook += node->ident->length;
	    dist -= node->ident->length*2;

	    //if (((dist1 != 0.0) || (dist2 != 0.0)) && ( node == mergeNode))
	    if ( node == mergeNode)
		continue;

	    //printf("bot %g \n",node->ident->length);
	    //safef(buffer, sizeof(buffer), "%s(%s)",topE1->species,topE1->version); 
	    //safef(buffer, sizeof(buffer), "%s0",topE1->version); 
	    //e = newElement(g, d->e1->name, cloneString(nextVersion()));
	    e = newElement(g, d->e1->name, cloneString(d->e1->version));
	    //printf("adding %s\n",eleName(e));
	    eleAddEdge(e, topE1);
	    topE1 = e;

	    //safef(buffer, sizeof(buffer), "%s(%s)",topE2->species,topE2->version); 
	    //safef(buffer, sizeof(buffer), "%s1",topE2->version); 
	    //e = newElement(g, d->e2->name, cloneString(nextVersion()));
	    e = newElement(g, d->e2->name, cloneString(d->e2->version));
	    //printf("adding %s\n",eleName(e));
	    eleAddEdge(e, topE2);
	    topE2 = e;

	    }

	//printf("dis2t %g: %s %s used %x %x\n",d->distance,d->e1->genome->name,d->e2->genome->name,d->e1->parent, d->e2->parent);
	//printf("outloop distTook %g d1 %g d2 %g\n",distTook,dist1,dist2);
	if (dist == 0.0)
	    {
	    struct genome *genome = node->priv;
	    struct element *e;
	    struct eleDistance *ed, *list;
	    //char buffer[512];
	    //extern struct hash *distEleHash;

	    //printf("found dup node\n");

	//printf("merge to %s\n",genome->name);
	    if (!sameString(d->e1->name, d->e2->name))
		errAbort("merging elements with different names");

	    /*
	    if (sameString(topE1->species, topE2->species))
		safef(buffer, sizeof(buffer), "%s(%s+%s)",topE1->species,topE1->version,topE2->version); 
	    else
		safef(buffer, sizeof(buffer), "(%s(%s)+%s(%s))",topE1->species,topE1->version,topE2->species,topE2->version); 
		*/
	    //e = newElement(genome, d->e1->name, cloneString(nextVersion()));
	    e = newElement(genome, d->e1->name, cloneString(d->e1->version));
	    if (genome->node->numEdges == 1)
		*strrchr(e->version, '.') = 0;
	    //e->version = cloneString(buffer);

	    //printf("new element %s.%s.%s genome %s\n",e->species, e->name, e->version,genome->name);

	    //slAddHead(&genome->elements, e);
	    eleAddEdge(e, topE1);
	    eleAddEdge(e, topE2);

	    //printf("now add\n");
	    /* now add new distances to table */
	    fixDistances(e, d->e1, distTook + dist1, distHash, distElemHash, &distanceList);
	    fixDistances(e, d->e2, distTook + dist2, distHash, distElemHash, &distanceList);

	    //printf("breaking\n");
	    break;
	    }
	else
	    errAbort("not finding dup node dist %g node %s",dist,node->ident->name);
	}
    } while(d);

    while(wantDepth)
	{
	if (fillParents(newRoot, 0, wantDepth, distHash, distElemHash, &distanceList))
	    goto top;
	wantDepth--;
	}
//    printElementTrees(newRoot, 0);

    outElementTrees(f, newRoot);

    checkTree(newRoot);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();

SpecialNames = optionExists("names");

elTreeBuild(argv[1],argv[2],argv[3],argv[4], argv[5]);
return 0;
}

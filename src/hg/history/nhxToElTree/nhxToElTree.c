#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "psGfx.h"
#include "element.h"
#include "chromColors.h"

static char const rcsid[] = "$Id: nhxToElTree.c,v 1.2 2007/07/27 21:49:06 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "nhxToElTree - convert an NHX format reconciled gene tree to\n"
  "   an element tree format file\n"
  "usage:\n"
  "   nhxToElTree species.tree nhx.tree out.eltree\n"
  "arguments:\n"
  "   species.tree   NH format species tree.\n"
  "   nhx.tree       NHX format reconciled gene tree.\n"
  "   out.eltree     element tree with single element at root\n"
  "options:\n"
  "   -prune         prune dead branches\n"
  );
}

static struct optionSpec options[] = {
   {"prune", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean doPrune = FALSE;

struct reconPriv
{
struct slName *childList;
};

struct elementPriv
{
double x, y;
int index;
};

struct genomePriv
{
int number;
struct hash *childHash;
double x, y;
double xmaj, ymaj;
};

#define MAX(a,b)	(((a) > (b)) ? (a) : (b)) 

void checkTree(struct phyloTree *tree)
{
if (tree->parent)
    {
    if (tree->parent->priv == tree->priv)
	errAbort("foobar");
    }

int ii;
for(ii=0; ii < tree->numEdges; ii++)
    checkTree(tree->edges[ii]);
}
void buildGenomeTree(struct phyloTree *tree, int *lowNumber, 
    struct hash *leafHash, struct slName **childList)
{
struct genome *g;
int ii;
char buf[4096];
struct genomePriv *gp;

buf[0] = 0;
AllocVar(g);
tree->priv = g;
g->node = tree;
AllocVar(gp);
g->priv = gp;
gp->childHash = newHash(8);

if (tree->numEdges == 0)
    {
    struct slName *name;

    assert(tree->ident->name != NULL);
    g->name = cloneString(tree->ident->name);
    hashAddUnique(leafHash, g->name, g);
    hashStore(gp->childHash, g->name);
    gp->number = *lowNumber;
    *lowNumber = *lowNumber + 1;

    name = newSlName(g->name);
    slAddHead(childList, name);
    return;
    }

assert( tree->numEdges == 2);
assert(tree->ident->name == NULL);

struct slName *myChildren = NULL;
for(ii=0; ii < tree->numEdges ; ii++)
    buildGenomeTree(tree->edges[ii], lowNumber, leafHash, &myChildren);

struct slName *name = myChildren;
for(; name; name = name->next)
    {
    hashStore(gp->childHash, name->name);
    strcat(buf, name->name);
    strcat(buf, "/");
    }

buf[strlen(buf) - 1] = 0;
g->name = cloneString(buf);
gp->number = *lowNumber;
*lowNumber = *lowNumber + 1;
tree->ident->name = g->name;

slCat(childList, myChildren);
}

struct genome *genomeWithList(struct genome *root, struct slName *list)
{
struct slName *iter = list;
struct genomePriv *gp = root->priv;
struct hash *childHash = gp->childHash;
struct phyloTree *tree = root->node;

if (childHash == NULL)
    return NULL;

for(; iter; iter = iter->next)
    {
    if (hashLookup(childHash, iter->name) == NULL)
	return NULL;
    }

int ii;
for(ii=0; ii < tree->numEdges; ii++)
    {
    struct genome *g = tree->edges[ii]->priv;

    if ((g = genomeWithList(g, list)) != NULL)
	return g;
    }

return root;
}

boolean hasHigherGenome(struct phyloTree *tree, int number)
{
struct genome *g = tree->priv;

if (g)
    {
    struct genomePriv *gp = g->priv;

    if (number <= gp->number)
	return TRUE;
    }


int ii;

for(ii=0; ii < tree->numEdges; ii++)
    if (hasHigherGenome(tree->edges[ii], number))
	return TRUE;

return FALSE;
}


void labelReconTree(struct phyloTree *tree, struct hash *leafHash,
    struct slName **childList, struct genome *rootGenome)
{
int ii;

if (tree->numEdges == 0) // if this is a leaf node
    {
    char *species = cloneString(tree->ident->name);
    struct slName *name;
    *strchr(species, '.') = 0;

    name = newSlName(species);

    slAddHead(childList, name);

    struct genome *g = hashMustFindVal(leafHash, species);

    tree->priv = g;
    return;
    }

struct slName *myChildren = NULL;

for(ii=0; ii < tree->numEdges ; ii++)
    labelReconTree(tree->edges[ii], leafHash, &myChildren, rootGenome);

struct genome *g = genomeWithList(rootGenome, myChildren);
struct genomePriv *gp = g->priv;

for(ii=0; ii < tree->numEdges ; ii++)
    if (hasHigherGenome(tree->edges[ii], gp->number))
	{
	// this is a duplication node
	tree->isDup = TRUE;
	break;
	}

if (ii == tree->numEdges)  // if we didn't break out above
    {
    // assigning genome 
    tree->priv = g;
    }

slCat(childList, myChildren);
}

void setAncestors(struct phyloTree *tree)
{
if ((tree->priv != NULL) && (tree->numEdges != 0))
    {
    struct genome *g = tree->priv;
    tree->ident->name = g->name;
    }

int ii;

for(ii=0; ii < tree->numEdges; ii++)
    setAncestors(tree->edges[ii]);
}

int setNumbers(struct phyloTree *tree)
{
struct genome *g = tree->priv;
struct genomePriv *gp;

if (tree->isDup)
    {
    if (setNumbers(tree->edges[0]) > setNumbers(tree->edges[1]))
	tree->priv = tree->edges[0]->priv;
    else
	tree->priv = tree->edges[1]->priv;

    assert(tree->priv != NULL);
    g = tree->priv;
    }
else
    {
    int ii;

    for(ii=0; ii < tree->numEdges; ii++)
	setNumbers(tree->edges[ii]);
    }

gp = g->priv;
return gp->number;
}

void checkBranch(struct phyloTree *parent, struct phyloTree *tree)
{
struct genome *g = tree->priv;
struct genomePriv *gp = g->priv;
struct genome *p = parent->priv;
struct genomePriv *pp = p->priv;

if (gp->number < pp->number)
    {
    if ((parent->isDup) || (g->node->parent != p->node))
	{
	struct phyloTree *t = g->node;
	struct genome *tg;
	struct phyloTree *n;

	if (parent->isDup)
	    t = parent;
	else
	    while(t->parent != p->node)
		t = t->parent;

	tg = t->priv;

	AllocVar(n);
	AllocVar(n->ident);
	n->priv = tg;
	n->ident->name = tg->name;

	if (parent->edges[0] == tree)
	    parent->edges[0] = n;
	else
	    parent->edges[1] = n;

	n->parent = parent;

	phyloAddEdge(n, tree);

	checkBranch(n, tree);
	}

    }
}

void insertLosses(struct phyloTree *tree)
{
int ii;

for(ii=0 ; ii < tree->numEdges; ii++)
    {
    checkBranch(tree, tree->edges[ii]);
    insertLosses(tree->edges[ii]);
    }

}

void addLosses(struct phyloTree *tree)
{
setNumbers(tree);

insertLosses(tree);
}

void assignDups(struct phyloTree *reconNode, struct phyloTree *genomeNode)
{
struct phyloTree *reconParent = reconNode->parent;
struct phyloTree *genomeParent = genomeNode->parent;
//struct genome *genomeGenome = genomeNode->priv;

//printf("assign %p %p\n",reconNode,genomeNode);
//printf("assign genomeNode->priv %p\n",genomeNode->priv);

{
//checkTree(genomeNode);
int ii;

for(ii=0; ii < reconNode->numEdges; ii++)
    {
    while (genomeNode->priv == reconNode->edges[ii]->priv)
	genomeNode = genomeNode->parent;

	//errAbort("goobar");
    }
}

reconNode->priv = genomeNode->priv;
//reconNode->ident->name = genomeGenome->name;

if (reconParent == NULL)
    return;

if (reconParent->isDup)
    {
    //if (genomeParent == NULL)
	//printf("loobar\n");
    if ((genomeParent == NULL) || (!genomeParent->isDup))
	{
	struct phyloTree *t;
	struct genome *g;
	static int dupCount = 0;
	char buffer[1024];

	safef(buffer, sizeof(buffer), "dup.%d",++dupCount);

	AllocVar(t);
	AllocVar(g);
	g->name = cloneString(buffer);
	g->node = t;
	t->isDup = TRUE;
	t->priv = g;
	AllocVar(t->ident);
	t->ident->name = g->name;

	t->parent = genomeParent;

	if (genomeParent != NULL)
	    {
	    if (genomeParent->edges[0] == genomeNode)
		genomeParent->edges[0] = t;
	    else
		genomeParent->edges[1] = t;
	    }

	//printf("allocDup\n");
	phyloAddEdge(t, genomeNode);
	genomeNode->parent = t;
	}
    }

//printf("poop\n");
if (!reconNode->isDup && (reconNode->priv != genomeNode->priv))
    errAbort("species node not matching %s %s",reconNode->ident->name,genomeNode->ident->name);

genomeParent = genomeNode->parent;
while ((!reconParent->isDup) && (genomeParent->isDup))
    {
    genomeParent = genomeParent->parent;
    }
assignDups(reconNode->parent, genomeParent);

}

void addDupsToGenomeTree(struct phyloTree *reconTree)
{
if (reconTree->numEdges == 0)
    {
    struct genome *g = reconTree->priv;

    //printf("starting up\n");
    assignDups(reconTree, g->node);
    }

int ii;

for(ii=0; ii < reconTree->numEdges; ii++)
    addDupsToGenomeTree(reconTree->edges[ii]);
}

struct element *addElements(struct phyloTree *reconTree)
{
struct genome *g = reconTree->priv;
char *name = NULL;
static int fooCount = 0;
if (reconTree->ident->name)
    name = strchr(reconTree->ident->name, '.');
if (name != NULL)
    name++;
else
    {
    char buffer[1024];

    safef(buffer, sizeof(buffer), "foo%d",++fooCount);
    name=cloneString(buffer);
    }

struct element *element = newElement(g, "0", name);
int ii;
struct element *child = NULL;
struct phyloTree *genomeNode = g->node;

for(ii=0; ii < reconTree->numEdges; ii++)
    eleAddEdge(element, child = addElements(reconTree->edges[ii]));

if (child != NULL)
    element->version = child->version;

struct element *oldElement;
//while ((genomeNode != NULL) && (reconTree->parent != NULL) && (genomeNode->parent != NULL) && (genomeNode->parent->priv != reconTree->parent->priv))
/*
if (!((genomeNode->parent == NULL) && (reconTree->parent == NULL)))
{
//struct genome *parentGenome = genomeNode->parent->priv;
struct genome *parentRecon = reconTree->parent->priv;


if (g->name == parentRecon->name)
    errAbort("messuy");
//printf("starting %s\n",g->name);
//printf("at node %s in recon.  at node %s in genome\n",parentRecon->name, parentGenome->name);
*/
//while ((genomeNode != NULL) && (reconTree->parent != NULL) && (genomeNode->parent != NULL) && (genomeNode->parent->priv != reconTree->parent->priv))
while ( genomeNode->parent->priv != reconTree->parent->priv)
    {
    assert(genomeNode->parent->priv);
    oldElement = element;
    if (name == NULL)
	break;
    genomeNode = genomeNode->parent;
    element = newElement(genomeNode->priv, "0", name);
    eleAddEdge(element, oldElement);
//parentGenome = genomeNode->parent->priv;
//printf("up node %s in recon.  at node %s in genome\n",parentRecon->name, parentGenome->name);
    }

//}
//printf("out\n");
return element;
}

struct phyloTree *findTop(struct phyloTree *tree)
{
struct genome *g = tree->priv;

if (g->elements != NULL)
    return tree;

int ii;
struct phyloTree *ret;

for(ii=0; ii < tree->numEdges; ii++)
    if ( (ret = findTop(tree->edges[ii])) != NULL)
	return ret;

return NULL;
}


void pruneDead(struct phyloTree *tree)
{
int ii;
for(ii=0; ii < tree->numEdges; )
    {
    struct genome *g = tree->edges[ii]->priv;

    //printf("name %s elements %p\n",g->name,g->elements);
    if ((g == NULL) || (g->elements == NULL))
	{
	phyloDeleteEdge(tree, tree->edges[ii]);
	continue;
	}
    else
	pruneDead(tree->edges[ii]);
    ii++;
    }
}


void nhxToElTree(char *speciesTreeName, char *reconTreeName, char *outFile)
{
struct phyloTree *speciesTree = phyloOpenTree(speciesTreeName);
struct phyloTree *reconTree = phyloOpenTree(reconTreeName);
struct phyloTree *tree;
FILE *f = mustOpen(outFile, "w");
struct hash *leafHash = newHash(10);
int lowNum = 1;
struct slName *myChildren = NULL;

buildGenomeTree(speciesTree, &lowNum, leafHash, &myChildren);

myChildren = NULL;
labelReconTree(reconTree, leafHash, &myChildren, speciesTree->priv);

setAncestors(reconTree);
//printf("ancestors assigned\n");
//phyloPrintTree(reconTree, stdout);

addLosses(reconTree);

//printf("losses added\n");
//phyloPrintTree(reconTree, stdout);

addDupsToGenomeTree(reconTree);

while(speciesTree->parent)
    speciesTree = speciesTree->parent;
//printf("species tree with dups added\n");
//phyloPrintTree(speciesTree, stdout);

//printf("reconTree tree with dups referenced\n");
//phyloPrintTree(reconTree, f);
checkTree(reconTree);

{
struct genome *g = reconTree->priv;
printf("g %s\n",g->name);
addElements(reconTree);

}

while(speciesTree->parent)
    speciesTree = speciesTree->parent;
tree = findTop(speciesTree);

if (doPrune)
    pruneDead(tree);

//printf("element tree\n");
outElementTrees(f, tree);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

doPrune = optionExists("prune");

nhxToElTree(argv[1], argv[2], argv[3]);
return 0;
}

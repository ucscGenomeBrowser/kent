#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "phyloTree.h"


struct phyloTree *phyloReadTree(struct lineFile *lf)
/* reads a phyloTree from lineFile (first line only) */
{
struct phyloTree *tree = NULL;
char *ptr;
int len;

if (lineFileNext(lf, &ptr, &len) && (len > 0))
    tree = phyloParseString(ptr);

return tree;
}

struct phyloTree *phyloOpenTree(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct phyloTree *tree = phyloReadTree(lf);

lineFileClose(&lf);

return tree;
}


static struct phyloName *parseIdent(char **ptrPtr)
/* read a node name with possibile branch length */
{
struct phyloName *pName;
char *start = *ptrPtr;
char *ptr = *ptrPtr;

AllocVar(pName);
/* legal id's are alphanumeric */
while(isalpha(*ptr) || isdigit(*ptr) || (*ptr == '/')
     || (*ptr == '\'')
     || (*ptr == '>')
     || (*ptr == '<')
    || (*ptr == '.') || (*ptr == '_')) 
    ptr++;

/* did we read something? */
if(ptr > start)
    {
    char val;

    val = *ptr;
    *ptr = 0;
    pName->name = cloneString(start);
    *ptr = val;
    }

/* is there some branch length info */
if (*ptr == ':')
    {
    ptr++;
    sscanf(ptr, "%lg", &pName->length);
    while ((*ptr != '[') && (*ptr != ')') && (*ptr != ',') && (*ptr != ';'))
	ptr++;
    }

*ptrPtr = ptr;

return pName;
}

static struct phyloTree *newEdge(struct phyloTree *parent, struct phyloTree *child)
{
parent->numEdges++;

if (parent->numEdges > parent->allocedEdges)
    {
    int oldSize = parent->allocedEdges * sizeof (struct phyloTree *);
    int newSize;

    parent->allocedEdges += 5;
    newSize = parent->allocedEdges * sizeof (struct phyloTree *);
    parent->edges = needMoreMem(parent->edges, oldSize, newSize);
    }

if (!child)
    errAbort("unexpected error: child is null in phyloTree.c::newEdge()");

child->parent = parent;
return parent->edges[parent->numEdges -1 ] = child;
}

struct phyloTree *phyloAddEdge(struct phyloTree *parent, struct phyloTree *child)
/* add an edge to a phyloTree node */
{
return newEdge(parent, child);
}

static struct phyloTree *parseSubTree(char **ptrPtr)
/* the recursive workhorse function, parses a tree from ptr */
{
struct phyloTree *node = NULL;
char *ptr = *ptrPtr;

/* trees are terminated by one of these three chars */
if ((*ptr == ';') || (*ptr == ',') || (*ptr == ')') )
    return NULL;

AllocVar(node);
if (*ptr == '(') 
    {
    struct phyloTree *edge;

    ptr++;

    do
	{
	struct phyloTree *child = parseSubTree(&ptr);
	if (!child)
	    errAbort("missing child/subTree at (%s)",ptr-1);
	edge = newEdge(node,child);
	edge->parent = node;
	} while (*ptr++ == ',');
    --ptr;
    if (*ptr++ != ')') 
	errAbort("unbalanced parenthesis at (%s)",ptr-1);
    node->ident = parseIdent(&ptr);
    }
else 
    if ((*ptr == ':') || (isalpha(*ptr))|| (isdigit(*ptr)) 
	 || (*ptr == '\'') || (*ptr == '.'))
	node->ident = parseIdent(&ptr);
else
    errAbort("illegal char '%c' in phyloString",*ptr);

if (*ptr == '[')
    {
    if (startsWith("[&&NHX:D=Y]",ptr))
	node->isDup = TRUE;

    while(*ptr != ']')
	ptr++;

    ptr++;

    }

*ptrPtr = ptr;


return node;
}

struct phyloTree *phyloParseString(char *string)
/* build a phyloTree from a string */
{
struct phyloTree *tree = NULL;
char *ptr = string;

eraseWhiteSpace(string);

tree = parseSubTree(&ptr);

if (*ptr != ';')
    errAbort("expecting tree terminator ';', found '%s'", ptr);

return tree;
}


/*  some static stuff for printing out trees */
static int recurseCount = 0;

static void tabOut(FILE *f)
{
int i;

for(i=0; i < recurseCount; i++)
    fputc(' ',f);
}

static void pTree( struct phyloTree *tree,FILE *f, boolean noDups)
/* print out phylogenetic tree in Newick format */
{
if (tree)
    {
    int ii;
    if (noDups && (tree->numEdges == 1))
	pTree(tree->edges[0], f, noDups);
    else 
	{
	if (tree->numEdges)
	    {
	    fprintf(f,"(");
	    for (ii= 0; ii < tree->numEdges; ii++)
		{
		pTree(tree->edges[ii], f, noDups);
		if (ii + 1 < tree->numEdges)
		    fprintf(f,",");
		}
	    fprintf(f,")");
	    }
	if (tree->ident->name)
	    fprintf(f,"%s",tree->ident->name);
	//if (tree->ident->length != 0.0)
	    fprintf(f,":%0.04g", tree->ident->length);
	if (tree->isDup)
	    fprintf(f,"[&&NHX:D=Y]");
	}
    }
}

void phyloPrintTreeNoDups( struct phyloTree *tree,FILE *f)
/* print out phylogenetic tree in Newick format (only speciation nodes) */
{
pTree(tree, f, TRUE);
fprintf(f, ";\n");
}

void phyloPrintTree( struct phyloTree *tree,FILE *f)
/* print out phylogenetic tree in Newick format */
{
pTree(tree, f, FALSE);
fprintf(f, ";\n");
}

void phyloDebugTree( struct phyloTree *tree,FILE *f)
/* print out phylogenetic tree */
{
if (tree)
    {
    int ii;
    fprintf(f,"%s:%g numEdges %d\n",tree->ident->name, tree->ident->length, tree->numEdges);
    recurseCount++;
    for (ii= 0; ii < tree->numEdges; ii++)
	{
	tabOut(f);
	phyloDebugTree(tree->edges[ii], f);
	}
    recurseCount--;
    }
}

struct phyloTree *phyloFindName( struct phyloTree *tree,char *name )
/* find the node with this name */
{
struct phyloTree *subTree = NULL;
int ii;

if (tree->ident->name && sameString(tree->ident->name, name))
    return tree;

for (ii=0; ii < tree->numEdges; ii++)
    {
    if ((subTree = phyloFindName( tree->edges[ii], name)) != NULL)
	break;
    }

return subTree;
}

void phyloClearTreeMarks(struct phyloTree *tree)
/* clear the favorite child marks */
{
int ii;

tree->mark = 0;

for (ii=0; ii < tree->numEdges; ii++)
    phyloClearTreeMarks(tree->edges[ii]);
}

struct phyloTree *phyloFindMarkUpTree(struct phyloTree *tree)
/* find a marked node somewhere above this node */
{
do
    {
    if (tree->mark)
	return tree;
    tree = tree->parent;
    } while (tree);

return NULL;
}

void phyloMarkUpTree(struct phyloTree *tree)
/* mark all the nodes from this one up to the top of the tree */
{
    tree->mark = tree;
    for(;tree->parent; tree = tree->parent)
	tree->parent->mark = tree;
}

char *phyloFindPath(struct phyloTree *tree, char *ref, char *cross)
/* find the shortest path from ref to cross (returns a list
 * of the node names separated by spaces) */
{
struct phyloTree *treeRef, *treeCross, *parent;
struct dyString *ds = newDyString(0);

if ((treeRef = phyloFindName(tree,ref)) == NULL)
    return NULL;

if ((treeCross = phyloFindName(tree,cross)) == NULL)
    return NULL;

phyloClearTreeMarks(tree);
phyloMarkUpTree(treeCross);
if ((parent = phyloFindMarkUpTree(treeRef)) == NULL)
    return NULL;

/* walk up the tree till we hit the common parent */
while(treeRef != parent)
    {
    treeRef = treeRef->parent;
    if (ds->stringSize)
	dyStringAppendC(ds, ' ');
    if (treeRef->ident->name)
	dyStringAppendN(ds, treeRef->ident->name, strlen(treeRef->ident->name));
    }

/* now walk down the tree till we come to the target species */
while (parent != treeCross)
    {
    parent = parent->mark;
    dyStringAppendC(ds, ' ');
    if (parent->ident->name)
	dyStringAppendN(ds, parent->ident->name, strlen(parent->ident->name));
    }

return ds->string;
}


static void nodeNames(struct phyloTree *tree, struct dyString *ds)
/* recursive workhorse to add all the node names to a string */
{
int ii;

if (tree->ident->name)
    {
    dyStringAppendN(ds, tree->ident->name, strlen(tree->ident->name));
    dyStringAppendC(ds, ' ');
    }

for (ii=0; ii < tree->numEdges; ii++)
    nodeNames(tree->edges[ii],ds);
}

char *phyloNodeNames(struct phyloTree *tree)
/* add all the node names to a dy string */
{
struct dyString *ds = newDyString(0);

nodeNames(tree, ds);

ds->string[ds->stringSize-1]=0;

return ds->string;
}


static void reParent(struct phyloTree *tree)
{
if (tree->parent)
    {
    struct phyloTree *edge, *saveParent = tree->parent;

    reParent(saveParent); /* make the parent into the root */
    phyloDeleteEdge(saveParent, tree); /* remove this tree from the
					  parent tree */
    tree->parent = NULL; /* make this tree the root */
    edge = newEdge(tree, saveParent); /* add the old parent tree as a
					child of the new root */
    edge->parent = tree; /* set the parent in the new child */

    edge->ident->length = tree->ident->length;
    }
}

struct phyloTree *phyloReRoot(struct phyloTree *tree)
/* return a tree whose root is tree and what were parents are now "right" children */
{
reParent(tree);
tree->ident->length = 0;

return tree;
}

void phyloDeleteEdge(struct phyloTree *tree, struct phyloTree *edge)
/* delete an edge from a node.  Aborts on error */
{
int ii;

for (ii=0; ii < tree->numEdges; ii++)
    if (tree->edges[ii] == edge)
	{
	memcpy(&tree->edges[ii], &tree->edges[ii+1], sizeof(tree) * (tree->numEdges - ii - 1));
	tree->numEdges--;
	//phyloFreeTree(edge);
	return;
	}

errAbort("tried to delete non-existant edge");
}

int phyloCountLeaves(struct phyloTree *tree)
{
int ii, count = 0;

if (tree->numEdges == 0)
    return 1;

for (ii=0; ii < tree->numEdges; ii++)
    count += phyloCountLeaves(tree->edges[ii]);

return count;
}

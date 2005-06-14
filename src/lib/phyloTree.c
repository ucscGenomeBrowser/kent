#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "phyloTree.h"

struct phyloTree *phyloReadTree(struct lineFile *lf)
/* reads a phyloTree from lineFile (first line only) */
{
struct phyloTree *tree = NULL;
char buffer[512];

tree = phyloParseString(buffer);

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
while(isalpha(*ptr) || isdigit(*ptr))
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
    while ((*ptr != ')') && (*ptr != ',') && (*ptr != ';'))
	ptr++;
    }

*ptrPtr = ptr;

return pName;
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
    ptr++;

    node->left = parseSubTree(&ptr);
    node->left->parent = node;
    if (*ptr++ != ',')
	errAbort("all nodes must have zero or two children error at (%s)",ptr-1);
    node->right = parseSubTree(&ptr);
    node->right->parent = node;
    if (*ptr++ != ')') 
	errAbort("unbalanced parenthesis");
    node->ident = parseIdent(&ptr);
    }
else 
    if ((*ptr == ':') || (isalpha(*ptr))|| (isdigit(*ptr)))
	node->ident = parseIdent(&ptr);
else
    errAbort("illegal char '%c' in phyloString",*ptr);

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
    errAbort("trees must terminated by ';'");

return tree;
}


/*  some static stuff for printing out trees */
static int recurseCount = 0;

static void tabOut(FILE *f)
{
int i;

for(i=0; i < recurseCount; i++)
    fputc('\t',f);
}

void phyloPrintTree( struct phyloTree *tree,FILE *f)
/* print out phylogenetic tree */
{
if (tree)
    {
    printf("%s:%g\n",tree->ident->name, tree->ident->length);
    if (tree->left)
	{
	recurseCount++;
	tabOut(f);
	phyloPrintTree(tree->left, f);
	tabOut(f);
	phyloPrintTree(tree->right, f);
	recurseCount--;
	}
    }
}

struct phyloTree *phyloFindName( struct phyloTree *tree,char *name )
/* find the node with this name */
{
struct phyloTree *subTree = NULL;

if (tree->ident->name && sameString(tree->ident->name, name))
    return tree;

if (tree->left)
    {
    if ((subTree = phyloFindName( tree->left, name)) == NULL)
	subTree = phyloFindName( tree->right, name);
    }

return subTree;
}

void phyloClearTreeMarks(struct phyloTree *tree)
/* clear the favortite child marks */
{
tree->mark = 0;

if (tree->left)
    {
    phyloClearTreeMarks(tree->left);
    phyloClearTreeMarks(tree->right);
    }
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
    if (tree->ident->name)
	{
	dyStringAppendN(ds, tree->ident->name, strlen(tree->ident->name));
	dyStringAppendC(ds, ' ');
	}
    if (tree->left)
	{
	nodeNames(tree->left,ds);
	nodeNames(tree->right,ds);
	}
}

char *phyloNodeNames(struct phyloTree *tree)
/* add all the node names to a dy string */
{
struct dyString *ds = newDyString(0);

nodeNames(tree, ds);

ds->string[ds->stringSize-1]=0;

return ds->string;
}

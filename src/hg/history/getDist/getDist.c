#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: getDist.c,v 1.5 2006/05/17 15:14:58 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getDist - prints an element tree\n"
  "usage:\n"
  "   getDist elementTreeFile outFile\n"
  "arguments:\n"
  "   elementTreeFile      name of file containing element tree\n"
  "   outFile              name of file to output distance matrices to\n"
  "options:\n"
  "   -names     use special names (remove I's)\n"
  );
}

static struct optionSpec options[] = {
    {"names", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean SpecialNames = FALSE;


void downToLeaves(struct distance **distanceList, struct hash **pDistHash, struct hash **pDistElemHash, 
    struct element *otherElement , struct element *e, double sumBranchLength)
{
if (e->numEdges)
    {
    int ii;

    for (ii = 0; ii < e->numEdges; ii++)
	downToLeaves(distanceList, pDistHash, pDistElemHash, otherElement, e->edges[ii], 
	    sumBranchLength + e->edges[ii]->genome->node->ident->length);
    }
else if (e->genome->node->numEdges == 0)
    {
    setElementDist(otherElement, e, sumBranchLength, distanceList,
		pDistHash, pDistElemHash);
    }
}

void getAllSibs(struct distance **distanceList, struct hash **pDistHash, struct hash **pDistElemHash, 
    struct element *otherElement, struct element *currentElement, double sumBranchLength)
{
int ii;
struct element *parent;

if (parent = currentElement->parent)
    {
    for (ii = 0; ii < parent->numEdges; ii++)
	{
	if (parent->edges[ii] != currentElement)
	    downToLeaves(distanceList, pDistHash, pDistElemHash, otherElement, parent->edges[ii],  
		sumBranchLength + parent->edges[ii]->genome->node->ident->length);
	}
    getAllSibs(distanceList, pDistHash, pDistElemHash, otherElement, parent, sumBranchLength + parent->genome->node->ident->length);
    }
}

void findAllLeaves(struct distance **distanceList, struct hash **pDistHash, struct hash **pDistElemHash, 
    struct element *e)
{
if (e->numEdges != 0)
    {
    int ii; 

    for(ii = 0; ii < e->numEdges; ii++)
	findAllLeaves(distanceList, pDistHash, pDistElemHash, e->edges[ii]);
    }
else if (e->genome->node->numEdges == 0)
    {
    getAllSibs(distanceList, pDistHash, pDistElemHash, e, e, e->genome->node->ident->length);
    }
}

int compar(const void *one, const void *two)
{
char *s1 = *(char **)one;
char *s2 = *(char **)two;

return strcmp(s1, s2);
}

void outElemTreeDist(struct element *e, FILE *f)
{
struct distance *distanceList = NULL;
struct hash *distHash = NULL;
struct hash *numHash = newHash(5);
struct distance *d;
struct hash *distElemHash = NULL;
int count = 0;
struct hashEl *hel, *dupe, *list = NULL;
int ii;
char **names;
double *distanceArray;

findAllLeaves(&distanceList, &distHash, &distElemHash, e);

if (distElemHash == NULL)
    return;

for (ii=0; ii<distElemHash->size; ++ii)
    for (hel = distElemHash->table[ii]; hel != NULL; hel = hel->next)
	{
	count++;
	}
fprintf(f, "%d\n", count);

names = (char **)needMem(sizeof(char *) * count);
count = 0;
for (ii=0; ii<distElemHash->size; ++ii)
    for (hel = distElemHash->table[ii]; hel != NULL; hel = hel->next)
	{
	names[count] = hel->name;
	count++;
	}

qsort(names, count, sizeof(char *), compar);

for(ii=0; ii < count; ii++)
    hashAdd(numHash, names[ii], (void *)ii);

distanceArray = (double *)needMem(sizeof(double) * count * count);

for(ii = 0; ii < count; ii++)
    distanceArray[ii * count + ii] = 0.0;

for(d= distanceList ; d ; d= d->next)
    {
    int row, col;

    row = (int)hashMustFindVal(numHash, eleName(d->e1));
    col = (int)hashMustFindVal(numHash, eleName(d->e2));
    distanceArray[row * count + col] = d->distance;
    distanceArray[col * count + row] = d->distance;
    }

for(ii=0; ii < count; ii++)
    {
    int jj;

    fprintf(f, "%-20s ",names[ii]);
    for(jj=0; jj < count; jj++)
	fprintf(f, "%g ",distanceArray[ii * count + jj]);
    fprintf(f, "\n");
    }

//freez(&names);
//freez(&distanceArray);
}

void outDistances(struct phyloTree *root, FILE *f)
{
struct genome *g = root->priv;
struct element *e = g->elements;

for(; e; e = e->next)
    {
    outElemTreeDist(e, f);
    }
}

void removeAllIs(struct phyloTree *node)
{
int ii;

removeXs(node->priv, 'I');

for(ii=0; ii < node->numEdges; ii++)
    removeAllIs(node->edges[ii]);
}

void getDist(char *treeFile, char *outFile)
{
struct phyloTree *node = eleReadTree(treeFile, FALSE);
FILE *f = mustOpen(outFile, "w");

//    printElementTrees(node, 0);

if (SpecialNames)
    removeAllIs(node);

    outDistances(node, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

if (optionExists("names"))
    SpecialNames = TRUE;

//verboseSetLevel(2);
getDist(argv[1], argv[2]);
return 0;
}

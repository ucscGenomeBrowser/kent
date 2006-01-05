#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"

#include "element.h"

void printGenomes(struct genome *genomes)
{
struct genome *genome;
for(genome=genomes; genome; genome = genome->next)
    {
    struct element *element;

    printf(">%s\n",genome->name);
    for(element = genome->elements; element; element = element->next)
	printf("%s.%s \n",element->name, element->version);
    printf("\n");
    }
}

struct genome *readGenomes(char *fileName)
{
struct genome *allGenomes = NULL, *genome = NULL;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[256];
int wordsRead;

while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
    {
    if ((wordsRead == 1) && (words[0][0] == '>'))
	{
	AllocVar(genome);
	slAddHead(&allGenomes, genome);
	genome->name = cloneString(&words[0][1]);
	verbose(2, "adding genome %s\n",genome->name);
	genome->elementHash = newHash(8);
	}
    else
	{
	int ii;

	if (genome == NULL)
	    errAbort("must specify genome name before listing elements");

	for(ii=0; ii < wordsRead; ii++)
	    {
	    char *ptr;
	    struct element *element;

	    AllocVar(element);
	    slAddHead(&genome->elements, element);

	    element->genome = genome;
	    element->species = genome->name;
	    element->name = cloneString(words[ii]);
	    if (*element->name == '-')
		element->name++;
	    hashAdd(genome->elementHash, element->name, element);

	    if ((ptr = strchr(element->name, '.')) == NULL)
		errAbort("elements must be of the format name.version");
	    *ptr++ = 0;
	    element->version = ptr;
	    verbose(2, "added element %s.%s\n",element->name,element->version);
	    }
	}
    }

for(genome=allGenomes; genome; genome = genome->next)
    slReverse(&genome->elements);

return allGenomes;
}

static struct element *getElement(char *name, struct hash *genomeHash)
{
char *s = cloneString(name);
struct element *e;
char *ptr;
struct genome *genome;
char *species, *ename;// *version;

if ((ptr = strchr(s, '.')) == NULL)
    errAbort("element names must be species.element.version");
species = s;

*ptr++ = 0;
ename = ptr;
if ((ptr = strchr(ptr, '.')) == NULL)
    errAbort("element names must be species.element.version");
//*ptr++ = 0;
//version = ptr;

if ((genome = hashFindVal(genomeHash, species)) == NULL)
    errAbort("can't find element list for %s\n",species);

if ((e = hashFindVal(genome->elementHash, ename)) == NULL)
    errAbort("can't find element with name %s in genome %s\n",ename,species);

return e;
}

static struct hash *distHash = NULL;

void setElementDist(struct element *e1, struct element *e2, double dist,
    struct distance **distanceList)
{
int val = 0;
char buffer[512];
char *str = "%s.%s.%s-%s.%s.%s";
float *pdist;

if (!sameString(e1->name, e2->name))
    errAbort("can't set element distance on elements with different names");

if ((val = strcmp(e1->species, e2->species)) == 0)
    val = strcmp(e1->version , e2->version);

if (val < 0)
    safef(buffer, sizeof(buffer), str, e1->species, e1->name, e1->version,
    	e2->species, e2->name, e2->version);
else
    safef(buffer, sizeof(buffer), str, e2->species, e2->name, e2->version,
    	e1->species, e1->name, e1->version);

if (distHash == NULL)
    {
    distHash = newHash(5);
    }

if ((pdist = (float *)hashFindVal(distHash, buffer)) != NULL)
    {
#define EPSILON 0.01
    if (fabs(dist - *pdist) > EPSILON)
	errAbort("attempting to set dist for %s to different value (%g) (%g)\n",
	    buffer, dist, *pdist);
    }
else
    {
	struct distance *distance;

	AllocVar(pdist);
	*pdist = dist;
	hashAdd(distHash, buffer, pdist);
	AllocVar(distance);
	slAddHead(distanceList, distance);

	distance->e1 = e1;
	distance->e2 = e2;
	distance->distance = dist;
	
    }
}

int sortByDistance(const void *p1, const void *p2)
{
const struct distance *d1 = *((struct distance **)p1);
const struct distance *d2 = *((struct distance **)p2);
double diff;

if ((diff = d1->distance - d2->distance) > 0)
    return 1;
else if (diff < 0)
    return -1;
return 0;
}

struct distance *readDistances(char *fileName, struct hash *genomeHash)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[256];
int wordsRead;
struct distance *distances = NULL;

while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *))))
    {
    int count = 0;
    char **rowNames;
    double **rowValues;
    int ii, jj;

    if ((wordsRead != 1) || ((count = atoi(words[0])) <= 0))
	errAbort("first line of dist matrix is count of rows");

    rowNames = (char **)needMem(count * sizeof(char *));
    rowValues = (double **)needMem(count * sizeof(double *));
    for(ii=0; ii < count; ii++)
	{
	wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *));
	if (wordsRead != count + 1)
	    errAbort("number of words in distance matrix row must be %d\n",count+1);

	rowNames[ii] = cloneString(words[0]);
	rowValues[ii] = needMem(sizeof(double) * count);

	for(jj=0; jj < count ; jj++)
	    rowValues[ii][jj] = atof(words[jj+1]);
	}
    for(ii=0; ii < count; ii++)
	{
	for(jj=0; jj < count ; jj++)
	    {
	    char *s1 = rowNames[ii];
	    char *s2 = rowNames[jj];
	    struct element *e1, *e2;

	    if (ii == jj)
		continue;

	    if (strcmp(s1, s2) > 0)
		{
		s2 = rowNames[ii];
		s1 = rowNames[jj];
		}
	    e1 = getElement(s1, genomeHash);
	    e2 = getElement(s2, genomeHash);

	    setElementDist(e1, e2, rowValues[ii][jj], &distances);
	    }
	}
    }

slSort(&distances, sortByDistance);
return distances;
}


static struct element *newEdge(struct element *parent, struct element *child)
{
parent->numEdges++;

if (parent->numEdges > parent->allocedEdges)
    {
    int oldSize = parent->allocedEdges * sizeof (struct element *);
    int newSize;

    parent->allocedEdges += 5;
    newSize = parent->allocedEdges * sizeof (struct element *);
    parent->edges = needMoreMem(parent->edges, oldSize, newSize);
    }

child->parent = parent;
return parent->edges[parent->numEdges -1 ] = child;
}

struct element *eleAddEdge(struct element *parent, struct element *child)
/* add an edge to a phyloTree node */
{
return newEdge(parent, child);
}

void eleDeleteEdge(struct element *tree, struct element *edge)
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

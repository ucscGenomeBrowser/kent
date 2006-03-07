#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: elTreeComp.c,v 1.3 2006/03/07 22:02:25 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "elTreeComp - compares two element trees\n"
  "usage:\n"
  "   elTreeComp elementTreeFile elementTreeFile\n"
  "arguments:\n"
  "   elementTreeFile      name of file containing element tree\n"
  "options:\n"
  "   -leaf      just print out leaf species\n"
  "   -noVers    don't print out the version strings\n"
  "   -sort      sort elements first so element order isn't important\n"
  );
}

static struct optionSpec options[] = {
    {"leaf", OPTION_BOOLEAN},
    {"noVers", OPTION_BOOLEAN},
    {"sort", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean JustLeaf = FALSE;
boolean NoVers = FALSE;
boolean DoSort = FALSE;


int elemComp(const void *one, const void *two)
{
struct element *e1 = *(struct element **)one;
struct element *e2 = *(struct element **)two;

return strcmp(e1->name, e2->name);
}

void compGenome(struct genome *g1, struct genome *g2)
{
struct element *e1, *e2;
int count = 0;

if (slCount(g1->elements) != slCount(g2->elements))
    warn("diff: genomes %s and %s have different # of elements (%d and %d)",
	g1->name, g2->name, slCount(g1->elements), slCount(g2->elements));

if (DoSort)
    {
    slSort(&g1->elements, elemComp);
    slSort(&g2->elements, elemComp);
    }

for(e1=g1->elements, e2=g2->elements; e1 && e2 ; e1 = e1->next, e2=e2->next,count++)
    {
    if (!sameString(e1->name, e2->name))
	{
	warn("in genomes %s and %s element %d not equal (%s and %s)",
	    g1->name, g2->name, count, e1->name, e2->name);
	break;
	}
    else if (!DoSort && (e1->isFlipped != e2->isFlipped))
	warn("in genomes %s element %s not same strand (%d and %d)",
	    g1->name,  eleName(e1), e1->isFlipped, e2->isFlipped);
    }
}

void compNode(struct phyloTree *node1, struct phyloTree *node2)
{
struct genome *g1 = node1->priv;
struct genome *g2 = node2->priv;

if (!sameString(node1->ident->name, node2->ident->name))
    warn("diff: nodes %s and %s have different names",
	node1->ident->name,node2->ident->name);

if (node1->numEdges != node2->numEdges)
    warn("diff: nodes %s and %s have different # of edges (%d and %d)",
	node1->ident->name,node2->ident->name,node1->numEdges,node2->numEdges);

compGenome(g1, g2);
}

void treeComp(struct phyloTree *node1, struct phyloTree *node2)
{
int ii, jj;

compNode(node1, node2);

for(ii=0; ii < node1->numEdges; ii++)
    {
    for(jj=0; jj < node2->numEdges; jj++)
	if (sameString(node1->edges[ii]->ident->name, node2->edges[jj]->ident->name))
	    break;
    if (jj == node2->numEdges)
	{
	warn("no child named %s under %s",node1->edges[ii]->ident->name, node2->ident->name);
	break;
	}

    treeComp(node1->edges[ii], node2->edges[jj]);
    }
}

void elTreeComp(char *treeFile1, char *treeFile2)
{
struct phyloTree *node1 = eleReadTree(treeFile1, FALSE);
struct phyloTree *node2 = eleReadTree(treeFile2, FALSE);

treeComp(node1, node2);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

if (optionExists("leaf"))
    JustLeaf = TRUE;
if (optionExists("noVers"))
    NoVers = TRUE;
if (optionExists("sort"))
    DoSort = TRUE;

//verboseSetLevel(2);
elTreeComp(argv[1], argv[2]);
return 0;
}

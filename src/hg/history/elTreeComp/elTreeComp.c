#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: elTreeComp.c,v 1.5 2006/05/17 15:13:49 braney Exp $";

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
  "   -sort      sort elements first so element order isn't important\n"
  "   -triple    only check nodes with two children and a parent\n"
  "   -noStrand  don't count strand errors\n"
  );
}

static struct optionSpec options[] = {
    {"sort", OPTION_BOOLEAN},
    {"triple", OPTION_BOOLEAN},
    {"noStrand", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean DoSort = FALSE;
boolean OnlyTriple = FALSE;
boolean NoStrand = FALSE;


int elemComp(const void *one, const void *two)
{
struct element *e1 = *(struct element **)one;
struct element *e2 = *(struct element **)two;
int d;

if ((d = strcmp(e1->name, e2->name)) == 0)
    d = strcmp(e1->version, e2->version);

return d;
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
	warn("in genomes %s element %d name not equal (%s and %s)",
	    g1->name,  count, e1->name, e2->name);
	break;
	}
    if (!sameString(e1->version, e2->version))
	{
	warn("in genome %s element %s version not equal (%s and %s)",
	    g1->name,  e1->name, e1->version, e2->version);
	break;
	}
    else if (!NoStrand && !DoSort && (e1->isFlipped != e2->isFlipped))
	warn("in genomes %s element %s not same strand (%d and %d)",
	    g1->name,  eleName(e1), e1->isFlipped, e2->isFlipped);
    }
}

void compNode(struct phyloTree *node1, struct phyloTree *node2)
{
struct genome *g1 = node1->priv;
struct genome *g2 = node2->priv;

//if (OnlyTriple && ((node1->parent == NULL) || (node1->parent->numEdges != 2) ||  (node1->numEdges != 2)))
if (OnlyTriple && ((node1->parent == NULL) || (node1->numEdges != 2)))
    {
    verbose(2, "not checking %s\n",node1->ident->name);
    return;
    }

verbose(2, "checking %s\n",node1->ident->name);
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

DoSort = optionExists("sort");
OnlyTriple = optionExists("triple");
NoStrand = optionExists("noStrand");

//verboseSetLevel(2);
elTreeComp(argv[1], argv[2]);
return 0;
}

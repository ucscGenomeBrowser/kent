#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: elTreeToList.c,v 1.5 2006/05/17 15:14:32 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "elTreeToList - converts an elTree to an element list\n"
  "usage:\n"
  "   elTreeToList elementTreeFile outFile\n"
  "arguments:\n"
  "   elementTreeFile      name of file containing element tree\n"
  "   outFile              name of file to write element lists\n"
  "options:\n"
  "   -leaf      just print out leaf species\n"
  "   -noVers    don't print out the version strings\n"
  "   -names     use special names (remove I's)\n"
  );
}

static struct optionSpec options[] = {
    {"leaf", OPTION_BOOLEAN},
    {"noVers", OPTION_BOOLEAN},
    {"names", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean JustLeaf = FALSE;
boolean NoVers = FALSE;
boolean SpecialNames = FALSE;


void printElements(FILE *f, struct phyloTree *node)
{
int ii;

if ((node->numEdges == 0) || !JustLeaf)
    {
    struct genome *g = node->priv;
    struct element *e;
    if (SpecialNames)
	removeXs(g, 'I');

    fprintf(f, ">%s %d\n",g->name,slCount(g->elements));
    for(e=g->elements; e ; e= e->next)
	{
	if (e->isFlipped)
	    fprintf(f, "-");
	if (NoVers)
	    fprintf(f, "%s ",e->name);
	else
	    fprintf(f, "%s.%s ",e->name, e->version);
	}
    fprintf(f, "\n");
    }

for(ii=0; ii < node->numEdges; ii++)
    printElements(f, node->edges[ii]);
}

void elTreeToList(char *treeFile, char *outFileName)
{
struct phyloTree *node = eleReadTree(treeFile, FALSE);
FILE *f = mustOpen(outFileName, "w");
    //printElementTrees(node, 0);

printElements(f, node);
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
if (optionExists("names"))
    SpecialNames = TRUE;

//verboseSetLevel(2);
elTreeToList(argv[1], argv[2]);
return 0;
}

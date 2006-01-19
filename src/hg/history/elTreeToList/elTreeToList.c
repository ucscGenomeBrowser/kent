#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"

static char const rcsid[] = "$Id: elTreeToList.c,v 1.1 2006/01/19 00:12:45 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "elTreeToList - prints an element tree\n"
  "usage:\n"
  "   elTreeToList elementTreeFile outFile\n"
  "arguments:\n"
  "   elementTreeFile      name of file containing element tree\n"
  "   outFile              name of file to write element lists\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void printLeafElements(FILE *f, struct phyloTree *node)
{
if (node->numEdges)
    {
    int ii;

    for(ii=0; ii < node->numEdges; ii++)
	printLeafElements(f, node->edges[ii]);
    }
else
    {
    struct genome *g = node->priv;
    struct element *e;
    fprintf(f, ">%s %d\n",g->name,slCount(g->elements));
    for(e=g->elements; e ; e= e->next)
	{
	fprintf(f, "%s.%s ",e->name, e->version);
	}
    fprintf(f, "\n");
    }
}

void elTreeToList(char *treeFile, char *outFileName)
{
struct phyloTree *node = eleReadTree(treeFile);
FILE *f = mustOpen(outFileName, "w");
    printElementTrees(node, 0);

printLeafElements(f, node);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

//verboseSetLevel(2);
elTreeToList(argv[1], argv[2]);
return 0;
}

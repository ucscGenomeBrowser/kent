#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "diGraph.h"

static char const rcsid[] = "$Id: chopTree.c,v 1.1 2006/01/05 16:56:07 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chopTree - given a neighbor joined tree and an outgroup, output \n"
  "           a phyloTree that represent the species tree with the outgroup\n"
  "           chopped off.\n"
  "usage:\n"
  "   chopTree treeFile outGroup outPhylo\n"
  "arguments:\n"
  "   treeFile       name of file containing neighbor joined tree\n"
  "   outGroup       the name of the species to use as outgroup\n"
  "   outPhylo       file to output species tree (phyloTree)\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void chopTree(char *treeFile, char *outGroup, char *outFileName)
{
struct phyloTree *node, *newRoot;
FILE *f = mustOpen(outFileName, "w");
struct lineFile *lf = lineFileOpen(treeFile, FALSE);
struct phyloTree *njTree = phyloReadTree(lf);

lineFileClose(&lf);

//phyloDebugTree(njTree,f);
if ((node = phyloFindName(njTree,outGroup)) == NULL)
    errAbort("can find outgroup (%s) in neighbor joined tree");

//fprintf(f, "group node \n");
//phyloDebugTree(node,f);

//fprintf(f, "group node parent \n");
//phyloDebugTree(node->parent,f);

//fprintf(f, "delete edge \n");
phyloDeleteEdge(node->parent, node);
//phyloDebugTree(node->parent,f);

newRoot = phyloReRoot(node->parent);
//phyloDebugTree(newRoot,f);

phyloPrintTree(newRoot,f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

//verboseSetLevel(2);
chopTree(argv[1],argv[2], argv[3]);
return 0;
}

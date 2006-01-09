#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"
#include "diGraph.h"

static char const rcsid[] = "$Id: printTree.c,v 1.1 2006/01/09 18:00:01 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "printTree - prints an element tree\n"
  "usage:\n"
  "   printTree elementTreeFile\n"
  "arguments:\n"
  "   elementTreeFile      name of file containing element tree\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void printTree(char *treeFile)
{
struct phyloTree *node = eleReadTree(treeFile);
    printElementTrees(node, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

//verboseSetLevel(2);
printTree(argv[1]);
return 0;
}

#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"
#include "psGfx.h"

static char const rcsid[] = "$Id: elTreeClip.c,v 1.2 2006/03/07 22:25:55 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "elTreeClip - clips out group and collapses inversion nodes\n"
  "usage:\n"
  "   elTreeClip inTree outName clipTree\n"
  "arguments:\n"
  "   inTree      name of file containing element tree\n"
  "   outName     name of out group\n"
  "   clipTree    tree without outgroup and with inversion nodes collapsed\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void removeInodes(struct phyloTree *node)
{
struct phyloTree *child;

removeIs(node->priv);
if (node->numEdges == 0)
    return;

child = node->edges[0];
while (child && (node->numEdges == 1)  && ( child->ident->name[strlen(child->ident->name) - 1] == 'I'))
    {
    struct genome *g;
    struct element *e;

    node->numEdges = child->numEdges;
    node->priv = child->priv;
    g = node->priv;
    for(e=g->elements; e; e=e->next)
	e->parent = e->parent->parent;
    node->ident->length += child->ident->length;
    removeIs(node->priv);
    if (node->numEdges == 0)
	break;

    node->edges[0] = child->edges[0];
    node->edges[1] = child->edges[1];
    child = node->edges[0];
    }

removeInodes(node->edges[0]);
if (node->numEdges == 2)
    removeInodes(node->edges[1]);
}

void elTreeClip(char *treeFile, char *outGroupName,  char *outFile)
{
struct phyloTree *node = eleReadTree(treeFile, FALSE);
struct phyloTree *child;
FILE *f = mustOpen(outFile, "w");

child = node->edges[0];
if (sameString(child->ident->name, outGroupName))
    child = node->edges[1];
else if (!sameString(node->edges[1]->ident->name, outGroupName))
    errAbort("can't find node named %s",outGroupName);

removeInodes(child);
outElementTrees(f, child);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

//verboseSetLevel(2);
elTreeClip(argv[1], argv[2], argv[3]);
return 0;
}

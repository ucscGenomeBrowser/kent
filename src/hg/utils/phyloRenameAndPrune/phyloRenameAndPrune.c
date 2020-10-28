/* phyloRenameAndPrune - Rename or remove leaves of phylogenetic tree and prune any branches with no remaining leaves. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "options.h"
#include "phyloTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "phyloRenameAndPrune - Rename or remove leaves of phylogenetic tree and prune any branches with no remaining leaves\n"
  "usage:\n"
  "   phyloRenameAndPrune treeIn.nh renaming.txt treeOut.nh\n"
//  "options:\n"
//  "   -xxx=XXX\n"
  "renaming.txt has two whitespace-separated columns: old name (must uniquely match\n"
  "some leaf in tree) and new name.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct phyloTree *rRenameAndPrune(struct phyloTree *node, struct hash *renaming)
{
if (node->numEdges == 0)
    {
    char *newName = NULL;
    if (node->ident->name && (newName = hashFindVal(renaming, node->ident->name)) != NULL)
        node->ident->name = newName;
    else
        return NULL;
    }
else
    {
    // Rename or prune children
    struct phyloTree *newKids[node->numEdges];
    int newKidCount = 0;
    int i;
    for (i = 0;  i < node->numEdges;  i++)
        {
        struct phyloTree *kid = rRenameAndPrune(node->edges[i], renaming);
        if (kid)
            newKids[newKidCount++] = kid;
        }
    if (newKidCount == 0)
        return NULL;
    else if (newKidCount < node->numEdges)
        {
        // At least one kid was pruned; update node.
        node->numEdges = newKidCount;
        for (i = 0;  i < newKidCount;  i++)
            node->edges[i] = newKids[i];
        }
    }
return node;
}

void phyloRenameAndPrune(char *treeInFile, char *renamingFile, char *treeOutFile)
/* phyloRenameAndPrune - Rename or remove leaves of phylogenetic tree and prune any branches with no remaining leaves. */
{
struct phyloTree *tree = phyloOpenTree(treeInFile);
struct hash *renaming = hashTwoColumnFile(renamingFile);
tree = rRenameAndPrune(tree, renaming);
FILE *outF = mustOpen(treeOutFile, "w");
if (tree)
    phyloPrintTree(tree, outF);
else
    warn("No leaves were renamed, all were pruned; no tree to write to %s.", treeOutFile);
carefulClose(&outF);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
phyloRenameAndPrune(argv[1], argv[2], argv[3]);
return 0;
}

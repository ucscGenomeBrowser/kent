#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"
#include "psGfx.h"

static char const rcsid[] = "$Id: printTree.c,v 1.2 2006/03/03 21:43:27 braney Exp $";

int psSize = 5*72;
int labelStep = 1;
double postScale = 1.0;
int margin = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "printTree - prints an element tree\n"
  "usage:\n"
  "   printTree elementTreeFile ps.out\n"
  "arguments:\n"
  "   elementTreeFile      name of file containing element tree\n"
  "   ps.out               name of file for postscript output \n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int currY = 0;
void treeOut(struct phyloTree *node, struct psGfx *ps, int depth)
{
struct genome *g = node->priv;
struct element *e;
int ii;
char buffer[512];

safef(buffer, sizeof(buffer), "%s %g %d: ",g->name, node->ident->length, node->numEdges);
psTextAt(ps, depth * 36, currY, buffer);
currY += 20;
//for(ii=0; ii < depth; ii++)
 //   printf(" ");
/*
for(e = g->elements; e; e = e->next)
    {
    if (e->isFlipped)
	printf("-");
    printf("%s.%s %d ",e->name,e->version,e->numEdges);
    }
printf("\n");
*/

for(ii=0; ii < node->numEdges; ii++)
    treeOut(node->edges[ii], ps, depth+1);
}

void printTree(char *treeFile, char *psFile)
{
struct phyloTree *node = eleReadTree(treeFile);
struct psGfx *ps = psOpen(psFile, psSize, psSize, psSize, psSize, margin);

treeOut(node, ps, 0);
psClose(&ps);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

//verboseSetLevel(2);
printTree(argv[1], argv[2]);
return 0;
}

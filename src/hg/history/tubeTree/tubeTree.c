#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "psGfx.h"

static char const rcsid[] = "$Id: tubeTree.c,v 1.1 2006/11/14 17:53:51 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tubeTree - render a tube tree from an NHX file\n"
  "usage:\n"
  "   tubeTree file.nhx file.ps \n"
  "arguments:\n"
  "   file.nhx      a file in the NHX format\n"
  "   file.ps       postscript output file\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


double genomeWidth;
double genomeSpacing;

double startX, startY;

struct genome
{
char *name;
double x, y;
};

void drawGenome(struct psGfx *gfx, struct genome *g, bool drawName)
{
if (1)//drawName)
    {
    psSetColor(gfx, 255, 255, 255);
    psFillEllipse(gfx, g->x, g->y, genomeWidth/3, genomeWidth/2);
    psSetColor(gfx, 255, 0, 0);
    psDrawEllipse(gfx, g->x, g->y, genomeWidth/3, genomeWidth/2, 0, 360);
    }

if (drawName)
    //psTextCentered(gfx, g->x, g->y ,  genomeWidth,
//	genomeWidth, g->name);
    psTextAt(gfx,  g->x + genomeWidth/2, g->y - genomeWidth/2 , g->name);
}


struct phyloTree *leftMostLeaf(struct phyloTree *tree)
{
if (tree->numEdges == 0)
    return tree;

return leftMostLeaf(tree->edges[0]);
}

struct phyloTree *rightMostLeaf(struct phyloTree *tree)
{
if (tree->numEdges == 0)
    return tree;

return rightMostLeaf(tree->edges[1]);
}

void drawTree(struct psGfx *gfx, struct phyloTree *tree)
{
int ii;
struct genome *g = tree->priv;

if (g && (tree->numEdges != 0))
    {
    struct phyloTree *left = tree->edges[0];
    struct phyloTree *right = tree->edges[1];
    struct genome *lg = left->priv;
    struct genome *rg = right->priv;
    psSetColor(gfx, 0, 0, 0);
    psDrawLine(gfx, g->x, g->y + genomeWidth/2 , lg->x, lg->y + genomeWidth/2);
    psDrawLine(gfx, g->x, g->y - genomeWidth/2 , lg->x, lg->y - genomeWidth/2);
    psDrawLine(gfx, g->x , g->y+ genomeWidth/2 , rg->x , rg->y + genomeWidth/2);
    psDrawLine(gfx, g->x, g->y - genomeWidth/2 , rg->x, rg->y - genomeWidth/2);

    psSetColor(gfx, 255, 255, 255);

    fprintf(gfx->f, "newpath\n");
    psMoveTo(gfx, g->x, g->y + genomeWidth / 2);
    psLineTo(gfx, rg->x , rg->y +  genomeWidth / 2);
    psLineTo(gfx, rg->x , rg->y- genomeWidth / 2);
    psLineTo(gfx, g->x , g->y- genomeWidth / 2);
    psLineTo(gfx, g->x , g->y+ genomeWidth / 2);
    fprintf(gfx->f, "fill\n");

    fprintf(gfx->f, "newpath\n");
    psMoveTo(gfx, g->x , g->y+ genomeWidth / 2);
    psLineTo(gfx, lg->x , lg->y+ genomeWidth / 2);
    psLineTo(gfx, lg->x , lg->y- genomeWidth / 2);
    psLineTo(gfx, g->x , g->y- genomeWidth / 2);
    psLineTo(gfx, g->x , g->y+ genomeWidth / 2);
    fprintf(gfx->f, "fill\n");
    }

if (tree->priv)
    drawGenome(gfx, tree->priv, (tree->numEdges == 0));

for(ii=0; ii < tree->numEdges; ii++)
    drawTree(gfx, tree->edges[ii]);
}

void calcPos(struct phyloTree *tree, struct phyloTree *child) 
{
struct phyloTree *p;
int count  = 0;
struct genome *g = tree->priv;
struct genome *cg = child->priv;

for(p = child->parent ; p != tree ; p = p->parent)
    count++;

if (count)
    {
    double incX = -(cg->x - g->x) / (count + 1);
    double incY = -(cg->y - g->y) / (count + 1);
    double x = cg->x;
    double y = cg->y;

    for(p = child->parent ; p != tree ; p = p->parent)
	{
	struct genome *g;

	assert(p->priv == NULL);
	AllocVar(g);
	g->name = p->ident->name;
	p->priv = g;

	x += incX;
	y += incY;

	g->x = x;
	g->y = y;

	}
    }

}

void fillInPos(struct phyloTree * tree)
{
struct phyloTree *rc, *lc;
struct genome *g = tree->priv;

assert(g != NULL);
if (tree->numEdges == 0)
    return;

lc = tree->edges[0];
rc = tree->edges[1];
if (lc->priv == NULL)
    calcPos(tree, leftMostLeaf(tree));
if (rc->priv == NULL)
    calcPos(tree, rightMostLeaf(tree));

fillInPos(lc);
fillInPos(rc);
}

void setLeavePos(struct phyloTree * tree)
{
int ii;

if (tree->numEdges == 0)
    {
    struct genome *g;

    AllocVar(g);
    g->name = tree->ident->name;
    g->x = startX;
    g->y = startY;
    startY += (genomeWidth + genomeSpacing );

    tree->priv = g;
    return;
    }

for(ii=0; ii < tree->numEdges; ii++)
    setLeavePos(tree->edges[ii]);
}

void tubeTree(char *inFile, char *outFile)
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct phyloTree *tree = phyloReadTree(lf);
FILE *f = mustOpen(outFile, "w");
int width = 500;
int height = 500;
struct psGfx *gfx = psOpen(outFile, width, height, 500.0, 500.0, 100.0);
int numLeaves = phyloCountLeaves(tree);
int ii;
int x, y;
struct genome *g;

genomeWidth =  width / numLeaves;
genomeSpacing = genomeWidth / 5;
genomeWidth -= genomeSpacing;

fprintf(gfx->f, "%d %d translate\n", 20, 20);

startX = 300;
startY = genomeWidth/2;

setLeavePos(tree);

AllocVar(g);
g->name = tree->ident->name;
g->x = 100;
g->y =  height / 4;
tree->priv = g;
fillInPos(tree);

drawTree(gfx, tree);

psClose(&gfx);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

tubeTree(argv[1], argv[2]);
return 0;
}

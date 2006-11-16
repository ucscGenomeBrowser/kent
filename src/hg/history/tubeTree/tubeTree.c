#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "psGfx.h"
#include "element.h"
#include "chromColors.h"

static char const rcsid[] = "$Id: tubeTree.c,v 1.2 2006/11/16 17:39:48 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tubeTree - render a tube tree from an NHX file\n"
  "usage:\n"
  "   tubeTree file.nhx elementName file.ps \n"
  "arguments:\n"
  "   file.nhx      a file in the NHX format\n"
  "   elementName   the name of the element to graph\n"
  "   file.ps       postscript output file\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct
{
    int r, g, b;
} Colors[256];

double genomeWidth;
double genomeSpacing;

double startX, startY;

struct elementPriv
{
double x, y;
int index;
};

struct genomePriv
{
double x, y;
};

void setColors()
{
Colors[0].r = 0; Colors[0].g = 0; Colors[0].b = 0; 
Colors[1].r = CHROM_1_R; Colors[1].g = CHROM_1_G; Colors[1].b = CHROM_1_B; 
Colors[2].r = CHROM_2_R; Colors[2].g = CHROM_2_G; Colors[2].b = CHROM_2_B; 
Colors[3].r = CHROM_3_R; Colors[3].g = CHROM_3_G; Colors[3].b = CHROM_3_B; 
Colors[4].r = CHROM_4_R; Colors[4].g = CHROM_4_G; Colors[4].b = CHROM_4_B; 
Colors[5].r = CHROM_5_R; Colors[5].g = CHROM_5_G; Colors[5].b = CHROM_5_B; 
Colors[6].r = CHROM_6_R; Colors[6].g = CHROM_6_G; Colors[6].b = CHROM_6_B; 
Colors[7].r = CHROM_7_R; Colors[7].g = CHROM_7_G; Colors[7].b = CHROM_7_B; 
Colors[8].r = CHROM_8_R; Colors[8].g = CHROM_8_G; Colors[8].b = CHROM_8_B; 
Colors[9].r = CHROM_9_R; Colors[9].g = CHROM_9_G; Colors[9].b = CHROM_9_B; 
Colors[10].r = CHROM_10_R; Colors[10].g = CHROM_10_G; Colors[10].b = CHROM_10_B; 
Colors[11].r = CHROM_11_R; Colors[11].g = CHROM_11_G; Colors[11].b = CHROM_11_B; 
Colors[12].r = CHROM_12_R; Colors[12].g = CHROM_12_G; Colors[12].b = CHROM_12_B; 
Colors[13].r = CHROM_13_R; Colors[13].g = CHROM_13_G; Colors[13].b = CHROM_13_B; 
Colors[14].r = CHROM_14_R; Colors[14].g = CHROM_14_G; Colors[14].b = CHROM_14_B; 
Colors[15].r = CHROM_15_R; Colors[15].g = CHROM_15_G; Colors[15].b = CHROM_15_B; 
Colors[16].r = CHROM_16_R; Colors[16].g = CHROM_16_G; Colors[16].b = CHROM_16_B; 
Colors[17].r = CHROM_17_R; Colors[17].g = CHROM_17_G; Colors[17].b = CHROM_17_B; 
Colors[18].r = CHROM_18_R; Colors[18].g = CHROM_18_G; Colors[18].b = CHROM_18_B; 
Colors[19].r = CHROM_19_R; Colors[19].g = CHROM_19_G; Colors[19].b = CHROM_19_B; 
Colors[20].r = CHROM_20_R; Colors[20].g = CHROM_20_G; Colors[20].b = CHROM_20_B; 
Colors[21].r = CHROM_21_R; Colors[21].g = CHROM_21_G; Colors[21].b = CHROM_21_B; 
Colors[22].r = CHROM_22_R; Colors[22].g = CHROM_22_G; Colors[22].b = CHROM_22_B; 
}

int elemComp(const void *one, const void *two)
{
struct element *e1 = *(struct element **)one;
struct element *e2 = *(struct element **)two;
struct element *p;
struct phyloTree *pTree;

int d = 0;

for(;;)
    {
    p = e1->parent;
    pTree = p->genome->node;
    if (pTree->numEdges == 2)
	break;
    e1 = p;
    }

for(;;)
    {
    p = e2->parent;
    pTree = p->genome->node;
    if (pTree->numEdges == 2)
	break;
    e2 = p;
    }

if (e1->parent > e2->parent)
    d = 1;
else if (e1->parent < e2->parent)
    d = -1;

return d;
}

void drawGenome(struct psGfx *gfx, struct genome *g, bool drawName)
{
struct genomePriv *gp = g->priv;

if (1)//drawName)
    {
fprintf(gfx->f, "%f setlinewidth\n", 1.0);
    psSetColor(gfx, 255, 255, 255);
    psFillEllipse(gfx, gp->x, gp->y, genomeWidth/3, genomeWidth/2);
    psSetColor(gfx, 127, 127, 127);
    psDrawEllipse(gfx, gp->x, gp->y, genomeWidth/3, genomeWidth/2, 0, 360);
    }

if (drawName)
    //psTextCentered(gfx, g->x, g->y ,  genomeWidth,
//	genomeWidth, g->name);
    psTextAt(gfx,  gp->x + genomeWidth + 20, gp->y - 4, g->name);
}


struct phyloTree *leftMostLeaf(struct phyloTree *tree)
{
if (tree->numEdges == 0)
    return tree;

return leftMostLeaf(tree->edges[0]);
}

struct phyloTree *rightMostLeaf(struct phyloTree *tree)
{
int childNum = 1;

if (tree->numEdges == 0)
    return tree;

if (tree->numEdges == 1)
    childNum = 0;

return rightMostLeaf(tree->edges[childNum]);
}

void drawSpeciesTree(struct psGfx *gfx, struct phyloTree *tree)
{
int ii;
struct genome *g = tree->priv;
struct genomePriv *gp = g->priv;

if (gp && (tree->numEdges == 2))
    {
    struct phyloTree *left = tree->edges[0];
    struct phyloTree *right = tree->edges[1];
    struct genome *lg, *rg;
    struct genomePriv *lgp, *rgp;

    while (left->numEdges == 1)
	left = left->edges[0];
    while (right->numEdges == 1)
	right = right->edges[0];

    lg = left->priv;
    rg = right->priv;
    lgp = lg->priv;
    rgp = rg->priv;

fprintf(gfx->f, "%f setlinewidth\n", 2.0);
    psSetColor(gfx, 127, 127, 127);
    psDrawLine(gfx, gp->x, gp->y + genomeWidth/2 , lgp->x, lgp->y + genomeWidth/2);
    psDrawLine(gfx, gp->x, gp->y - genomeWidth/2 , lgp->x, lgp->y - genomeWidth/2);
    psDrawLine(gfx, gp->x , gp->y+ genomeWidth/2 , rgp->x , rgp->y + genomeWidth/2);
    psDrawLine(gfx, gp->x, gp->y - genomeWidth/2 , rgp->x, rgp->y - genomeWidth/2);

    psSetColor(gfx, 255, 255, 255);

    fprintf(gfx->f, "newpath\n");
    psMoveTo(gfx, gp->x, gp->y + genomeWidth / 2);
    psLineTo(gfx, rgp->x , rgp->y +  genomeWidth / 2);
    psLineTo(gfx, rgp->x , rgp->y- genomeWidth / 2);
    psLineTo(gfx, gp->x , gp->y- genomeWidth / 2);
    psLineTo(gfx, gp->x , gp->y+ genomeWidth / 2);
    fprintf(gfx->f, "fill\n");

    fprintf(gfx->f, "newpath\n");
    psMoveTo(gfx, gp->x , gp->y+ genomeWidth / 2);
    psLineTo(gfx, lgp->x , lgp->y+ genomeWidth / 2);
    psLineTo(gfx, lgp->x , lgp->y- genomeWidth / 2);
    psLineTo(gfx, gp->x , gp->y- genomeWidth / 2);
    psLineTo(gfx, gp->x , gp->y+ genomeWidth / 2);
    fprintf(gfx->f, "fill\n");
    }

if (g->priv)
    drawGenome(gfx, tree->priv, (tree->numEdges == 0));

for(ii=0; ii < tree->numEdges; ii++)
    drawSpeciesTree(gfx, tree->edges[ii]);
}

void calcPos(struct phyloTree *tree, struct phyloTree *child) 
{
struct phyloTree *p;
int count  = 0;
struct genome *g = tree->priv;
struct genomePriv *gp = g->priv;
struct genome *cg = child->priv;
struct genomePriv *cgp = cg->priv;

for(p = child->parent ; p != tree ; p = p->parent)
    if (p->numEdges == 2)
	count++;

//fprintf(stderr, "count is %d\n",count);
if (count)
    {
    double incX = -(cgp->x - gp->x) / (count + 1);
    double incY = -(cgp->y - gp->y) / (count + 1);
    double x = cgp->x;
    double y = cgp->y;

//fprintf(stderr, "gpx %g  gpy %g\n",gp->x, gp->y);
//fprintf(stderr, "incX %g  incY %g\n",incX, incY);
    for(p = child->parent ; p != tree ; p = p->parent)
	{
	struct genome *pg;
	struct genomePriv *pgp;

	if (p->numEdges == 1)
	    continue;

	pg = p->priv;
	assert(pg->priv == NULL);
	AllocVar(pgp);
	pg->priv = pgp;

	x += incX;
	y += incY;

	pgp->x = x;
	pgp->y = y;

	}
    }

}

void fillInPos(struct phyloTree * tree)
{
struct phyloTree *rc, *lc;
struct genome *rcg, *lcg;
struct genome *g = tree->priv;
struct genomePriv *gp = g->priv;

assert(g != NULL);
if (tree->numEdges == 0)
    return;

if (tree->numEdges == 1)
    {
    fillInPos(tree->edges[0]);
    return;
    }

lc = tree->edges[0];
while (lc->numEdges == 1)
    lc = lc->edges[0];
lcg = lc->priv;

rc = tree->edges[1];
while (rc->numEdges == 1)
    rc = rc->edges[0];
rcg = rc->priv;

if (lcg->priv == NULL)
    calcPos(tree, leftMostLeaf(tree));
if (rcg->priv == NULL)
    calcPos(tree, rightMostLeaf(tree));

fillInPos(lc);
fillInPos(rc);
}

void setLeavePos(struct phyloTree * tree)
{
int ii;

if (tree->numEdges == 0)
    {
    struct genome*g;
    struct genomePriv *gp;

    AllocVar(gp);
    g = tree->priv;
    gp->x = startX;
    gp->y = startY;
    startY += (genomeWidth + genomeSpacing );

    g->priv = gp;
    return;
    }

for(ii=0; ii < tree->numEdges; ii++)
    setLeavePos(tree->edges[ii]);
}

void setLineColor(struct psGfx *gfx, int index)
{
    //fprintf(stderr,"index %d\n",index);
    psSetColor(gfx, Colors[index].r,Colors[index].g,Colors[index].b);
}

int Index;

void drawElementTree(struct psGfx *gfx, struct phyloTree *tree, char *elementName)
{
struct genome *g = tree->priv;
struct genomePriv *gp = g->priv;
struct element *e;
int count = 0;
struct element *eTable[1024];
int ii;

for(e = g->elements; e ; e = e->next)
    {
    char buf[4096];
    char *ptr;

    strncpy(buf, e->name, sizeof(buf));
    if ((ptr = strchr(buf, '.')) != NULL)
	*ptr = 0;

    //fprintf(stderr, "looking for %s at %s\n",elementName, buf);
    if (sameString(buf, elementName))
	{
	eTable[count] = e;
	count++;
	}
    }
    //fprintf(stderr, "count %d\n",count);

if (count && (gp != NULL))
    {
    double incY = (genomeWidth) / (count + 0);
    double x = gp->x;
    double y = gp->y - genomeWidth/3;
    int ii;

    if (count == 1)
	y = gp->y;

    psSetColor(gfx, 0, 0, 0);
    for(ii=0; ii < count; ii++)
	{
	struct elementPriv *ep;
	struct element *p;

	e = eTable[ii];

	assert(e->priv == NULL);
	AllocVar(ep);
	e->priv = ep;

	ep->index = Index++;
	ep->x = x;
	ep->y = y;
	 //   fprintf(stderr, "new node %g %g \n",ep->x,ep->y);
	y += incY;

	psFillEllipse(gfx, ep->x, ep->y, genomeWidth/10, genomeWidth/10);
	if (tree->numEdges == 0)
	    {
	    //char buffer[4096];

	    //safef(buffer, sizeof(buffer), "%s.%s.%s",e->species,e->name, e->version);
	    psTextAt(gfx, ep->x + genomeWidth/2, ep->y - 4, e->version);
	    }

	//    fprintf(stderr, "looing parent\n");
	if (e->parent)
	    {
	    struct elementPriv *pep;
	    struct phyloTree *pTree;

	    for(;;)
		{
		p = e->parent;
		pTree = p->genome->node;
		if (pTree->numEdges == 2)
		    break;
		e = p;
		}

	    pep = p->priv;
	    if (pep == NULL)
		errAbort("nonono");;

	    //fprintf(stderr, "has parent %g %g %g %g\n",ep->x,ep->y,pep->x, pep->y);
//fprintf(gfx->f, "%f setlinewidth\n", 10.0);
	    setLineColor(gfx, ep->index);
	    psDrawLine(gfx, ep->x, ep->y, pep->x, pep->y);
	    }
	}
    }

for(ii=0; ii < tree->numEdges; ii++)
    drawElementTree(gfx, tree->edges[ii], elementName);
}

void fillInDupNodes(struct phyloTree *tree)
{

}

void sortElements(struct phyloTree *tree)
{
struct genome *g = tree->priv;
int ii;

if (tree->parent != NULL)
    slSort(&g->elements, elemComp);

for(ii=0; ii < tree->numEdges; ii++)
    sortElements(tree->edges[ii]);
}

void tubeTree(char *inFile, char *elementName, char *outFile)
{
struct phyloTree *tree = eleReadTree(inFile, FALSE);
FILE *f = mustOpen(outFile, "w");
int width = 500;
int height = 500;
struct psGfx *gfx = psOpen(outFile, width, height, 500.0, 500.0, 100.0);
int numLeaves = phyloCountLeaves(tree);
int ii;
int x, y;
struct genomePriv *gp;
struct genome *g;

sortElements(tree);

genomeWidth =  width / numLeaves;
genomeSpacing = genomeWidth / 15;
genomeWidth -= genomeSpacing;

fprintf(gfx->f, "%d %d translate\n", 20, 20);

startX = 300;
startY = genomeWidth/2;

setLeavePos(tree);

AllocVar(gp);
gp->x = 100;
gp->y =  height / 2;
g = tree->priv;
g->priv = gp;
fillInPos(tree);
fillInDupNodes(tree);

drawSpeciesTree(gfx, tree);

drawElementTree(gfx, tree, elementName);

psClose(&gfx);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

setColors();
tubeTree(argv[1], argv[2], argv[3]);
return 0;
}

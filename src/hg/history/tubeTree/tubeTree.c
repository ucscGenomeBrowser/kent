#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "psGfx.h"
#include "element.h"
#include "chromColors.h"

static char const rcsid[] = "$Id: tubeTree.c,v 1.4 2007/04/24 18:31:14 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tubeTree - render a tube tree from a genome tree file\n"
  "usage:\n"
  "   tubeTree elementTree elementName file.ps \n"
  "arguments:\n"
  "   elementTree   a file containing an element tree\n"
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
double xmaj, ymaj;
};

int countElems(struct genome *g, char *elementName, struct element **eTable);

void HSVtoRGB(double h, double s, double v, int *pr, int *pg, int *pb)
{
if (s == 0)
    {
    *pr = 255 * v;
    *pg = 255 * v;
    *pb = 255 * v;
    }
else
    {
    double i, f, p, q, t;
    double r=0, g=0, b=0;

    h /= 60;
    i = (int)h;
    f =  h - i;
    p = v * ( 1 - s);
    q = v * ( 1 - (s*f));
    t = v * ( 1 - ( s * ( 1 - f)));
    switch((int)h)
	{
	case 0: r = v; g = t; b = p; break;
	case 1: r = q; g = v; b = p; break;
	case 2: r = p; g = v; b = t; break;
	case 3: r = p; g = q; b = v; break;
	case 4: r = t; g = p; b = v; break;
	case 5: r = v; g = p; b = q; break;
	default:
	    errAbort("bad hsv");
	}
    *pr = r * 255;
    *pg = g * 255;
    *pb = b * 255;
    }
}

void setColors()
{
int hueInc = 360/7;
double valInc = 0.15;
double val;
int hue;
int count = 0;
int hueStart = 0;

for(val = 0.8; val > 0.0; hueStart += 20, val -= valInc)
    {
    for(hue=hueStart + 20 * val; hue < 360; hue += hueInc)
	{
	HSVtoRGB(hue, val, val, &Colors[count].r, &Colors[count].g, &Colors[count].b);
	count++;
	}
    }
}

int elemComp(const void *one, const void *two)
{
struct element *e1 = *(struct element **)one;
struct element *e2 = *(struct element **)two;

return e1->count - e2->count;
}

void drawGenome(struct psGfx *gfx, struct genome *g, bool drawName)
{
struct genomePriv *gp = g->priv;
struct phyloTree *tree = g->node;

if ((tree->numEdges != 1) || (tree->parent == NULL))//drawName)
    {
    fprintf(gfx->f, "%f setlinewidth\n", 1.0);
    psSetColor(gfx, 255, 255, 255);
    psFillEllipse(gfx, gp->x, gp->y, gp->xmaj, gp->ymaj);
    psSetColor(gfx, 127, 127, 127);
    psDrawEllipse(gfx, gp->x, gp->y, gp->xmaj, gp->ymaj, 0, 360);
    }

if (drawName)
    {
    psTextAt(gfx,  gp->x + gp->xmaj + 50, gp->y - 4, g->name);
    }
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

fprintf(gfx->f, "[3] 0 setdash\n");

if (gp && ((tree->parent == NULL) ||  (tree->numEdges == 2)))
    {
    struct phyloTree *left = tree->edges[0];
    struct phyloTree *right = tree->edges[1];
    struct genome *lg, *rg;
    struct genomePriv *lgp, *rgp;


if (tree->parent != NULL)
    fprintf(gfx->f, "%f setlinewidth\n", 2.0);
else
    fprintf(gfx->f, "%f setlinewidth\n", 1.0);

    psSetColor(gfx, 127, 127, 127);

    while (left->numEdges == 1)
	left = left->edges[0];

    lg = left->priv;
    lgp = lg->priv;
    psDrawLine(gfx, gp->x, gp->y + gp->ymaj , lgp->x, lgp->y + lgp->ymaj);
    psDrawLine(gfx, gp->x, gp->y - gp->ymaj , lgp->x, lgp->y - lgp->ymaj);

    if (tree->parent != NULL)
	{
	while (right->numEdges == 1)
	    right = right->edges[0];
	rg = right->priv;
	rgp = rg->priv;

	psDrawLine(gfx, gp->x , gp->y+ gp->ymaj , rgp->x , rgp->y + rgp->ymaj);
	psDrawLine(gfx, gp->x, gp->y - gp->ymaj , rgp->x, rgp->y - rgp->ymaj);

	psSetColor(gfx, 255, 255, 255);

	fprintf(gfx->f, "newpath\n");
	psMoveTo(gfx, gp->x, gp->y + gp->ymaj);
	psLineTo(gfx, rgp->x , rgp->y +  rgp->ymaj);
	psLineTo(gfx, rgp->x , rgp->y- rgp->ymaj);
	psLineTo(gfx, gp->x , gp->y- gp->ymaj);
	psLineTo(gfx, gp->x , gp->y+ gp->ymaj);
	fprintf(gfx->f, "fill\n");

	fprintf(gfx->f, "newpath\n");
	psMoveTo(gfx, gp->x , gp->y+ gp->ymaj);
	psLineTo(gfx, lgp->x , lgp->y+ lgp->ymaj);
	psLineTo(gfx, lgp->x , lgp->y- lgp->ymaj);
	psLineTo(gfx, gp->x , gp->y- gp->ymaj);
	psLineTo(gfx, gp->x , gp->y+ gp->ymaj);
	fprintf(gfx->f, "fill\n");
	}
    }

if (g->priv)
    drawGenome(gfx, tree->priv, (tree->numEdges == 0));

fprintf(gfx->f, "[ ] 0 setdash \n");

for(ii=0; ii < tree->numEdges; ii++)
    drawSpeciesTree(gfx, tree->edges[ii]);
}

void calcPos(struct phyloTree *tree, struct phyloTree *child, char *elementName) 
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

if (count)
    {
    double incX = -(cgp->x - gp->x) / (count + 1);
    double incY = -(cgp->y - gp->y) / (count + 1);
    double x = cgp->x;
    double y = cgp->y;

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
	pgp->xmaj = genomeWidth/3;
	pgp->ymaj = genomeWidth/2 + countElems(pg, elementName, NULL) * 10;
	}
    }

}

void calcPosDup(struct phyloTree *tree, struct phyloTree *child, char *elementName) 
{
struct phyloTree *p;
int count  = 0;
struct genome *g = tree->priv;
struct genomePriv *gp = g->priv;
struct genome *cg = child->priv;
struct genomePriv *cgp = cg->priv;

for(p = child->parent ; p != tree ; p = p->parent)
    count++;

if (count)
    {
    double incX = -(cgp->x - gp->x) / (count + 1);
    double incY = -(cgp->y - gp->y) / (count + 1);
    double x = cgp->x;
    double y = cgp->y;

    for(p = child->parent ; p != tree ; p = p->parent)
	{
	struct genome *pg;
	struct genomePriv *pgp;

	pg = p->priv;
	assert(pg->priv == NULL);
	AllocVar(pgp);
	pg->priv = pgp;

	x += incX;
	y += incY;

	pgp->x = x;
	pgp->y = y;
	pgp->xmaj = genomeWidth/3;
	pgp->ymaj = genomeWidth/2 + countElems(pg, elementName, NULL) * 10;

	}
    }

}

void fillInPos(struct phyloTree * tree, char *elementName)
{
struct phyloTree *rc, *lc;
struct genome *rcg, *lcg;
struct genome *g = tree->priv;

assert(g != NULL);
if (tree->numEdges == 0)
    return;

if (tree->numEdges == 1)
    {
    fillInPos(tree->edges[0], elementName);
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
    calcPos(tree, leftMostLeaf(tree), elementName);
if (rcg->priv == NULL)
    calcPos(tree, rightMostLeaf(tree), elementName);

fillInPos(lc, elementName);
fillInPos(rc, elementName);
}


void setLineColor(struct psGfx *gfx, int index)
{
    psSetColor(gfx, Colors[index].r,Colors[index].g,Colors[index].b);
}

int Index;

int countElems(struct genome *g, char *elementName, struct element **eTable)
{
int count = 0;
struct element *e;

for(e = g->elements; e ; e = e->next)
    {
    char buf[4096];
    char *ptr;

    strncpy(buf, e->name, sizeof(buf));
    if ((ptr = strchr(buf, '.')) != NULL)
	*ptr = 0;

    if (sameString(buf, elementName))
	{
	if (eTable)
	    eTable[count] = e;
	count++;
	}
    }

return count;
}

void setLeavePos(struct phyloTree * tree, char *elementName)
{
int ii;

if (tree->numEdges == 0)
    {
    struct genome*g;
    struct genomePriv *gp;

    AllocVar(gp);
    g = tree->priv;
    g->priv = gp;
    gp->xmaj = genomeWidth/3;
    gp->ymaj = genomeWidth/2 + countElems(g, elementName, NULL) * 10;
    gp->x = startX;
    gp->y = startY + gp->ymaj;
    startY += (2*gp->ymaj + genomeSpacing );

    return;
    }

for(ii=0; ii < tree->numEdges; ii++)
    setLeavePos(tree->edges[ii], elementName);
}

void drawElementTree(struct psGfx *gfx, struct phyloTree *tree, char *elementName)
{
struct genome *g = tree->priv;
struct genomePriv *gp = g->priv;
struct element *e;
int count = 0;
struct element *eTable[1024];
int ii;


count = countElems(g, elementName, eTable);

if (count) // && (gp != NULL))
    {
    double incY = (gp->ymaj * 2 - 2*genomeWidth / 5) / (count + 0);
    double x = gp->x;
    double y = gp->y - gp->ymaj + incY;
    int ii;

    if (count == 1)
	y = gp->y;

    psSetColor(gfx, 0, 0, 0);
    for(ii=count-1; ii >= 0; ii--)
	{
	struct elementPriv *ep;
	struct element *p;

	e = eTable[ii];

	assert(e->priv == NULL);
	AllocVar(ep);
	e->priv = ep;

	ep->index = Index;
	ep->x = x;
	ep->y = y;
	y += incY;

	if (tree->numEdges != 1)
	    psFillEllipse(gfx, ep->x, ep->y, genomeWidth/20, genomeWidth/20);
	if (tree->numEdges == 0)
	    {
	    psSetColor(gfx, 0, 0, 0);
	    psTextAt(gfx, ep->x + gp->xmaj - genomeWidth/3 + 15, ep->y - 4, e->version);
	    }

	if (e->parent)
	    {
	    struct elementPriv *pep;
	    struct phyloTree *pTree;

	    p = e->parent;
	    pTree = p->genome->node;
	    pep = p->priv;
	    if (pep == NULL)
		errAbort("nonono");

	    ep->index = pep->index;
	    if ((p->numEdges > 1) && (pTree->numEdges == 1) &&  (p->edges[0] != e))
		ep->index = ++Index;

	    fprintf(gfx->f, "%f setlinewidth\n", 2.0);
	    setLineColor(gfx, ep->index);
	    psDrawLine(gfx, ep->x, ep->y, pep->x, pep->y);
	    }
	if ((tree->numEdges != 1) || (tree->parent == NULL))
	    psFillEllipse(gfx, ep->x, ep->y, genomeWidth/20, genomeWidth/20);
	}
    }

for(ii=0; ii < tree->numEdges; ii++)
    drawElementTree(gfx, tree->edges[ii], elementName);
}

void fillInDupNodes(struct phyloTree *tree, char *elementName)
{
struct phyloTree *lc, *rc;
struct genome *g, *lcg, *rcg;

if (tree->numEdges == 0)
    return;

g = tree->priv;
lc = tree->edges[0];
lcg = lc->priv;
if (tree->numEdges == 1)
    {
    if (tree->parent == NULL)
	{
	struct phyloTree *speciesChild = lc->edges[0];
	while(speciesChild->numEdges == 1)
	    speciesChild = speciesChild->edges[0];
	calcPosDup(tree, speciesChild, elementName);
	}
    else
	{
	assert(lcg->priv != NULL);
	assert(g->priv != NULL);
	}
    fillInDupNodes(lc, elementName);
    return;
    }

rc = tree->edges[1];
rcg = rc->priv;
if (rcg->priv == NULL)
    {
    struct phyloTree *speciesChild = rc->edges[0];

    assert(rc->numEdges == 1);
    while(speciesChild->numEdges == 1)
	speciesChild = speciesChild->edges[0];
    calcPosDup(tree, speciesChild, elementName);
    }

if (lcg->priv == NULL)
    {
    struct phyloTree *speciesChild = lc->edges[0];

    assert(lc->numEdges == 1);
    while(speciesChild->numEdges == 1)
	speciesChild = speciesChild->edges[0];
    calcPosDup(tree, speciesChild, elementName);
    }

fillInDupNodes(lc, elementName);
fillInDupNodes(rc, elementName);
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

int countElements(struct phyloTree *tree, char *elementName)
{
int count = 0;
struct genome *g = tree->priv;
struct element *e;
int ii;

if (tree->parent != NULL)
    {
    for(e = g->elements; e;  e = e->next)
	e->count = e->parent->count;
    slSort(&g->elements, elemComp);
    }

for(e = g->elements; e;  e = e->next)
    e->count = count++;

if (tree->numEdges == 0)
    return countElems(g, elementName, NULL);

count = 0;
for(ii=0; ii < tree->numEdges; ii++)
    count += countElements(tree->edges[ii], elementName);

return count;
}

void tubeTree(char *inFile, char *elementName, char *outFile)
{
struct phyloTree *tree = eleReadTree(inFile, FALSE);
int width = 500;
int height = 500;
struct psGfx *gfx = psOpen(outFile, width, height, 500.0, 500.0, 100.0);
struct genomePriv *gp;
struct genome *g;
int count;
struct phyloTree *subTree;

count = countElements(tree, elementName);

//genomeWidth =  width / (50/10.0 );
//genomeWidth =  width / (count/10.0 );
genomeWidth =  700 / (count/3.0 );
genomeSpacing = genomeWidth / 5.0;
genomeWidth -= genomeSpacing;
//genomeWidth -= count * 4;

psClipRect(gfx, -100.0, -2500.0, 3000.0, 3000.0);
fprintf(gfx->f, "%d %d translate\n", 20, 20);

startX = 600;
startY = genomeSpacing - 600;

setLeavePos(tree, elementName);

subTree = tree;
if (tree->numEdges == 1)
    {

    AllocVar(gp);
    gp->x = -50;
    gp->y =  0;
    gp->xmaj = genomeWidth / 3;
    gp->ymaj = genomeWidth / 2;
    g = tree->priv;
    g->priv = gp;
    for( ; subTree->numEdges == 1;  subTree = subTree->edges[0])
	;
    }

g = subTree->priv;
AllocVar(gp);
gp->x = 100;
gp->y =  0;
gp->xmaj = genomeWidth / 3;
gp->ymaj = genomeWidth/2 + countElems(g, elementName, NULL) * 10;
//gp->ymaj = genomeWidth / 2;
g->priv = gp;
fillInPos(subTree, elementName);
fillInDupNodes(tree, elementName);

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

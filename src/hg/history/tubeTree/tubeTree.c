#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "phyloTree.h"
#include "psGfx.h"
#include "element.h"
#include "chromColors.h"

static char const rcsid[] = "$Id: tubeTree.c,v 1.3 2007/04/24 17:00:56 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tubeTree - render a tube tree from an NHX file\n"
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
    double r, g, b;

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
/*
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
Colors[5]= Colors[21];
Colors[7]= Colors[22];
*/
}

int elemComp(const void *one, const void *two)
{
struct element *e1 = *(struct element **)one;
struct element *e2 = *(struct element **)two;
struct element *p;
struct phyloTree *pTree;

int d = 0;

/*
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
    */
/*
if (e1->parent > e2->parent)
    d = 1;
lse if (e1->parent < e2->parent)
    d = -1;
    */

return e1->count - e2->count;

//return d;
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
    //psTextCentered(gfx, g->x, g->y ,  genomeWidth,
//	genomeWidth, g->name);
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
	pgp->xmaj = genomeWidth/3;
	pgp->ymaj = genomeWidth/2 + countElems(pg, elementName, NULL) * 10;
	//pgp->ymaj = genomeWidth/2;

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

	pg = p->priv;
	assert(pg->priv == NULL);
	AllocVar(pgp);
	pg->priv = pgp;

	x += incX;
	y += incY;

	pgp->x = x;
	pgp->y = y;
	pgp->xmaj = genomeWidth/3;
	//pgp->ymaj = genomeWidth/2;
	pgp->ymaj = genomeWidth/2 + countElems(pg, elementName, NULL) * 10;

	}
    }

}

void fillInPos(struct phyloTree * tree, char *elementName)
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
    //fprintf(stderr,"index %d\n",index);
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

    //fprintf(stderr, "looking for %s at %s\n",elementName, buf);
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
    //fprintf(stderr, "count %d\n",count);

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
	 //   fprintf(stderr, "new node %g %g \n",ep->x,ep->y);
	y += incY;

	if (tree->numEdges != 1)
	    psFillEllipse(gfx, ep->x, ep->y, genomeWidth/20, genomeWidth/20);
	if (tree->numEdges == 0)
	    {
	    //char buffer[4096];

	    //safef(buffer, sizeof(buffer), "%s.%s.%s",e->species,e->name, e->version);

	    psSetColor(gfx, 0, 0, 0);
	    psTextAt(gfx, ep->x + gp->xmaj - genomeWidth/3 + 15, ep->y - 4, e->version);
	    }

	//    fprintf(stderr, "looing parent\n");
	if (e->parent)
	    {
	    struct elementPriv *pep;
	    struct phyloTree *pTree;

	    /*
	    for(;;)
		{
		p = e->parent;
		pTree = p->genome->node;
		if (pTree->numEdges == 2)
		    break;
		e = p;
		}
		*/

	    p = e->parent;
	    pTree = p->genome->node;
	    pep = p->priv;
	    if (pep == NULL)
		errAbort("nonono");

	    ep->index = pep->index;
	    if ((p->numEdges > 1) && (pTree->numEdges == 1) &&  (p->edges[0] != e))
		ep->index = ++Index;

	    //fprintf(stderr, "has parent %g %g %g %g\n",ep->x,ep->y,pep->x, pep->y);
//fprintf(gfx->f, "%f setlinewidth\n", 4.0);
	//    psSetColor(gfx, 255, 255, 255);
	 ////   psDrawLine(gfx, ep->x, ep->y, pep->x, pep->y);
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
struct genomePriv *gp, *lcp, *rcp;

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

    fprintf(stderr,"loo\n");
rc = tree->edges[1];
rcg = rc->priv;
if (rcg->priv == NULL)
    {
    struct phyloTree *speciesChild = rc->edges[0];

    fprintf(stderr,"foo\n");
    assert(rc->numEdges == 1);
    while(speciesChild->numEdges == 1)
	speciesChild = speciesChild->edges[0];
    calcPosDup(tree, speciesChild, elementName);
    }

if (lcg->priv == NULL)
    {
    struct phyloTree *speciesChild = lc->edges[0];

    fprintf(stderr,"goo\n");
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
FILE *f = mustOpen(outFile, "w");
int width = 500;
int height = 500;
struct psGfx *gfx = psOpen(outFile, width, height, 500.0, 500.0, 100.0);
int numLeaves = phyloCountLeaves(tree);
int ii;
int x, y;
struct genomePriv *gp;
struct genome *g;
int count;
struct phyloTree *subTree;

count = countElements(tree, elementName);
//sortElements(tree);

fprintf(stderr,"count %d\n",count);
//genomeWidth =  width / (50/10.0 );
//genomeWidth =  width / (count/10.0 );
genomeWidth =  width / (count/3.0 );
genomeSpacing = genomeWidth / 5.0;
genomeWidth -= genomeSpacing;
//genomeWidth -= count * 4;

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

/* gfxPoly - two dimensional polygon. */
#include "common.h"
#include "gfxPoly.h"

static char const rcsid[] = "$Id: gfxPoly.c,v 1.3 2008/09/17 17:56:37 kent Exp $";

struct gfxPoly *gfxPolyNew()
/* Create new (empty) polygon */
{
struct gfxPoly *poly;
AllocVar(poly);
return poly;
}

void gfxPolyFree(struct gfxPoly **pPoly)
/* Free up resources associated with polygon */
{
struct gfxPoly *poly = *pPoly;
if (poly != NULL)
    {
    if (poly->lastPoint != NULL)
	{
	poly->lastPoint->next = NULL;
	slFreeList(&poly->ptList);
	}
    freez(pPoly);
    }
}

void gfxPolyAddPoint(struct gfxPoly *poly, int x, int y)
/* Add point to polygon. */
{
struct gfxPoint *pt;
poly->ptCount += 1;
AllocVar(pt);
pt->x = x;
pt->y = y;
if (poly->ptList == NULL)
    {
    poly->ptList = poly->lastPoint = pt;
    pt->next = pt;
    }
else
    {
    poly->lastPoint->next = pt;
    pt->next = poly->ptList;
    poly->lastPoint = pt;
    }
}


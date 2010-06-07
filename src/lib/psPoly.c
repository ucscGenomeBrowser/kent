/* psPoly - two dimensional polygon. */
#include "common.h"
#include "psPoly.h"

static char const rcsid[] = "$Id: psPoly.c,v 1.2 2008/09/17 17:56:38 kent Exp $";

struct psPoly *psPolyNew()
/* Create new (empty) polygon */
{
struct psPoly *poly;
AllocVar(poly);
return poly;
}

void psPolyFree(struct psPoly **pPoly)
/* Free up resources associated with polygon */
{
struct psPoly *poly = *pPoly;
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

void psPolyAddPoint(struct psPoly *poly, double x, double y)
/* Add point to polygon. */
{
struct psPoint *pt;
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


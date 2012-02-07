/* psPoly - two dimensional polygon. */
#include "common.h"
#include "psPoly.h"


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


/* psPoly - two dimensional polygon. */

#ifndef PSPOLY_H
#define PSPOLY_H

struct psPoint
/* A two-dimensional point, typically in pixel coordinates. */
    {
    struct psPoint *next;
    double x, y;		/* Position */
    };

struct psPoly
/* A two-dimensional polygon */
    {
    struct psPoly *next;
    int ptCount;		/* Number of points. */
    struct psPoint *ptList;	/* First point in list, which is circular. */
    struct psPoint *lastPoint;	/* Last point in list. */
    };

struct psPoly *psPolyNew();
/* Create new (empty) polygon */

void psPolyFree(struct psPoly **pPoly);
/* Free up resources associated with polygon */

void psPolyAddPoint(struct psPoly *poly, double x, double y);
/* Add point to polygon. */

#endif /* PSPOLY_H */

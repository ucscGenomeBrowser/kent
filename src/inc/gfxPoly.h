/* gfxPoly - two dimensional polygon. */

#ifndef GFXPOLY_H
#define GFXPOLY_H

struct gfxPoint
/* A two-dimensional point, typically in pixel coordinates. */
    {
    struct gfxPoint *next;
    int x, y;		/* Position */
    };

struct gfxPoly
/* A two-dimensional polygon */
    {
    struct gfxPoly *next;
    int ptCount;		/* Number of points. */
    struct gfxPoint *ptList;	/* First point in list, which is circular. */
    struct gfxPoint *lastPoint;	/* Last point in list. */
    };

struct gfxPoly *gfxPolyNew();
/* Create new (empty) polygon */

void gfxPolyFree(struct gfxPoly **pPoly);
/* Free up resources associated with polygon */

void gfxPolyAddPoint(struct gfxPoly *poly, int x, int y);
/* Add point to polygon. */

#endif /* GFXPOLY_H */

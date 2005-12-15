/* polygon.h autoXml generated file */
#ifndef POLYGON_H
#define POLYGON_H

struct polygonPolygon
    {
    struct polygonPolygon *next;
    char *id;	/* Required */
    struct polygonDescription *polygonDescription;	/** Optional (may be NULL). **/
    struct polygonPoint *polygonPoint;	/** Non-empty list required. **/
    };

void polygonPolygonFree(struct polygonPolygon **pObj);
/* Free up polygonPolygon. */

void polygonPolygonFreeList(struct polygonPolygon **pList);
/* Free up list of polygonPolygon. */

void polygonPolygonSave(struct polygonPolygon *obj, int indent, FILE *f);
/* Save polygonPolygon to file. */

struct polygonPolygon *polygonPolygonLoad(char *fileName);
/* Load polygonPolygon from file. */

struct polygonDescription
    {
    struct polygonDescription *next;
    char *text;
    };

void polygonDescriptionFree(struct polygonDescription **pObj);
/* Free up polygonDescription. */

void polygonDescriptionFreeList(struct polygonDescription **pList);
/* Free up list of polygonDescription. */

void polygonDescriptionSave(struct polygonDescription *obj, int indent, FILE *f);
/* Save polygonDescription to file. */

struct polygonDescription *polygonDescriptionLoad(char *fileName);
/* Load polygonDescription from file. */

struct polygonPoint
    {
    struct polygonPoint *next;
    double x;	/* Required */
    double y;	/* Required */
    double z;	/* Defaults to 0 */
    };

void polygonPointFree(struct polygonPoint **pObj);
/* Free up polygonPoint. */

void polygonPointFreeList(struct polygonPoint **pList);
/* Free up list of polygonPoint. */

void polygonPointSave(struct polygonPoint *obj, int indent, FILE *f);
/* Save polygonPoint to file. */

struct polygonPoint *polygonPointLoad(char *fileName);
/* Load polygonPoint from file. */

#endif /* POLYGON_H */


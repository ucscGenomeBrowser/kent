/* polygon.h autoXml generated file */
#ifndef POLYGON_H
#define POLYGON_H

#ifndef XAP_H
#include "xap.h"
#endif

/* The start and end handlers here are used with routines defined in xap.h.
 * In particular if you want to read just parts of the XML file into memory
 * call xapOpen() with these, and then xapNext() with the name of the tag
 * you want to load. */

void *polygonStartHandler(struct xap *xp, char *name, char **atts);
/* Called by xap with start tag.  Does most of the parsing work. */

void polygonEndHandler(struct xap *xp, char *name);
/* Called by xap with end tag.  Checks all required children are loaded. */


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
/* Load polygonPolygon from XML file where it is root element. */

struct polygonPolygon *polygonPolygonLoadNext(struct xap *xap);
/* Load next polygonPolygon element.  Use xapOpen to get xap. */

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
/* Load polygonDescription from XML file where it is root element. */

struct polygonDescription *polygonDescriptionLoadNext(struct xap *xap);
/* Load next polygonDescription element.  Use xapOpen to get xap. */

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
/* Load polygonPoint from XML file where it is root element. */

struct polygonPoint *polygonPointLoadNext(struct xap *xap);
/* Load next polygonPoint element.  Use xapOpen to get xap. */

#endif /* POLYGON_H */


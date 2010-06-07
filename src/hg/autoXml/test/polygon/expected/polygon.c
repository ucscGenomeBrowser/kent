/* polygon.c autoXml generated file */

#include "common.h"
#include "xap.h"
#include "polygon.h"


void polygonPolygonFree(struct polygonPolygon **pObj)
/* Free up polygonPolygon. */
{
struct polygonPolygon *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
polygonDescriptionFree(&obj->polygonDescription);
polygonPointFreeList(&obj->polygonPoint);
freez(pObj);
}

void polygonPolygonFreeList(struct polygonPolygon **pList)
/* Free up list of polygonPolygon. */
{
struct polygonPolygon *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    polygonPolygonFree(&el);
    el = next;
    }
}

void polygonPolygonSave(struct polygonPolygon *obj, int indent, FILE *f)
/* Save polygonPolygon to file. */
{
struct polygonPoint *polygonPoint;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<POLYGON");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
fprintf(f, "\n");
polygonDescriptionSave(obj->polygonDescription, indent+2, f);
for (polygonPoint = obj->polygonPoint; polygonPoint != NULL; polygonPoint = polygonPoint->next)
   {
   polygonPointSave(polygonPoint, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</POLYGON>\n");
}

struct polygonPolygon *polygonPolygonLoad(char *fileName)
/* Load polygonPolygon from XML file where it is root element. */
{
struct polygonPolygon *obj;
xapParseAny(fileName, "POLYGON", polygonStartHandler, polygonEndHandler, NULL, &obj);
return obj;
}

struct polygonPolygon *polygonPolygonLoadNext(struct xap *xap)
/* Load next polygonPolygon element.  Use xapOpen to get xap. */
{
return xapNext(xap, "POLYGON");
}

void polygonDescriptionFree(struct polygonDescription **pObj)
/* Free up polygonDescription. */
{
struct polygonDescription *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void polygonDescriptionFreeList(struct polygonDescription **pList)
/* Free up list of polygonDescription. */
{
struct polygonDescription *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    polygonDescriptionFree(&el);
    el = next;
    }
}

void polygonDescriptionSave(struct polygonDescription *obj, int indent, FILE *f)
/* Save polygonDescription to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<DESCRIPTION>");
fprintf(f, "%s", obj->text);
fprintf(f, "</DESCRIPTION>\n");
}

struct polygonDescription *polygonDescriptionLoad(char *fileName)
/* Load polygonDescription from XML file where it is root element. */
{
struct polygonDescription *obj;
xapParseAny(fileName, "DESCRIPTION", polygonStartHandler, polygonEndHandler, NULL, &obj);
return obj;
}

struct polygonDescription *polygonDescriptionLoadNext(struct xap *xap)
/* Load next polygonDescription element.  Use xapOpen to get xap. */
{
return xapNext(xap, "DESCRIPTION");
}

void polygonPointFree(struct polygonPoint **pObj)
/* Free up polygonPoint. */
{
struct polygonPoint *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void polygonPointFreeList(struct polygonPoint **pList)
/* Free up list of polygonPoint. */
{
struct polygonPoint *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    polygonPointFree(&el);
    el = next;
    }
}

void polygonPointSave(struct polygonPoint *obj, int indent, FILE *f)
/* Save polygonPoint to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<POINT");
fprintf(f, " x=\"%f\"", obj->x);
fprintf(f, " y=\"%f\"", obj->y);
fprintf(f, " z=\"%f\"", obj->z);
fprintf(f, "/>\n");
}

struct polygonPoint *polygonPointLoad(char *fileName)
/* Load polygonPoint from XML file where it is root element. */
{
struct polygonPoint *obj;
xapParseAny(fileName, "POINT", polygonStartHandler, polygonEndHandler, NULL, &obj);
return obj;
}

struct polygonPoint *polygonPointLoadNext(struct xap *xap)
/* Load next polygonPoint element.  Use xapOpen to get xap. */
{
return xapNext(xap, "POINT");
}

void *polygonStartHandler(struct xap *xp, char *name, char **atts)
/* Called by xap with start tag.  Does most of the parsing work. */
{
struct xapStack *st = xp->stack+1;
int depth = xp->stackDepth;
int i;

if (sameString(name, "POLYGON"))
    {
    struct polygonPolygon *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "id"))
            obj->id = cloneString(val);
        }
    if (obj->id == NULL)
        xapError(xp, "missing id");
    return obj;
    }
else if (sameString(name, "DESCRIPTION"))
    {
    struct polygonDescription *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "POLYGON"))
            {
            struct polygonPolygon *parent = st->object;
            slAddHead(&parent->polygonDescription, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "POINT"))
    {
    struct polygonPoint *obj;
    AllocVar(obj);
    obj->z = 0;
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "x"))
            obj->x = atof(val);
        else if (sameString(name, "y"))
            obj->y = atof(val);
        else if (sameString(name, "z"))
            obj->z = atof(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "POLYGON"))
            {
            struct polygonPolygon *parent = st->object;
            slAddHead(&parent->polygonPoint, obj);
            }
        }
    return obj;
    }
else
    {
    xapSkip(xp);
    return NULL;
    }
}

void polygonEndHandler(struct xap *xp, char *name)
/* Called by xap with end tag.  Checks all required children are loaded. */
{
struct xapStack *stack = xp->stack;
if (sameString(name, "POLYGON"))
    {
    struct polygonPolygon *obj = stack->object;
    if (obj->polygonDescription != NULL && obj->polygonDescription->next != NULL)
        xapError(xp, "Multiple DESCRIPTION");
    if (obj->polygonPoint == NULL)
        xapError(xp, "Missing POINT");
    slReverse(&obj->polygonPoint);
    }
else if (sameString(name, "DESCRIPTION"))
    {
    struct polygonDescription *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
}


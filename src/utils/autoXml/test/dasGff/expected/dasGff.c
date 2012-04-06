/* dasGff.c autoXml generated file */

#include "common.h"
#include "xap.h"
#include "dasGff.h"


void dasGffDasGffFree(struct dasGffDasGff **pObj)
/* Free up dasGffDasGff. */
{
struct dasGffDasGff *obj = *pObj;
if (obj == NULL) return;
dasGffGffFree(&obj->dasGffGff);
freez(pObj);
}

void dasGffDasGffFreeList(struct dasGffDasGff **pList)
/* Free up list of dasGffDasGff. */
{
struct dasGffDasGff *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffDasGffFree(&el);
    el = next;
    }
}

void dasGffDasGffSave(struct dasGffDasGff *obj, int indent, FILE *f)
/* Save dasGffDasGff to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<DAS_GFF>");
fprintf(f, "\n");
dasGffGffSave(obj->dasGffGff, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</DAS_GFF>\n");
}

struct dasGffDasGff *dasGffDasGffLoad(char *fileName)
/* Load dasGffDasGff from XML file where it is root element. */
{
struct dasGffDasGff *obj;
xapParseAny(fileName, "DAS_GFF", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffDasGff *dasGffDasGffLoadNext(struct xap *xap)
/* Load next dasGffDasGff element.  Use xapOpen to get xap. */
{
return xapNext(xap, "DAS_GFF");
}

void dasGffNumberFree(struct dasGffNumber **pObj)
/* Free up dasGffNumber. */
{
struct dasGffNumber *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void dasGffNumberFreeList(struct dasGffNumber **pList)
/* Free up list of dasGffNumber. */
{
struct dasGffNumber *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffNumberFree(&el);
    el = next;
    }
}

void dasGffNumberSave(struct dasGffNumber *obj, int indent, FILE *f)
/* Save dasGffNumber to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<NUMBER>");
fprintf(f, "%d", obj->text);
fprintf(f, "</NUMBER>\n");
}

struct dasGffNumber *dasGffNumberLoad(char *fileName)
/* Load dasGffNumber from XML file where it is root element. */
{
struct dasGffNumber *obj;
xapParseAny(fileName, "NUMBER", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffNumber *dasGffNumberLoadNext(struct xap *xap)
/* Load next dasGffNumber element.  Use xapOpen to get xap. */
{
return xapNext(xap, "NUMBER");
}

void dasGffGffFree(struct dasGffGff **pObj)
/* Free up dasGffGff. */
{
struct dasGffGff *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->version);
freeMem(obj->href);
dasGffSegmentFreeList(&obj->dasGffSegment);
freez(pObj);
}

void dasGffGffFreeList(struct dasGffGff **pList)
/* Free up list of dasGffGff. */
{
struct dasGffGff *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffGffFree(&el);
    el = next;
    }
}

void dasGffGffSave(struct dasGffGff *obj, int indent, FILE *f)
/* Save dasGffGff to file. */
{
struct dasGffSegment *dasGffSegment;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GFF");
fprintf(f, " version=\"%s\"", obj->version);
fprintf(f, " href=\"%s\"", obj->href);
fprintf(f, ">");
fprintf(f, "\n");
for (dasGffSegment = obj->dasGffSegment; dasGffSegment != NULL; dasGffSegment = dasGffSegment->next)
   {
   dasGffSegmentSave(dasGffSegment, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</GFF>\n");
}

struct dasGffGff *dasGffGffLoad(char *fileName)
/* Load dasGffGff from XML file where it is root element. */
{
struct dasGffGff *obj;
xapParseAny(fileName, "GFF", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffGff *dasGffGffLoadNext(struct xap *xap)
/* Load next dasGffGff element.  Use xapOpen to get xap. */
{
return xapNext(xap, "GFF");
}

void dasGffSegmentFree(struct dasGffSegment **pObj)
/* Free up dasGffSegment. */
{
struct dasGffSegment *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
freeMem(obj->label);
dasGffFeatureFreeList(&obj->dasGffFeature);
freez(pObj);
}

void dasGffSegmentFreeList(struct dasGffSegment **pList)
/* Free up list of dasGffSegment. */
{
struct dasGffSegment *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffSegmentFree(&el);
    el = next;
    }
}

void dasGffSegmentSave(struct dasGffSegment *obj, int indent, FILE *f)
/* Save dasGffSegment to file. */
{
struct dasGffFeature *dasGffFeature;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<SEGMENT");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, " start=\"%d\"", obj->start);
fprintf(f, " stop=\"%d\"", obj->stop);
fprintf(f, " version=\"%f\"", obj->version);
if (obj->label != NULL)
    fprintf(f, " label=\"%s\"", obj->label);
fprintf(f, ">");
fprintf(f, "\n");
for (dasGffFeature = obj->dasGffFeature; dasGffFeature != NULL; dasGffFeature = dasGffFeature->next)
   {
   dasGffFeatureSave(dasGffFeature, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</SEGMENT>\n");
}

struct dasGffSegment *dasGffSegmentLoad(char *fileName)
/* Load dasGffSegment from XML file where it is root element. */
{
struct dasGffSegment *obj;
xapParseAny(fileName, "SEGMENT", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffSegment *dasGffSegmentLoadNext(struct xap *xap)
/* Load next dasGffSegment element.  Use xapOpen to get xap. */
{
return xapNext(xap, "SEGMENT");
}

void dasGffFeatureFree(struct dasGffFeature **pObj)
/* Free up dasGffFeature. */
{
struct dasGffFeature *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
freeMem(obj->label);
freeMem(obj->version);
dasGffTypeFree(&obj->dasGffType);
dasGffMethodFree(&obj->dasGffMethod);
dasGffStartFree(&obj->dasGffStart);
dasGffEndFree(&obj->dasGffEnd);
dasGffScoreFree(&obj->dasGffScore);
dasGffOrientationFree(&obj->dasGffOrientation);
dasGffPhaseFree(&obj->dasGffPhase);
dasGffGroupFree(&obj->dasGffGroup);
freez(pObj);
}

void dasGffFeatureFreeList(struct dasGffFeature **pList)
/* Free up list of dasGffFeature. */
{
struct dasGffFeature *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffFeatureFree(&el);
    el = next;
    }
}

void dasGffFeatureSave(struct dasGffFeature *obj, int indent, FILE *f)
/* Save dasGffFeature to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<FEATURE");
fprintf(f, " id=\"%s\"", obj->id);
if (obj->label != NULL)
    fprintf(f, " label=\"%s\"", obj->label);
if (obj->version != NULL)
    fprintf(f, " version=\"%s\"", obj->version);
fprintf(f, ">");
fprintf(f, "\n");
dasGffTypeSave(obj->dasGffType, indent+2, f);
dasGffMethodSave(obj->dasGffMethod, indent+2, f);
dasGffStartSave(obj->dasGffStart, indent+2, f);
dasGffEndSave(obj->dasGffEnd, indent+2, f);
dasGffScoreSave(obj->dasGffScore, indent+2, f);
dasGffOrientationSave(obj->dasGffOrientation, indent+2, f);
dasGffPhaseSave(obj->dasGffPhase, indent+2, f);
dasGffGroupSave(obj->dasGffGroup, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</FEATURE>\n");
}

struct dasGffFeature *dasGffFeatureLoad(char *fileName)
/* Load dasGffFeature from XML file where it is root element. */
{
struct dasGffFeature *obj;
xapParseAny(fileName, "FEATURE", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffFeature *dasGffFeatureLoadNext(struct xap *xap)
/* Load next dasGffFeature element.  Use xapOpen to get xap. */
{
return xapNext(xap, "FEATURE");
}

void dasGffTypeFree(struct dasGffType **pObj)
/* Free up dasGffType. */
{
struct dasGffType *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
freeMem(obj->category);
freeMem(obj->reference);
freeMem(obj->subparts);
freeMem(obj->text);
freez(pObj);
}

void dasGffTypeFreeList(struct dasGffType **pList)
/* Free up list of dasGffType. */
{
struct dasGffType *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffTypeFree(&el);
    el = next;
    }
}

void dasGffTypeSave(struct dasGffType *obj, int indent, FILE *f)
/* Save dasGffType to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<TYPE");
if (obj->id != NULL)
    fprintf(f, " id=\"%s\"", obj->id);
if (obj->category != NULL)
    fprintf(f, " category=\"%s\"", obj->category);
if (obj->reference != NULL)
    fprintf(f, " reference=\"%s\"", obj->reference);
if (obj->subparts != NULL)
    fprintf(f, " subparts=\"%s\"", obj->subparts);
fprintf(f, ">");
fprintf(f, "%s", obj->text);
fprintf(f, "</TYPE>\n");
}

struct dasGffType *dasGffTypeLoad(char *fileName)
/* Load dasGffType from XML file where it is root element. */
{
struct dasGffType *obj;
xapParseAny(fileName, "TYPE", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffType *dasGffTypeLoadNext(struct xap *xap)
/* Load next dasGffType element.  Use xapOpen to get xap. */
{
return xapNext(xap, "TYPE");
}

void dasGffMethodFree(struct dasGffMethod **pObj)
/* Free up dasGffMethod. */
{
struct dasGffMethod *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
freeMem(obj->text);
freez(pObj);
}

void dasGffMethodFreeList(struct dasGffMethod **pList)
/* Free up list of dasGffMethod. */
{
struct dasGffMethod *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffMethodFree(&el);
    el = next;
    }
}

void dasGffMethodSave(struct dasGffMethod *obj, int indent, FILE *f)
/* Save dasGffMethod to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<METHOD");
if (obj->id != NULL)
    fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
fprintf(f, "%s", obj->text);
fprintf(f, "</METHOD>\n");
}

struct dasGffMethod *dasGffMethodLoad(char *fileName)
/* Load dasGffMethod from XML file where it is root element. */
{
struct dasGffMethod *obj;
xapParseAny(fileName, "METHOD", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffMethod *dasGffMethodLoadNext(struct xap *xap)
/* Load next dasGffMethod element.  Use xapOpen to get xap. */
{
return xapNext(xap, "METHOD");
}

void dasGffStartFree(struct dasGffStart **pObj)
/* Free up dasGffStart. */
{
struct dasGffStart *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void dasGffStartFreeList(struct dasGffStart **pList)
/* Free up list of dasGffStart. */
{
struct dasGffStart *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffStartFree(&el);
    el = next;
    }
}

void dasGffStartSave(struct dasGffStart *obj, int indent, FILE *f)
/* Save dasGffStart to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<START>");
fprintf(f, "%s", obj->text);
fprintf(f, "</START>\n");
}

struct dasGffStart *dasGffStartLoad(char *fileName)
/* Load dasGffStart from XML file where it is root element. */
{
struct dasGffStart *obj;
xapParseAny(fileName, "START", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffStart *dasGffStartLoadNext(struct xap *xap)
/* Load next dasGffStart element.  Use xapOpen to get xap. */
{
return xapNext(xap, "START");
}

void dasGffEndFree(struct dasGffEnd **pObj)
/* Free up dasGffEnd. */
{
struct dasGffEnd *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void dasGffEndFreeList(struct dasGffEnd **pList)
/* Free up list of dasGffEnd. */
{
struct dasGffEnd *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffEndFree(&el);
    el = next;
    }
}

void dasGffEndSave(struct dasGffEnd *obj, int indent, FILE *f)
/* Save dasGffEnd to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<END>");
fprintf(f, "%s", obj->text);
fprintf(f, "</END>\n");
}

struct dasGffEnd *dasGffEndLoad(char *fileName)
/* Load dasGffEnd from XML file where it is root element. */
{
struct dasGffEnd *obj;
xapParseAny(fileName, "END", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffEnd *dasGffEndLoadNext(struct xap *xap)
/* Load next dasGffEnd element.  Use xapOpen to get xap. */
{
return xapNext(xap, "END");
}

void dasGffScoreFree(struct dasGffScore **pObj)
/* Free up dasGffScore. */
{
struct dasGffScore *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void dasGffScoreFreeList(struct dasGffScore **pList)
/* Free up list of dasGffScore. */
{
struct dasGffScore *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffScoreFree(&el);
    el = next;
    }
}

void dasGffScoreSave(struct dasGffScore *obj, int indent, FILE *f)
/* Save dasGffScore to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<SCORE>");
fprintf(f, "%s", obj->text);
fprintf(f, "</SCORE>\n");
}

struct dasGffScore *dasGffScoreLoad(char *fileName)
/* Load dasGffScore from XML file where it is root element. */
{
struct dasGffScore *obj;
xapParseAny(fileName, "SCORE", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffScore *dasGffScoreLoadNext(struct xap *xap)
/* Load next dasGffScore element.  Use xapOpen to get xap. */
{
return xapNext(xap, "SCORE");
}

void dasGffOrientationFree(struct dasGffOrientation **pObj)
/* Free up dasGffOrientation. */
{
struct dasGffOrientation *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void dasGffOrientationFreeList(struct dasGffOrientation **pList)
/* Free up list of dasGffOrientation. */
{
struct dasGffOrientation *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffOrientationFree(&el);
    el = next;
    }
}

void dasGffOrientationSave(struct dasGffOrientation *obj, int indent, FILE *f)
/* Save dasGffOrientation to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<ORIENTATION>");
fprintf(f, "%s", obj->text);
fprintf(f, "</ORIENTATION>\n");
}

struct dasGffOrientation *dasGffOrientationLoad(char *fileName)
/* Load dasGffOrientation from XML file where it is root element. */
{
struct dasGffOrientation *obj;
xapParseAny(fileName, "ORIENTATION", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffOrientation *dasGffOrientationLoadNext(struct xap *xap)
/* Load next dasGffOrientation element.  Use xapOpen to get xap. */
{
return xapNext(xap, "ORIENTATION");
}

void dasGffPhaseFree(struct dasGffPhase **pObj)
/* Free up dasGffPhase. */
{
struct dasGffPhase *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void dasGffPhaseFreeList(struct dasGffPhase **pList)
/* Free up list of dasGffPhase. */
{
struct dasGffPhase *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffPhaseFree(&el);
    el = next;
    }
}

void dasGffPhaseSave(struct dasGffPhase *obj, int indent, FILE *f)
/* Save dasGffPhase to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<PHASE>");
fprintf(f, "%s", obj->text);
fprintf(f, "</PHASE>\n");
}

struct dasGffPhase *dasGffPhaseLoad(char *fileName)
/* Load dasGffPhase from XML file where it is root element. */
{
struct dasGffPhase *obj;
xapParseAny(fileName, "PHASE", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffPhase *dasGffPhaseLoadNext(struct xap *xap)
/* Load next dasGffPhase element.  Use xapOpen to get xap. */
{
return xapNext(xap, "PHASE");
}

void dasGffGroupFree(struct dasGffGroup **pObj)
/* Free up dasGffGroup. */
{
struct dasGffGroup *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
dasGffNoteFree(&obj->dasGffNote);
dasGffLinkFree(&obj->dasGffLink);
dasGffTargetFree(&obj->dasGffTarget);
freez(pObj);
}

void dasGffGroupFreeList(struct dasGffGroup **pList)
/* Free up list of dasGffGroup. */
{
struct dasGffGroup *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffGroupFree(&el);
    el = next;
    }
}

void dasGffGroupSave(struct dasGffGroup *obj, int indent, FILE *f)
/* Save dasGffGroup to file. */
{
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GROUP");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
if (obj->dasGffNote != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    dasGffNoteSave(obj->dasGffNote, indent+2, f);
    }
if (obj->dasGffLink != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    dasGffLinkSave(obj->dasGffLink, indent+2, f);
    }
if (obj->dasGffTarget != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    dasGffTargetSave(obj->dasGffTarget, indent+2, f);
    }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</GROUP>\n");
}

struct dasGffGroup *dasGffGroupLoad(char *fileName)
/* Load dasGffGroup from XML file where it is root element. */
{
struct dasGffGroup *obj;
xapParseAny(fileName, "GROUP", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffGroup *dasGffGroupLoadNext(struct xap *xap)
/* Load next dasGffGroup element.  Use xapOpen to get xap. */
{
return xapNext(xap, "GROUP");
}

void dasGffNoteFree(struct dasGffNote **pObj)
/* Free up dasGffNote. */
{
struct dasGffNote *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void dasGffNoteFreeList(struct dasGffNote **pList)
/* Free up list of dasGffNote. */
{
struct dasGffNote *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffNoteFree(&el);
    el = next;
    }
}

void dasGffNoteSave(struct dasGffNote *obj, int indent, FILE *f)
/* Save dasGffNote to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<NOTE>");
fprintf(f, "%s", obj->text);
fprintf(f, "</NOTE>\n");
}

struct dasGffNote *dasGffNoteLoad(char *fileName)
/* Load dasGffNote from XML file where it is root element. */
{
struct dasGffNote *obj;
xapParseAny(fileName, "NOTE", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffNote *dasGffNoteLoadNext(struct xap *xap)
/* Load next dasGffNote element.  Use xapOpen to get xap. */
{
return xapNext(xap, "NOTE");
}

void dasGffLinkFree(struct dasGffLink **pObj)
/* Free up dasGffLink. */
{
struct dasGffLink *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->href);
freeMem(obj->text);
freez(pObj);
}

void dasGffLinkFreeList(struct dasGffLink **pList)
/* Free up list of dasGffLink. */
{
struct dasGffLink *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffLinkFree(&el);
    el = next;
    }
}

void dasGffLinkSave(struct dasGffLink *obj, int indent, FILE *f)
/* Save dasGffLink to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<LINK");
fprintf(f, " href=\"%s\"", obj->href);
fprintf(f, ">");
fprintf(f, "%s", obj->text);
fprintf(f, "</LINK>\n");
}

struct dasGffLink *dasGffLinkLoad(char *fileName)
/* Load dasGffLink from XML file where it is root element. */
{
struct dasGffLink *obj;
xapParseAny(fileName, "LINK", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffLink *dasGffLinkLoadNext(struct xap *xap)
/* Load next dasGffLink element.  Use xapOpen to get xap. */
{
return xapNext(xap, "LINK");
}

void dasGffTargetFree(struct dasGffTarget **pObj)
/* Free up dasGffTarget. */
{
struct dasGffTarget *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
freeMem(obj->start);
freeMem(obj->stop);
freeMem(obj->text);
freez(pObj);
}

void dasGffTargetFreeList(struct dasGffTarget **pList)
/* Free up list of dasGffTarget. */
{
struct dasGffTarget *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dasGffTargetFree(&el);
    el = next;
    }
}

void dasGffTargetSave(struct dasGffTarget *obj, int indent, FILE *f)
/* Save dasGffTarget to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<TARGET");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, " start=\"%s\"", obj->start);
fprintf(f, " stop=\"%s\"", obj->stop);
fprintf(f, ">");
fprintf(f, "%s", obj->text);
fprintf(f, "</TARGET>\n");
}

struct dasGffTarget *dasGffTargetLoad(char *fileName)
/* Load dasGffTarget from XML file where it is root element. */
{
struct dasGffTarget *obj;
xapParseAny(fileName, "TARGET", dasGffStartHandler, dasGffEndHandler, NULL, &obj);
return obj;
}

struct dasGffTarget *dasGffTargetLoadNext(struct xap *xap)
/* Load next dasGffTarget element.  Use xapOpen to get xap. */
{
return xapNext(xap, "TARGET");
}

void *dasGffStartHandler(struct xap *xp, char *name, char **atts)
/* Called by xap with start tag.  Does most of the parsing work. */
{
struct xapStack *st = xp->stack+1;
int depth = xp->stackDepth;
int i;

if (sameString(name, "DAS_GFF"))
    {
    struct dasGffDasGff *obj;
    AllocVar(obj);
    return obj;
    }
else if (sameString(name, "NUMBER"))
    {
    struct dasGffNumber *obj;
    AllocVar(obj);
    return obj;
    }
else if (sameString(name, "GFF"))
    {
    struct dasGffGff *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "version"))
            obj->version = cloneString(val);
        else if (sameString(name, "href"))
            obj->href = cloneString(val);
        }
    if (obj->version == NULL)
        xapError(xp, "missing version");
    if (obj->href == NULL)
        xapError(xp, "missing href");
    if (depth > 1)
        {
        if  (sameString(st->elName, "DAS_GFF"))
            {
            struct dasGffDasGff *parent = st->object;
            slAddHead(&parent->dasGffGff, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "SEGMENT"))
    {
    struct dasGffSegment *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "id"))
            obj->id = cloneString(val);
        else if (sameString(name, "start"))
            obj->start = atoi(val);
        else if (sameString(name, "stop"))
            obj->stop = atoi(val);
        else if (sameString(name, "version"))
            obj->version = atof(val);
        else if (sameString(name, "label"))
            obj->label = cloneString(val);
        }
    if (obj->id == NULL)
        xapError(xp, "missing id");
    if (depth > 1)
        {
        if  (sameString(st->elName, "GFF"))
            {
            struct dasGffGff *parent = st->object;
            slAddHead(&parent->dasGffSegment, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "FEATURE"))
    {
    struct dasGffFeature *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "id"))
            obj->id = cloneString(val);
        else if (sameString(name, "label"))
            obj->label = cloneString(val);
        else if (sameString(name, "version"))
            obj->version = cloneString(val);
        }
    if (obj->id == NULL)
        xapError(xp, "missing id");
    if (depth > 1)
        {
        if  (sameString(st->elName, "SEGMENT"))
            {
            struct dasGffSegment *parent = st->object;
            slAddHead(&parent->dasGffFeature, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "TYPE"))
    {
    struct dasGffType *obj;
    AllocVar(obj);
    obj->reference = "no";
    obj->subparts = "no";
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "id"))
            obj->id = cloneString(val);
        else if (sameString(name, "category"))
            obj->category = cloneString(val);
        else if (sameString(name, "reference"))
            obj->reference = cloneString(val);
        else if (sameString(name, "subparts"))
            obj->subparts = cloneString(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "FEATURE"))
            {
            struct dasGffFeature *parent = st->object;
            slAddHead(&parent->dasGffType, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "METHOD"))
    {
    struct dasGffMethod *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "id"))
            obj->id = cloneString(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "FEATURE"))
            {
            struct dasGffFeature *parent = st->object;
            slAddHead(&parent->dasGffMethod, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "START"))
    {
    struct dasGffStart *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "FEATURE"))
            {
            struct dasGffFeature *parent = st->object;
            slAddHead(&parent->dasGffStart, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "END"))
    {
    struct dasGffEnd *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "FEATURE"))
            {
            struct dasGffFeature *parent = st->object;
            slAddHead(&parent->dasGffEnd, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "SCORE"))
    {
    struct dasGffScore *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "FEATURE"))
            {
            struct dasGffFeature *parent = st->object;
            slAddHead(&parent->dasGffScore, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "ORIENTATION"))
    {
    struct dasGffOrientation *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "FEATURE"))
            {
            struct dasGffFeature *parent = st->object;
            slAddHead(&parent->dasGffOrientation, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "PHASE"))
    {
    struct dasGffPhase *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "FEATURE"))
            {
            struct dasGffFeature *parent = st->object;
            slAddHead(&parent->dasGffPhase, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GROUP"))
    {
    struct dasGffGroup *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "id"))
            obj->id = cloneString(val);
        }
    if (obj->id == NULL)
        xapError(xp, "missing id");
    if (depth > 1)
        {
        if  (sameString(st->elName, "FEATURE"))
            {
            struct dasGffFeature *parent = st->object;
            slAddHead(&parent->dasGffGroup, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "NOTE"))
    {
    struct dasGffNote *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GROUP"))
            {
            struct dasGffGroup *parent = st->object;
            slAddHead(&parent->dasGffNote, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "LINK"))
    {
    struct dasGffLink *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "href"))
            obj->href = cloneString(val);
        }
    if (obj->href == NULL)
        xapError(xp, "missing href");
    if (depth > 1)
        {
        if  (sameString(st->elName, "GROUP"))
            {
            struct dasGffGroup *parent = st->object;
            slAddHead(&parent->dasGffLink, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "TARGET"))
    {
    struct dasGffTarget *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "id"))
            obj->id = cloneString(val);
        else if (sameString(name, "start"))
            obj->start = cloneString(val);
        else if (sameString(name, "stop"))
            obj->stop = cloneString(val);
        }
    if (obj->id == NULL)
        xapError(xp, "missing id");
    if (obj->start == NULL)
        xapError(xp, "missing start");
    if (obj->stop == NULL)
        xapError(xp, "missing stop");
    if (depth > 1)
        {
        if  (sameString(st->elName, "GROUP"))
            {
            struct dasGffGroup *parent = st->object;
            slAddHead(&parent->dasGffTarget, obj);
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

void dasGffEndHandler(struct xap *xp, char *name)
/* Called by xap with end tag.  Checks all required children are loaded. */
{
struct xapStack *stack = xp->stack;
if (sameString(name, "DAS_GFF"))
    {
    struct dasGffDasGff *obj = stack->object;
    if (obj->dasGffGff == NULL)
        xapError(xp, "Missing GFF");
    if (obj->dasGffGff->next != NULL)
        xapError(xp, "Multiple GFF");
    }
else if (sameString(name, "NUMBER"))
    {
    struct dasGffNumber *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GFF"))
    {
    struct dasGffGff *obj = stack->object;
    if (obj->dasGffSegment == NULL)
        xapError(xp, "Missing SEGMENT");
    slReverse(&obj->dasGffSegment);
    }
else if (sameString(name, "SEGMENT"))
    {
    struct dasGffSegment *obj = stack->object;
    if (obj->dasGffFeature == NULL)
        xapError(xp, "Missing FEATURE");
    slReverse(&obj->dasGffFeature);
    }
else if (sameString(name, "FEATURE"))
    {
    struct dasGffFeature *obj = stack->object;
    if (obj->dasGffType == NULL)
        xapError(xp, "Missing TYPE");
    if (obj->dasGffType->next != NULL)
        xapError(xp, "Multiple TYPE");
    if (obj->dasGffMethod == NULL)
        xapError(xp, "Missing METHOD");
    if (obj->dasGffMethod->next != NULL)
        xapError(xp, "Multiple METHOD");
    if (obj->dasGffStart == NULL)
        xapError(xp, "Missing START");
    if (obj->dasGffStart->next != NULL)
        xapError(xp, "Multiple START");
    if (obj->dasGffEnd == NULL)
        xapError(xp, "Missing END");
    if (obj->dasGffEnd->next != NULL)
        xapError(xp, "Multiple END");
    if (obj->dasGffScore == NULL)
        xapError(xp, "Missing SCORE");
    if (obj->dasGffScore->next != NULL)
        xapError(xp, "Multiple SCORE");
    if (obj->dasGffOrientation == NULL)
        xapError(xp, "Missing ORIENTATION");
    if (obj->dasGffOrientation->next != NULL)
        xapError(xp, "Multiple ORIENTATION");
    if (obj->dasGffPhase == NULL)
        xapError(xp, "Missing PHASE");
    if (obj->dasGffPhase->next != NULL)
        xapError(xp, "Multiple PHASE");
    if (obj->dasGffGroup != NULL && obj->dasGffGroup->next != NULL)
        xapError(xp, "Multiple GROUP");
    }
else if (sameString(name, "TYPE"))
    {
    struct dasGffType *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "METHOD"))
    {
    struct dasGffMethod *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "START"))
    {
    struct dasGffStart *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "END"))
    {
    struct dasGffEnd *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "SCORE"))
    {
    struct dasGffScore *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "ORIENTATION"))
    {
    struct dasGffOrientation *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "PHASE"))
    {
    struct dasGffPhase *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GROUP"))
    {
    struct dasGffGroup *obj = stack->object;
    if (obj->dasGffNote != NULL && obj->dasGffNote->next != NULL)
        xapError(xp, "Multiple NOTE");
    if (obj->dasGffLink != NULL && obj->dasGffLink->next != NULL)
        xapError(xp, "Multiple LINK");
    if (obj->dasGffTarget != NULL && obj->dasGffTarget->next != NULL)
        xapError(xp, "Multiple TARGET");
    }
else if (sameString(name, "NOTE"))
    {
    struct dasGffNote *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "LINK"))
    {
    struct dasGffLink *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "TARGET"))
    {
    struct dasGffTarget *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
}


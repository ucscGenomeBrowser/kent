/* gs.c autoXml generated file */

#include "common.h"
#include "xap.h"
#include "gs.h"

void *gsStartHandler(struct xap *xp, char *name, char **atts);
/* Called by expat with start tag.  Does most of the parsing work. */

void gsEndHandler(struct xap *xp, char *name);
/* Called by expat with end tag.  Checks all required children are loaded. */


void gsGensatImageSetFree(struct gsGensatImageSet **pObj)
/* Free up gsGensatImageSet. */
{
struct gsGensatImageSet *obj = *pObj;
if (obj == NULL) return;
gsGensatImageFreeList(&obj->gsGensatImage);
freez(pObj);
}

void gsGensatImageSetFreeList(struct gsGensatImageSet **pList)
/* Free up list of gsGensatImageSet. */
{
struct gsGensatImageSet *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageSetFree(&el);
    el = next;
    }
}

void gsGensatImageSetSave(struct gsGensatImageSet *obj, int indent, FILE *f)
/* Save gsGensatImageSet to file. */
{
struct gsGensatImage *gsGensatImage;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageSet>");
for (gsGensatImage = obj->gsGensatImage; gsGensatImage != NULL; gsGensatImage = gsGensatImage->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   gsGensatImageSave(gsGensatImage, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</GensatImageSet>\n");
}

struct gsGensatImageSet *gsGensatImageSetLoad(char *fileName)
/* Load gsGensatImageSet from file. */
{
struct gsGensatImageSet *obj;
xapParseAny(fileName, "GensatImageSet", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsDateStdFree(struct gsDateStd **pObj)
/* Free up gsDateStd. */
{
struct gsDateStd *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsDateStdFreeList(struct gsDateStd **pList)
/* Free up list of gsDateStd. */
{
struct gsDateStd *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsDateStdFree(&el);
    el = next;
    }
}

void gsDateStdSave(struct gsDateStd *obj, int indent, FILE *f)
/* Save gsDateStd to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Date_std>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Date_std>\n");
}

struct gsDateStd *gsDateStdLoad(char *fileName)
/* Load gsDateStd from file. */
{
struct gsDateStd *obj;
xapParseAny(fileName, "Date_std", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsDateFree(struct gsDate **pObj)
/* Free up gsDate. */
{
struct gsDate *obj = *pObj;
if (obj == NULL) return;
gsDateStdFree(&obj->gsDateStd);
freez(pObj);
}

void gsDateFreeList(struct gsDate **pList)
/* Free up list of gsDate. */
{
struct gsDate *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsDateFree(&el);
    el = next;
    }
}

void gsDateSave(struct gsDate *obj, int indent, FILE *f)
/* Save gsDate to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Date>");
fprintf(f, "\n");
gsDateStdSave(obj->gsDateStd, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</Date>\n");
}

struct gsDate *gsDateLoad(char *fileName)
/* Load gsDate from file. */
{
struct gsDate *obj;
xapParseAny(fileName, "Date", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatAnnotationFree(struct gsGensatAnnotation **pObj)
/* Free up gsGensatAnnotation. */
{
struct gsGensatAnnotation *obj = *pObj;
if (obj == NULL) return;
gsGensatAnnotationExpressionLevelFree(&obj->gsGensatAnnotationExpressionLevel);
gsGensatAnnotationExpressionPatternFree(&obj->gsGensatAnnotationExpressionPattern);
gsGensatAnnotationRegionFree(&obj->gsGensatAnnotationRegion);
gsGensatAnnotationCellTypeFree(&obj->gsGensatAnnotationCellType);
gsGensatAnnotationCellSubtypeFree(&obj->gsGensatAnnotationCellSubtype);
gsGensatAnnotationCreateDateFree(&obj->gsGensatAnnotationCreateDate);
gsGensatAnnotationModifiedDateFree(&obj->gsGensatAnnotationModifiedDate);
freez(pObj);
}

void gsGensatAnnotationFreeList(struct gsGensatAnnotation **pList)
/* Free up list of gsGensatAnnotation. */
{
struct gsGensatAnnotation *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatAnnotationFree(&el);
    el = next;
    }
}

void gsGensatAnnotationSave(struct gsGensatAnnotation *obj, int indent, FILE *f)
/* Save gsGensatAnnotation to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation>");
fprintf(f, "\n");
gsGensatAnnotationExpressionLevelSave(obj->gsGensatAnnotationExpressionLevel, indent+2, f);
gsGensatAnnotationExpressionPatternSave(obj->gsGensatAnnotationExpressionPattern, indent+2, f);
gsGensatAnnotationRegionSave(obj->gsGensatAnnotationRegion, indent+2, f);
gsGensatAnnotationCellTypeSave(obj->gsGensatAnnotationCellType, indent+2, f);
gsGensatAnnotationCellSubtypeSave(obj->gsGensatAnnotationCellSubtype, indent+2, f);
gsGensatAnnotationCreateDateSave(obj->gsGensatAnnotationCreateDate, indent+2, f);
gsGensatAnnotationModifiedDateSave(obj->gsGensatAnnotationModifiedDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatAnnotation>\n");
}

struct gsGensatAnnotation *gsGensatAnnotationLoad(char *fileName)
/* Load gsGensatAnnotation from file. */
{
struct gsGensatAnnotation *obj;
xapParseAny(fileName, "GensatAnnotation", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatAnnotationExpressionLevelFree(struct gsGensatAnnotationExpressionLevel **pObj)
/* Free up gsGensatAnnotationExpressionLevel. */
{
struct gsGensatAnnotationExpressionLevel *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->value);
freez(pObj);
}

void gsGensatAnnotationExpressionLevelFreeList(struct gsGensatAnnotationExpressionLevel **pList)
/* Free up list of gsGensatAnnotationExpressionLevel. */
{
struct gsGensatAnnotationExpressionLevel *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatAnnotationExpressionLevelFree(&el);
    el = next;
    }
}

void gsGensatAnnotationExpressionLevelSave(struct gsGensatAnnotationExpressionLevel *obj, int indent, FILE *f)
/* Save gsGensatAnnotationExpressionLevel to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_expression-level");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatAnnotationExpressionLevel *gsGensatAnnotationExpressionLevelLoad(char *fileName)
/* Load gsGensatAnnotationExpressionLevel from file. */
{
struct gsGensatAnnotationExpressionLevel *obj;
xapParseAny(fileName, "GensatAnnotation_expression-level", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatAnnotationExpressionPatternFree(struct gsGensatAnnotationExpressionPattern **pObj)
/* Free up gsGensatAnnotationExpressionPattern. */
{
struct gsGensatAnnotationExpressionPattern *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->value);
freez(pObj);
}

void gsGensatAnnotationExpressionPatternFreeList(struct gsGensatAnnotationExpressionPattern **pList)
/* Free up list of gsGensatAnnotationExpressionPattern. */
{
struct gsGensatAnnotationExpressionPattern *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatAnnotationExpressionPatternFree(&el);
    el = next;
    }
}

void gsGensatAnnotationExpressionPatternSave(struct gsGensatAnnotationExpressionPattern *obj, int indent, FILE *f)
/* Save gsGensatAnnotationExpressionPattern to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_expression-pattern");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatAnnotationExpressionPattern *gsGensatAnnotationExpressionPatternLoad(char *fileName)
/* Load gsGensatAnnotationExpressionPattern from file. */
{
struct gsGensatAnnotationExpressionPattern *obj;
xapParseAny(fileName, "GensatAnnotation_expression-pattern", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatAnnotationRegionFree(struct gsGensatAnnotationRegion **pObj)
/* Free up gsGensatAnnotationRegion. */
{
struct gsGensatAnnotationRegion *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatAnnotationRegionFreeList(struct gsGensatAnnotationRegion **pList)
/* Free up list of gsGensatAnnotationRegion. */
{
struct gsGensatAnnotationRegion *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatAnnotationRegionFree(&el);
    el = next;
    }
}

void gsGensatAnnotationRegionSave(struct gsGensatAnnotationRegion *obj, int indent, FILE *f)
/* Save gsGensatAnnotationRegion to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_region>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatAnnotation_region>\n");
}

struct gsGensatAnnotationRegion *gsGensatAnnotationRegionLoad(char *fileName)
/* Load gsGensatAnnotationRegion from file. */
{
struct gsGensatAnnotationRegion *obj;
xapParseAny(fileName, "GensatAnnotation_region", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatAnnotationCellTypeFree(struct gsGensatAnnotationCellType **pObj)
/* Free up gsGensatAnnotationCellType. */
{
struct gsGensatAnnotationCellType *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatAnnotationCellTypeFreeList(struct gsGensatAnnotationCellType **pList)
/* Free up list of gsGensatAnnotationCellType. */
{
struct gsGensatAnnotationCellType *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatAnnotationCellTypeFree(&el);
    el = next;
    }
}

void gsGensatAnnotationCellTypeSave(struct gsGensatAnnotationCellType *obj, int indent, FILE *f)
/* Save gsGensatAnnotationCellType to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_cell-type>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatAnnotation_cell-type>\n");
}

struct gsGensatAnnotationCellType *gsGensatAnnotationCellTypeLoad(char *fileName)
/* Load gsGensatAnnotationCellType from file. */
{
struct gsGensatAnnotationCellType *obj;
xapParseAny(fileName, "GensatAnnotation_cell-type", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatAnnotationCellSubtypeFree(struct gsGensatAnnotationCellSubtype **pObj)
/* Free up gsGensatAnnotationCellSubtype. */
{
struct gsGensatAnnotationCellSubtype *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatAnnotationCellSubtypeFreeList(struct gsGensatAnnotationCellSubtype **pList)
/* Free up list of gsGensatAnnotationCellSubtype. */
{
struct gsGensatAnnotationCellSubtype *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatAnnotationCellSubtypeFree(&el);
    el = next;
    }
}

void gsGensatAnnotationCellSubtypeSave(struct gsGensatAnnotationCellSubtype *obj, int indent, FILE *f)
/* Save gsGensatAnnotationCellSubtype to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_cell-subtype>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatAnnotation_cell-subtype>\n");
}

struct gsGensatAnnotationCellSubtype *gsGensatAnnotationCellSubtypeLoad(char *fileName)
/* Load gsGensatAnnotationCellSubtype from file. */
{
struct gsGensatAnnotationCellSubtype *obj;
xapParseAny(fileName, "GensatAnnotation_cell-subtype", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatAnnotationCreateDateFree(struct gsGensatAnnotationCreateDate **pObj)
/* Free up gsGensatAnnotationCreateDate. */
{
struct gsGensatAnnotationCreateDate *obj = *pObj;
if (obj == NULL) return;
gsDateFree(&obj->gsDate);
freez(pObj);
}

void gsGensatAnnotationCreateDateFreeList(struct gsGensatAnnotationCreateDate **pList)
/* Free up list of gsGensatAnnotationCreateDate. */
{
struct gsGensatAnnotationCreateDate *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatAnnotationCreateDateFree(&el);
    el = next;
    }
}

void gsGensatAnnotationCreateDateSave(struct gsGensatAnnotationCreateDate *obj, int indent, FILE *f)
/* Save gsGensatAnnotationCreateDate to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_create-date>");
fprintf(f, "\n");
gsDateSave(obj->gsDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatAnnotation_create-date>\n");
}

struct gsGensatAnnotationCreateDate *gsGensatAnnotationCreateDateLoad(char *fileName)
/* Load gsGensatAnnotationCreateDate from file. */
{
struct gsGensatAnnotationCreateDate *obj;
xapParseAny(fileName, "GensatAnnotation_create-date", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatAnnotationModifiedDateFree(struct gsGensatAnnotationModifiedDate **pObj)
/* Free up gsGensatAnnotationModifiedDate. */
{
struct gsGensatAnnotationModifiedDate *obj = *pObj;
if (obj == NULL) return;
gsDateFree(&obj->gsDate);
freez(pObj);
}

void gsGensatAnnotationModifiedDateFreeList(struct gsGensatAnnotationModifiedDate **pList)
/* Free up list of gsGensatAnnotationModifiedDate. */
{
struct gsGensatAnnotationModifiedDate *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatAnnotationModifiedDateFree(&el);
    el = next;
    }
}

void gsGensatAnnotationModifiedDateSave(struct gsGensatAnnotationModifiedDate *obj, int indent, FILE *f)
/* Save gsGensatAnnotationModifiedDate to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_modified-date>");
fprintf(f, "\n");
gsDateSave(obj->gsDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatAnnotation_modified-date>\n");
}

struct gsGensatAnnotationModifiedDate *gsGensatAnnotationModifiedDateLoad(char *fileName)
/* Load gsGensatAnnotationModifiedDate from file. */
{
struct gsGensatAnnotationModifiedDate *obj;
xapParseAny(fileName, "GensatAnnotation_modified-date", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageInfoFree(struct gsGensatImageInfo **pObj)
/* Free up gsGensatImageInfo. */
{
struct gsGensatImageInfo *obj = *pObj;
if (obj == NULL) return;
gsGensatImageInfoFilenameFree(&obj->gsGensatImageInfoFilename);
gsGensatImageInfoWidthFree(&obj->gsGensatImageInfoWidth);
gsGensatImageInfoHeightFree(&obj->gsGensatImageInfoHeight);
freez(pObj);
}

void gsGensatImageInfoFreeList(struct gsGensatImageInfo **pList)
/* Free up list of gsGensatImageInfo. */
{
struct gsGensatImageInfo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageInfoFree(&el);
    el = next;
    }
}

void gsGensatImageInfoSave(struct gsGensatImageInfo *obj, int indent, FILE *f)
/* Save gsGensatImageInfo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageInfo>");
fprintf(f, "\n");
gsGensatImageInfoFilenameSave(obj->gsGensatImageInfoFilename, indent+2, f);
gsGensatImageInfoWidthSave(obj->gsGensatImageInfoWidth, indent+2, f);
gsGensatImageInfoHeightSave(obj->gsGensatImageInfoHeight, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImageInfo>\n");
}

struct gsGensatImageInfo *gsGensatImageInfoLoad(char *fileName)
/* Load gsGensatImageInfo from file. */
{
struct gsGensatImageInfo *obj;
xapParseAny(fileName, "GensatImageInfo", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageInfoFilenameFree(struct gsGensatImageInfoFilename **pObj)
/* Free up gsGensatImageInfoFilename. */
{
struct gsGensatImageInfoFilename *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatImageInfoFilenameFreeList(struct gsGensatImageInfoFilename **pList)
/* Free up list of gsGensatImageInfoFilename. */
{
struct gsGensatImageInfoFilename *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageInfoFilenameFree(&el);
    el = next;
    }
}

void gsGensatImageInfoFilenameSave(struct gsGensatImageInfoFilename *obj, int indent, FILE *f)
/* Save gsGensatImageInfoFilename to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageInfo_filename>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatImageInfo_filename>\n");
}

struct gsGensatImageInfoFilename *gsGensatImageInfoFilenameLoad(char *fileName)
/* Load gsGensatImageInfoFilename from file. */
{
struct gsGensatImageInfoFilename *obj;
xapParseAny(fileName, "GensatImageInfo_filename", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageInfoWidthFree(struct gsGensatImageInfoWidth **pObj)
/* Free up gsGensatImageInfoWidth. */
{
struct gsGensatImageInfoWidth *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void gsGensatImageInfoWidthFreeList(struct gsGensatImageInfoWidth **pList)
/* Free up list of gsGensatImageInfoWidth. */
{
struct gsGensatImageInfoWidth *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageInfoWidthFree(&el);
    el = next;
    }
}

void gsGensatImageInfoWidthSave(struct gsGensatImageInfoWidth *obj, int indent, FILE *f)
/* Save gsGensatImageInfoWidth to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageInfo_width>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatImageInfo_width>\n");
}

struct gsGensatImageInfoWidth *gsGensatImageInfoWidthLoad(char *fileName)
/* Load gsGensatImageInfoWidth from file. */
{
struct gsGensatImageInfoWidth *obj;
xapParseAny(fileName, "GensatImageInfo_width", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageInfoHeightFree(struct gsGensatImageInfoHeight **pObj)
/* Free up gsGensatImageInfoHeight. */
{
struct gsGensatImageInfoHeight *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void gsGensatImageInfoHeightFreeList(struct gsGensatImageInfoHeight **pList)
/* Free up list of gsGensatImageInfoHeight. */
{
struct gsGensatImageInfoHeight *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageInfoHeightFree(&el);
    el = next;
    }
}

void gsGensatImageInfoHeightSave(struct gsGensatImageInfoHeight *obj, int indent, FILE *f)
/* Save gsGensatImageInfoHeight to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageInfo_height>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatImageInfo_height>\n");
}

struct gsGensatImageInfoHeight *gsGensatImageInfoHeightLoad(char *fileName)
/* Load gsGensatImageInfoHeight from file. */
{
struct gsGensatImageInfoHeight *obj;
xapParseAny(fileName, "GensatImageInfo_height", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoFree(struct gsGensatGeneInfo **pObj)
/* Free up gsGensatGeneInfo. */
{
struct gsGensatGeneInfo *obj = *pObj;
if (obj == NULL) return;
gsGensatGeneInfoGeneIdFree(&obj->gsGensatGeneInfoGeneId);
gsGensatGeneInfoGeneSymbolFree(&obj->gsGensatGeneInfoGeneSymbol);
gsGensatGeneInfoGeneNameFree(&obj->gsGensatGeneInfoGeneName);
gsGensatGeneInfoGeneAliasesFree(&obj->gsGensatGeneInfoGeneAliases);
gsGensatGeneInfoBacNameFree(&obj->gsGensatGeneInfoBacName);
gsGensatGeneInfoBacAddressFree(&obj->gsGensatGeneInfoBacAddress);
gsGensatGeneInfoBacMarkerFree(&obj->gsGensatGeneInfoBacMarker);
gsGensatGeneInfoBacCommentFree(&obj->gsGensatGeneInfoBacComment);
freez(pObj);
}

void gsGensatGeneInfoFreeList(struct gsGensatGeneInfo **pList)
/* Free up list of gsGensatGeneInfo. */
{
struct gsGensatGeneInfo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoSave(struct gsGensatGeneInfo *obj, int indent, FILE *f)
/* Save gsGensatGeneInfo to file. */
{
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo>");
if (obj->gsGensatGeneInfoGeneId != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatGeneInfoGeneIdSave(obj->gsGensatGeneInfoGeneId, indent+2, f);
    }
if (obj->gsGensatGeneInfoGeneSymbol != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatGeneInfoGeneSymbolSave(obj->gsGensatGeneInfoGeneSymbol, indent+2, f);
    }
if (obj->gsGensatGeneInfoGeneName != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatGeneInfoGeneNameSave(obj->gsGensatGeneInfoGeneName, indent+2, f);
    }
if (obj->gsGensatGeneInfoGeneAliases != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatGeneInfoGeneAliasesSave(obj->gsGensatGeneInfoGeneAliases, indent+2, f);
    }
if (obj->gsGensatGeneInfoBacName != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatGeneInfoBacNameSave(obj->gsGensatGeneInfoBacName, indent+2, f);
    }
if (obj->gsGensatGeneInfoBacAddress != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatGeneInfoBacAddressSave(obj->gsGensatGeneInfoBacAddress, indent+2, f);
    }
if (obj->gsGensatGeneInfoBacMarker != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatGeneInfoBacMarkerSave(obj->gsGensatGeneInfoBacMarker, indent+2, f);
    }
if (obj->gsGensatGeneInfoBacComment != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatGeneInfoBacCommentSave(obj->gsGensatGeneInfoBacComment, indent+2, f);
    }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</GensatGeneInfo>\n");
}

struct gsGensatGeneInfo *gsGensatGeneInfoLoad(char *fileName)
/* Load gsGensatGeneInfo from file. */
{
struct gsGensatGeneInfo *obj;
xapParseAny(fileName, "GensatGeneInfo", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoGeneIdFree(struct gsGensatGeneInfoGeneId **pObj)
/* Free up gsGensatGeneInfoGeneId. */
{
struct gsGensatGeneInfoGeneId *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void gsGensatGeneInfoGeneIdFreeList(struct gsGensatGeneInfoGeneId **pList)
/* Free up list of gsGensatGeneInfoGeneId. */
{
struct gsGensatGeneInfoGeneId *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoGeneIdFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoGeneIdSave(struct gsGensatGeneInfoGeneId *obj, int indent, FILE *f)
/* Save gsGensatGeneInfoGeneId to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo_gene-id>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatGeneInfo_gene-id>\n");
}

struct gsGensatGeneInfoGeneId *gsGensatGeneInfoGeneIdLoad(char *fileName)
/* Load gsGensatGeneInfoGeneId from file. */
{
struct gsGensatGeneInfoGeneId *obj;
xapParseAny(fileName, "GensatGeneInfo_gene-id", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoGeneSymbolFree(struct gsGensatGeneInfoGeneSymbol **pObj)
/* Free up gsGensatGeneInfoGeneSymbol. */
{
struct gsGensatGeneInfoGeneSymbol *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatGeneInfoGeneSymbolFreeList(struct gsGensatGeneInfoGeneSymbol **pList)
/* Free up list of gsGensatGeneInfoGeneSymbol. */
{
struct gsGensatGeneInfoGeneSymbol *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoGeneSymbolFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoGeneSymbolSave(struct gsGensatGeneInfoGeneSymbol *obj, int indent, FILE *f)
/* Save gsGensatGeneInfoGeneSymbol to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo_gene-symbol>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatGeneInfo_gene-symbol>\n");
}

struct gsGensatGeneInfoGeneSymbol *gsGensatGeneInfoGeneSymbolLoad(char *fileName)
/* Load gsGensatGeneInfoGeneSymbol from file. */
{
struct gsGensatGeneInfoGeneSymbol *obj;
xapParseAny(fileName, "GensatGeneInfo_gene-symbol", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoGeneNameFree(struct gsGensatGeneInfoGeneName **pObj)
/* Free up gsGensatGeneInfoGeneName. */
{
struct gsGensatGeneInfoGeneName *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatGeneInfoGeneNameFreeList(struct gsGensatGeneInfoGeneName **pList)
/* Free up list of gsGensatGeneInfoGeneName. */
{
struct gsGensatGeneInfoGeneName *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoGeneNameFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoGeneNameSave(struct gsGensatGeneInfoGeneName *obj, int indent, FILE *f)
/* Save gsGensatGeneInfoGeneName to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo_gene-name>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatGeneInfo_gene-name>\n");
}

struct gsGensatGeneInfoGeneName *gsGensatGeneInfoGeneNameLoad(char *fileName)
/* Load gsGensatGeneInfoGeneName from file. */
{
struct gsGensatGeneInfoGeneName *obj;
xapParseAny(fileName, "GensatGeneInfo_gene-name", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoGeneAliasesFree(struct gsGensatGeneInfoGeneAliases **pObj)
/* Free up gsGensatGeneInfoGeneAliases. */
{
struct gsGensatGeneInfoGeneAliases *obj = *pObj;
if (obj == NULL) return;
gsGensatGeneInfoGeneAliasesEFreeList(&obj->gsGensatGeneInfoGeneAliasesE);
freez(pObj);
}

void gsGensatGeneInfoGeneAliasesFreeList(struct gsGensatGeneInfoGeneAliases **pList)
/* Free up list of gsGensatGeneInfoGeneAliases. */
{
struct gsGensatGeneInfoGeneAliases *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoGeneAliasesFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoGeneAliasesSave(struct gsGensatGeneInfoGeneAliases *obj, int indent, FILE *f)
/* Save gsGensatGeneInfoGeneAliases to file. */
{
struct gsGensatGeneInfoGeneAliasesE *gsGensatGeneInfoGeneAliasesE;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo_gene-aliases>");
for (gsGensatGeneInfoGeneAliasesE = obj->gsGensatGeneInfoGeneAliasesE; gsGensatGeneInfoGeneAliasesE != NULL; gsGensatGeneInfoGeneAliasesE = gsGensatGeneInfoGeneAliasesE->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   gsGensatGeneInfoGeneAliasesESave(gsGensatGeneInfoGeneAliasesE, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</GensatGeneInfo_gene-aliases>\n");
}

struct gsGensatGeneInfoGeneAliases *gsGensatGeneInfoGeneAliasesLoad(char *fileName)
/* Load gsGensatGeneInfoGeneAliases from file. */
{
struct gsGensatGeneInfoGeneAliases *obj;
xapParseAny(fileName, "GensatGeneInfo_gene-aliases", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoGeneAliasesEFree(struct gsGensatGeneInfoGeneAliasesE **pObj)
/* Free up gsGensatGeneInfoGeneAliasesE. */
{
struct gsGensatGeneInfoGeneAliasesE *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatGeneInfoGeneAliasesEFreeList(struct gsGensatGeneInfoGeneAliasesE **pList)
/* Free up list of gsGensatGeneInfoGeneAliasesE. */
{
struct gsGensatGeneInfoGeneAliasesE *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoGeneAliasesEFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoGeneAliasesESave(struct gsGensatGeneInfoGeneAliasesE *obj, int indent, FILE *f)
/* Save gsGensatGeneInfoGeneAliasesE to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo_gene-aliases_E>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatGeneInfo_gene-aliases_E>\n");
}

struct gsGensatGeneInfoGeneAliasesE *gsGensatGeneInfoGeneAliasesELoad(char *fileName)
/* Load gsGensatGeneInfoGeneAliasesE from file. */
{
struct gsGensatGeneInfoGeneAliasesE *obj;
xapParseAny(fileName, "GensatGeneInfo_gene-aliases_E", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoBacNameFree(struct gsGensatGeneInfoBacName **pObj)
/* Free up gsGensatGeneInfoBacName. */
{
struct gsGensatGeneInfoBacName *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatGeneInfoBacNameFreeList(struct gsGensatGeneInfoBacName **pList)
/* Free up list of gsGensatGeneInfoBacName. */
{
struct gsGensatGeneInfoBacName *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoBacNameFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoBacNameSave(struct gsGensatGeneInfoBacName *obj, int indent, FILE *f)
/* Save gsGensatGeneInfoBacName to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo_bac-name>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatGeneInfo_bac-name>\n");
}

struct gsGensatGeneInfoBacName *gsGensatGeneInfoBacNameLoad(char *fileName)
/* Load gsGensatGeneInfoBacName from file. */
{
struct gsGensatGeneInfoBacName *obj;
xapParseAny(fileName, "GensatGeneInfo_bac-name", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoBacAddressFree(struct gsGensatGeneInfoBacAddress **pObj)
/* Free up gsGensatGeneInfoBacAddress. */
{
struct gsGensatGeneInfoBacAddress *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatGeneInfoBacAddressFreeList(struct gsGensatGeneInfoBacAddress **pList)
/* Free up list of gsGensatGeneInfoBacAddress. */
{
struct gsGensatGeneInfoBacAddress *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoBacAddressFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoBacAddressSave(struct gsGensatGeneInfoBacAddress *obj, int indent, FILE *f)
/* Save gsGensatGeneInfoBacAddress to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo_bac-address>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatGeneInfo_bac-address>\n");
}

struct gsGensatGeneInfoBacAddress *gsGensatGeneInfoBacAddressLoad(char *fileName)
/* Load gsGensatGeneInfoBacAddress from file. */
{
struct gsGensatGeneInfoBacAddress *obj;
xapParseAny(fileName, "GensatGeneInfo_bac-address", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoBacMarkerFree(struct gsGensatGeneInfoBacMarker **pObj)
/* Free up gsGensatGeneInfoBacMarker. */
{
struct gsGensatGeneInfoBacMarker *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatGeneInfoBacMarkerFreeList(struct gsGensatGeneInfoBacMarker **pList)
/* Free up list of gsGensatGeneInfoBacMarker. */
{
struct gsGensatGeneInfoBacMarker *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoBacMarkerFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoBacMarkerSave(struct gsGensatGeneInfoBacMarker *obj, int indent, FILE *f)
/* Save gsGensatGeneInfoBacMarker to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo_bac-marker>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatGeneInfo_bac-marker>\n");
}

struct gsGensatGeneInfoBacMarker *gsGensatGeneInfoBacMarkerLoad(char *fileName)
/* Load gsGensatGeneInfoBacMarker from file. */
{
struct gsGensatGeneInfoBacMarker *obj;
xapParseAny(fileName, "GensatGeneInfo_bac-marker", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatGeneInfoBacCommentFree(struct gsGensatGeneInfoBacComment **pObj)
/* Free up gsGensatGeneInfoBacComment. */
{
struct gsGensatGeneInfoBacComment *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatGeneInfoBacCommentFreeList(struct gsGensatGeneInfoBacComment **pList)
/* Free up list of gsGensatGeneInfoBacComment. */
{
struct gsGensatGeneInfoBacComment *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatGeneInfoBacCommentFree(&el);
    el = next;
    }
}

void gsGensatGeneInfoBacCommentSave(struct gsGensatGeneInfoBacComment *obj, int indent, FILE *f)
/* Save gsGensatGeneInfoBacComment to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatGeneInfo_bac-comment>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatGeneInfo_bac-comment>\n");
}

struct gsGensatGeneInfoBacComment *gsGensatGeneInfoBacCommentLoad(char *fileName)
/* Load gsGensatGeneInfoBacComment from file. */
{
struct gsGensatGeneInfoBacComment *obj;
xapParseAny(fileName, "GensatGeneInfo_bac-comment", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatSequenceInfoFree(struct gsGensatSequenceInfo **pObj)
/* Free up gsGensatSequenceInfo. */
{
struct gsGensatSequenceInfo *obj = *pObj;
if (obj == NULL) return;
gsGensatSequenceInfoGiFree(&obj->gsGensatSequenceInfoGi);
gsGensatSequenceInfoAccessionFree(&obj->gsGensatSequenceInfoAccession);
gsGensatSequenceInfoTaxIdFree(&obj->gsGensatSequenceInfoTaxId);
freez(pObj);
}

void gsGensatSequenceInfoFreeList(struct gsGensatSequenceInfo **pList)
/* Free up list of gsGensatSequenceInfo. */
{
struct gsGensatSequenceInfo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatSequenceInfoFree(&el);
    el = next;
    }
}

void gsGensatSequenceInfoSave(struct gsGensatSequenceInfo *obj, int indent, FILE *f)
/* Save gsGensatSequenceInfo to file. */
{
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatSequenceInfo>");
if (obj->gsGensatSequenceInfoGi != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatSequenceInfoGiSave(obj->gsGensatSequenceInfoGi, indent+2, f);
    }
if (obj->gsGensatSequenceInfoAccession != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatSequenceInfoAccessionSave(obj->gsGensatSequenceInfoAccession, indent+2, f);
    }
if (obj->gsGensatSequenceInfoTaxId != NULL)
    {
    if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
    gsGensatSequenceInfoTaxIdSave(obj->gsGensatSequenceInfoTaxId, indent+2, f);
    }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</GensatSequenceInfo>\n");
}

struct gsGensatSequenceInfo *gsGensatSequenceInfoLoad(char *fileName)
/* Load gsGensatSequenceInfo from file. */
{
struct gsGensatSequenceInfo *obj;
xapParseAny(fileName, "GensatSequenceInfo", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatSequenceInfoGiFree(struct gsGensatSequenceInfoGi **pObj)
/* Free up gsGensatSequenceInfoGi. */
{
struct gsGensatSequenceInfoGi *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void gsGensatSequenceInfoGiFreeList(struct gsGensatSequenceInfoGi **pList)
/* Free up list of gsGensatSequenceInfoGi. */
{
struct gsGensatSequenceInfoGi *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatSequenceInfoGiFree(&el);
    el = next;
    }
}

void gsGensatSequenceInfoGiSave(struct gsGensatSequenceInfoGi *obj, int indent, FILE *f)
/* Save gsGensatSequenceInfoGi to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatSequenceInfo_gi>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatSequenceInfo_gi>\n");
}

struct gsGensatSequenceInfoGi *gsGensatSequenceInfoGiLoad(char *fileName)
/* Load gsGensatSequenceInfoGi from file. */
{
struct gsGensatSequenceInfoGi *obj;
xapParseAny(fileName, "GensatSequenceInfo_gi", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatSequenceInfoAccessionFree(struct gsGensatSequenceInfoAccession **pObj)
/* Free up gsGensatSequenceInfoAccession. */
{
struct gsGensatSequenceInfoAccession *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatSequenceInfoAccessionFreeList(struct gsGensatSequenceInfoAccession **pList)
/* Free up list of gsGensatSequenceInfoAccession. */
{
struct gsGensatSequenceInfoAccession *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatSequenceInfoAccessionFree(&el);
    el = next;
    }
}

void gsGensatSequenceInfoAccessionSave(struct gsGensatSequenceInfoAccession *obj, int indent, FILE *f)
/* Save gsGensatSequenceInfoAccession to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatSequenceInfo_accession>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatSequenceInfo_accession>\n");
}

struct gsGensatSequenceInfoAccession *gsGensatSequenceInfoAccessionLoad(char *fileName)
/* Load gsGensatSequenceInfoAccession from file. */
{
struct gsGensatSequenceInfoAccession *obj;
xapParseAny(fileName, "GensatSequenceInfo_accession", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatSequenceInfoTaxIdFree(struct gsGensatSequenceInfoTaxId **pObj)
/* Free up gsGensatSequenceInfoTaxId. */
{
struct gsGensatSequenceInfoTaxId *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void gsGensatSequenceInfoTaxIdFreeList(struct gsGensatSequenceInfoTaxId **pList)
/* Free up list of gsGensatSequenceInfoTaxId. */
{
struct gsGensatSequenceInfoTaxId *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatSequenceInfoTaxIdFree(&el);
    el = next;
    }
}

void gsGensatSequenceInfoTaxIdSave(struct gsGensatSequenceInfoTaxId *obj, int indent, FILE *f)
/* Save gsGensatSequenceInfoTaxId to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatSequenceInfo_tax-id>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatSequenceInfo_tax-id>\n");
}

struct gsGensatSequenceInfoTaxId *gsGensatSequenceInfoTaxIdLoad(char *fileName)
/* Load gsGensatSequenceInfoTaxId from file. */
{
struct gsGensatSequenceInfoTaxId *obj;
xapParseAny(fileName, "GensatSequenceInfo_tax-id", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageFree(struct gsGensatImage **pObj)
/* Free up gsGensatImage. */
{
struct gsGensatImage *obj = *pObj;
if (obj == NULL) return;
gsGensatImageIdFree(&obj->gsGensatImageId);
gsGensatImageImageInfoFree(&obj->gsGensatImageImageInfo);
gsGensatImageImageTechniqueFree(&obj->gsGensatImageImageTechnique);
gsGensatImageAgeFree(&obj->gsGensatImageAge);
gsGensatImageSexFree(&obj->gsGensatImageSex);
gsGensatImageGeneInfoFree(&obj->gsGensatImageGeneInfo);
gsGensatImageSeqInfoFree(&obj->gsGensatImageSeqInfo);
gsGensatImageSectionPlaneFree(&obj->gsGensatImageSectionPlane);
gsGensatImageSectionLevelFree(&obj->gsGensatImageSectionLevel);
gsGensatImageSacrificeDateFree(&obj->gsGensatImageSacrificeDate);
gsGensatImageSectionDateFree(&obj->gsGensatImageSectionDate);
gsGensatImageAnnotationsFree(&obj->gsGensatImageAnnotations);
freez(pObj);
}

void gsGensatImageFreeList(struct gsGensatImage **pList)
/* Free up list of gsGensatImage. */
{
struct gsGensatImage *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageFree(&el);
    el = next;
    }
}

void gsGensatImageSave(struct gsGensatImage *obj, int indent, FILE *f)
/* Save gsGensatImage to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage>");
fprintf(f, "\n");
gsGensatImageIdSave(obj->gsGensatImageId, indent+2, f);
gsGensatImageImageInfoSave(obj->gsGensatImageImageInfo, indent+2, f);
gsGensatImageImageTechniqueSave(obj->gsGensatImageImageTechnique, indent+2, f);
gsGensatImageAgeSave(obj->gsGensatImageAge, indent+2, f);
gsGensatImageSexSave(obj->gsGensatImageSex, indent+2, f);
gsGensatImageGeneInfoSave(obj->gsGensatImageGeneInfo, indent+2, f);
gsGensatImageSeqInfoSave(obj->gsGensatImageSeqInfo, indent+2, f);
gsGensatImageSectionPlaneSave(obj->gsGensatImageSectionPlane, indent+2, f);
gsGensatImageSectionLevelSave(obj->gsGensatImageSectionLevel, indent+2, f);
gsGensatImageSacrificeDateSave(obj->gsGensatImageSacrificeDate, indent+2, f);
gsGensatImageSectionDateSave(obj->gsGensatImageSectionDate, indent+2, f);
gsGensatImageAnnotationsSave(obj->gsGensatImageAnnotations, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage>\n");
}

struct gsGensatImage *gsGensatImageLoad(char *fileName)
/* Load gsGensatImage from file. */
{
struct gsGensatImage *obj;
xapParseAny(fileName, "GensatImage", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageIdFree(struct gsGensatImageId **pObj)
/* Free up gsGensatImageId. */
{
struct gsGensatImageId *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void gsGensatImageIdFreeList(struct gsGensatImageId **pList)
/* Free up list of gsGensatImageId. */
{
struct gsGensatImageId *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageIdFree(&el);
    el = next;
    }
}

void gsGensatImageIdSave(struct gsGensatImageId *obj, int indent, FILE *f)
/* Save gsGensatImageId to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_id>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatImage_id>\n");
}

struct gsGensatImageId *gsGensatImageIdLoad(char *fileName)
/* Load gsGensatImageId from file. */
{
struct gsGensatImageId *obj;
xapParseAny(fileName, "GensatImage_id", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageImageInfoFree(struct gsGensatImageImageInfo **pObj)
/* Free up gsGensatImageImageInfo. */
{
struct gsGensatImageImageInfo *obj = *pObj;
if (obj == NULL) return;
gsGensatImageImageInfoDirectoryFree(&obj->gsGensatImageImageInfoDirectory);
gsGensatImageImageInfoSubmittorIdFree(&obj->gsGensatImageImageInfoSubmittorId);
gsGensatImageImageInfoFullImgFree(&obj->gsGensatImageImageInfoFullImg);
gsGensatImageImageInfoMedImgFree(&obj->gsGensatImageImageInfoMedImg);
gsGensatImageImageInfoIconImgFree(&obj->gsGensatImageImageInfoIconImg);
freez(pObj);
}

void gsGensatImageImageInfoFreeList(struct gsGensatImageImageInfo **pList)
/* Free up list of gsGensatImageImageInfo. */
{
struct gsGensatImageImageInfo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageImageInfoFree(&el);
    el = next;
    }
}

void gsGensatImageImageInfoSave(struct gsGensatImageImageInfo *obj, int indent, FILE *f)
/* Save gsGensatImageImageInfo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info>");
fprintf(f, "\n");
gsGensatImageImageInfoDirectorySave(obj->gsGensatImageImageInfoDirectory, indent+2, f);
gsGensatImageImageInfoSubmittorIdSave(obj->gsGensatImageImageInfoSubmittorId, indent+2, f);
gsGensatImageImageInfoFullImgSave(obj->gsGensatImageImageInfoFullImg, indent+2, f);
gsGensatImageImageInfoMedImgSave(obj->gsGensatImageImageInfoMedImg, indent+2, f);
gsGensatImageImageInfoIconImgSave(obj->gsGensatImageImageInfoIconImg, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_image-info>\n");
}

struct gsGensatImageImageInfo *gsGensatImageImageInfoLoad(char *fileName)
/* Load gsGensatImageImageInfo from file. */
{
struct gsGensatImageImageInfo *obj;
xapParseAny(fileName, "GensatImage_image-info", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageImageInfoDirectoryFree(struct gsGensatImageImageInfoDirectory **pObj)
/* Free up gsGensatImageImageInfoDirectory. */
{
struct gsGensatImageImageInfoDirectory *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatImageImageInfoDirectoryFreeList(struct gsGensatImageImageInfoDirectory **pList)
/* Free up list of gsGensatImageImageInfoDirectory. */
{
struct gsGensatImageImageInfoDirectory *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageImageInfoDirectoryFree(&el);
    el = next;
    }
}

void gsGensatImageImageInfoDirectorySave(struct gsGensatImageImageInfoDirectory *obj, int indent, FILE *f)
/* Save gsGensatImageImageInfoDirectory to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info_directory>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatImage_image-info_directory>\n");
}

struct gsGensatImageImageInfoDirectory *gsGensatImageImageInfoDirectoryLoad(char *fileName)
/* Load gsGensatImageImageInfoDirectory from file. */
{
struct gsGensatImageImageInfoDirectory *obj;
xapParseAny(fileName, "GensatImage_image-info_directory", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageImageInfoSubmittorIdFree(struct gsGensatImageImageInfoSubmittorId **pObj)
/* Free up gsGensatImageImageInfoSubmittorId. */
{
struct gsGensatImageImageInfoSubmittorId *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatImageImageInfoSubmittorIdFreeList(struct gsGensatImageImageInfoSubmittorId **pList)
/* Free up list of gsGensatImageImageInfoSubmittorId. */
{
struct gsGensatImageImageInfoSubmittorId *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageImageInfoSubmittorIdFree(&el);
    el = next;
    }
}

void gsGensatImageImageInfoSubmittorIdSave(struct gsGensatImageImageInfoSubmittorId *obj, int indent, FILE *f)
/* Save gsGensatImageImageInfoSubmittorId to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info_submittor-id>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatImage_image-info_submittor-id>\n");
}

struct gsGensatImageImageInfoSubmittorId *gsGensatImageImageInfoSubmittorIdLoad(char *fileName)
/* Load gsGensatImageImageInfoSubmittorId from file. */
{
struct gsGensatImageImageInfoSubmittorId *obj;
xapParseAny(fileName, "GensatImage_image-info_submittor-id", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageImageInfoFullImgFree(struct gsGensatImageImageInfoFullImg **pObj)
/* Free up gsGensatImageImageInfoFullImg. */
{
struct gsGensatImageImageInfoFullImg *obj = *pObj;
if (obj == NULL) return;
gsGensatImageInfoFree(&obj->gsGensatImageInfo);
freez(pObj);
}

void gsGensatImageImageInfoFullImgFreeList(struct gsGensatImageImageInfoFullImg **pList)
/* Free up list of gsGensatImageImageInfoFullImg. */
{
struct gsGensatImageImageInfoFullImg *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageImageInfoFullImgFree(&el);
    el = next;
    }
}

void gsGensatImageImageInfoFullImgSave(struct gsGensatImageImageInfoFullImg *obj, int indent, FILE *f)
/* Save gsGensatImageImageInfoFullImg to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info_full-img>");
fprintf(f, "\n");
gsGensatImageInfoSave(obj->gsGensatImageInfo, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_image-info_full-img>\n");
}

struct gsGensatImageImageInfoFullImg *gsGensatImageImageInfoFullImgLoad(char *fileName)
/* Load gsGensatImageImageInfoFullImg from file. */
{
struct gsGensatImageImageInfoFullImg *obj;
xapParseAny(fileName, "GensatImage_image-info_full-img", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageImageInfoMedImgFree(struct gsGensatImageImageInfoMedImg **pObj)
/* Free up gsGensatImageImageInfoMedImg. */
{
struct gsGensatImageImageInfoMedImg *obj = *pObj;
if (obj == NULL) return;
gsGensatImageInfoFree(&obj->gsGensatImageInfo);
freez(pObj);
}

void gsGensatImageImageInfoMedImgFreeList(struct gsGensatImageImageInfoMedImg **pList)
/* Free up list of gsGensatImageImageInfoMedImg. */
{
struct gsGensatImageImageInfoMedImg *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageImageInfoMedImgFree(&el);
    el = next;
    }
}

void gsGensatImageImageInfoMedImgSave(struct gsGensatImageImageInfoMedImg *obj, int indent, FILE *f)
/* Save gsGensatImageImageInfoMedImg to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info_med-img>");
fprintf(f, "\n");
gsGensatImageInfoSave(obj->gsGensatImageInfo, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_image-info_med-img>\n");
}

struct gsGensatImageImageInfoMedImg *gsGensatImageImageInfoMedImgLoad(char *fileName)
/* Load gsGensatImageImageInfoMedImg from file. */
{
struct gsGensatImageImageInfoMedImg *obj;
xapParseAny(fileName, "GensatImage_image-info_med-img", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageImageInfoIconImgFree(struct gsGensatImageImageInfoIconImg **pObj)
/* Free up gsGensatImageImageInfoIconImg. */
{
struct gsGensatImageImageInfoIconImg *obj = *pObj;
if (obj == NULL) return;
gsGensatImageInfoFree(&obj->gsGensatImageInfo);
freez(pObj);
}

void gsGensatImageImageInfoIconImgFreeList(struct gsGensatImageImageInfoIconImg **pList)
/* Free up list of gsGensatImageImageInfoIconImg. */
{
struct gsGensatImageImageInfoIconImg *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageImageInfoIconImgFree(&el);
    el = next;
    }
}

void gsGensatImageImageInfoIconImgSave(struct gsGensatImageImageInfoIconImg *obj, int indent, FILE *f)
/* Save gsGensatImageImageInfoIconImg to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info_icon-img>");
fprintf(f, "\n");
gsGensatImageInfoSave(obj->gsGensatImageInfo, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_image-info_icon-img>\n");
}

struct gsGensatImageImageInfoIconImg *gsGensatImageImageInfoIconImgLoad(char *fileName)
/* Load gsGensatImageImageInfoIconImg from file. */
{
struct gsGensatImageImageInfoIconImg *obj;
xapParseAny(fileName, "GensatImage_image-info_icon-img", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageImageTechniqueFree(struct gsGensatImageImageTechnique **pObj)
/* Free up gsGensatImageImageTechnique. */
{
struct gsGensatImageImageTechnique *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->value);
freez(pObj);
}

void gsGensatImageImageTechniqueFreeList(struct gsGensatImageImageTechnique **pList)
/* Free up list of gsGensatImageImageTechnique. */
{
struct gsGensatImageImageTechnique *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageImageTechniqueFree(&el);
    el = next;
    }
}

void gsGensatImageImageTechniqueSave(struct gsGensatImageImageTechnique *obj, int indent, FILE *f)
/* Save gsGensatImageImageTechnique to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-technique");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatImageImageTechnique *gsGensatImageImageTechniqueLoad(char *fileName)
/* Load gsGensatImageImageTechnique from file. */
{
struct gsGensatImageImageTechnique *obj;
xapParseAny(fileName, "GensatImage_image-technique", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageAgeFree(struct gsGensatImageAge **pObj)
/* Free up gsGensatImageAge. */
{
struct gsGensatImageAge *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->value);
freez(pObj);
}

void gsGensatImageAgeFreeList(struct gsGensatImageAge **pList)
/* Free up list of gsGensatImageAge. */
{
struct gsGensatImageAge *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageAgeFree(&el);
    el = next;
    }
}

void gsGensatImageAgeSave(struct gsGensatImageAge *obj, int indent, FILE *f)
/* Save gsGensatImageAge to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_age");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatImageAge *gsGensatImageAgeLoad(char *fileName)
/* Load gsGensatImageAge from file. */
{
struct gsGensatImageAge *obj;
xapParseAny(fileName, "GensatImage_age", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageSexFree(struct gsGensatImageSex **pObj)
/* Free up gsGensatImageSex. */
{
struct gsGensatImageSex *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->value);
freez(pObj);
}

void gsGensatImageSexFreeList(struct gsGensatImageSex **pList)
/* Free up list of gsGensatImageSex. */
{
struct gsGensatImageSex *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageSexFree(&el);
    el = next;
    }
}

void gsGensatImageSexSave(struct gsGensatImageSex *obj, int indent, FILE *f)
/* Save gsGensatImageSex to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_sex");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatImageSex *gsGensatImageSexLoad(char *fileName)
/* Load gsGensatImageSex from file. */
{
struct gsGensatImageSex *obj;
xapParseAny(fileName, "GensatImage_sex", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageGeneInfoFree(struct gsGensatImageGeneInfo **pObj)
/* Free up gsGensatImageGeneInfo. */
{
struct gsGensatImageGeneInfo *obj = *pObj;
if (obj == NULL) return;
gsGensatGeneInfoFree(&obj->gsGensatGeneInfo);
freez(pObj);
}

void gsGensatImageGeneInfoFreeList(struct gsGensatImageGeneInfo **pList)
/* Free up list of gsGensatImageGeneInfo. */
{
struct gsGensatImageGeneInfo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageGeneInfoFree(&el);
    el = next;
    }
}

void gsGensatImageGeneInfoSave(struct gsGensatImageGeneInfo *obj, int indent, FILE *f)
/* Save gsGensatImageGeneInfo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_gene-info>");
fprintf(f, "\n");
gsGensatGeneInfoSave(obj->gsGensatGeneInfo, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_gene-info>\n");
}

struct gsGensatImageGeneInfo *gsGensatImageGeneInfoLoad(char *fileName)
/* Load gsGensatImageGeneInfo from file. */
{
struct gsGensatImageGeneInfo *obj;
xapParseAny(fileName, "GensatImage_gene-info", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageSeqInfoFree(struct gsGensatImageSeqInfo **pObj)
/* Free up gsGensatImageSeqInfo. */
{
struct gsGensatImageSeqInfo *obj = *pObj;
if (obj == NULL) return;
gsGensatSequenceInfoFree(&obj->gsGensatSequenceInfo);
freez(pObj);
}

void gsGensatImageSeqInfoFreeList(struct gsGensatImageSeqInfo **pList)
/* Free up list of gsGensatImageSeqInfo. */
{
struct gsGensatImageSeqInfo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageSeqInfoFree(&el);
    el = next;
    }
}

void gsGensatImageSeqInfoSave(struct gsGensatImageSeqInfo *obj, int indent, FILE *f)
/* Save gsGensatImageSeqInfo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_seq-info>");
fprintf(f, "\n");
gsGensatSequenceInfoSave(obj->gsGensatSequenceInfo, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_seq-info>\n");
}

struct gsGensatImageSeqInfo *gsGensatImageSeqInfoLoad(char *fileName)
/* Load gsGensatImageSeqInfo from file. */
{
struct gsGensatImageSeqInfo *obj;
xapParseAny(fileName, "GensatImage_seq-info", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageSectionPlaneFree(struct gsGensatImageSectionPlane **pObj)
/* Free up gsGensatImageSectionPlane. */
{
struct gsGensatImageSectionPlane *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->value);
freez(pObj);
}

void gsGensatImageSectionPlaneFreeList(struct gsGensatImageSectionPlane **pList)
/* Free up list of gsGensatImageSectionPlane. */
{
struct gsGensatImageSectionPlane *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageSectionPlaneFree(&el);
    el = next;
    }
}

void gsGensatImageSectionPlaneSave(struct gsGensatImageSectionPlane *obj, int indent, FILE *f)
/* Save gsGensatImageSectionPlane to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_section-plane");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatImageSectionPlane *gsGensatImageSectionPlaneLoad(char *fileName)
/* Load gsGensatImageSectionPlane from file. */
{
struct gsGensatImageSectionPlane *obj;
xapParseAny(fileName, "GensatImage_section-plane", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageSectionLevelFree(struct gsGensatImageSectionLevel **pObj)
/* Free up gsGensatImageSectionLevel. */
{
struct gsGensatImageSectionLevel *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void gsGensatImageSectionLevelFreeList(struct gsGensatImageSectionLevel **pList)
/* Free up list of gsGensatImageSectionLevel. */
{
struct gsGensatImageSectionLevel *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageSectionLevelFree(&el);
    el = next;
    }
}

void gsGensatImageSectionLevelSave(struct gsGensatImageSectionLevel *obj, int indent, FILE *f)
/* Save gsGensatImageSectionLevel to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_section-level>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatImage_section-level>\n");
}

struct gsGensatImageSectionLevel *gsGensatImageSectionLevelLoad(char *fileName)
/* Load gsGensatImageSectionLevel from file. */
{
struct gsGensatImageSectionLevel *obj;
xapParseAny(fileName, "GensatImage_section-level", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageSacrificeDateFree(struct gsGensatImageSacrificeDate **pObj)
/* Free up gsGensatImageSacrificeDate. */
{
struct gsGensatImageSacrificeDate *obj = *pObj;
if (obj == NULL) return;
gsDateFree(&obj->gsDate);
freez(pObj);
}

void gsGensatImageSacrificeDateFreeList(struct gsGensatImageSacrificeDate **pList)
/* Free up list of gsGensatImageSacrificeDate. */
{
struct gsGensatImageSacrificeDate *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageSacrificeDateFree(&el);
    el = next;
    }
}

void gsGensatImageSacrificeDateSave(struct gsGensatImageSacrificeDate *obj, int indent, FILE *f)
/* Save gsGensatImageSacrificeDate to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_sacrifice-date>");
fprintf(f, "\n");
gsDateSave(obj->gsDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_sacrifice-date>\n");
}

struct gsGensatImageSacrificeDate *gsGensatImageSacrificeDateLoad(char *fileName)
/* Load gsGensatImageSacrificeDate from file. */
{
struct gsGensatImageSacrificeDate *obj;
xapParseAny(fileName, "GensatImage_sacrifice-date", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageSectionDateFree(struct gsGensatImageSectionDate **pObj)
/* Free up gsGensatImageSectionDate. */
{
struct gsGensatImageSectionDate *obj = *pObj;
if (obj == NULL) return;
gsDateFree(&obj->gsDate);
freez(pObj);
}

void gsGensatImageSectionDateFreeList(struct gsGensatImageSectionDate **pList)
/* Free up list of gsGensatImageSectionDate. */
{
struct gsGensatImageSectionDate *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageSectionDateFree(&el);
    el = next;
    }
}

void gsGensatImageSectionDateSave(struct gsGensatImageSectionDate *obj, int indent, FILE *f)
/* Save gsGensatImageSectionDate to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_section-date>");
fprintf(f, "\n");
gsDateSave(obj->gsDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_section-date>\n");
}

struct gsGensatImageSectionDate *gsGensatImageSectionDateLoad(char *fileName)
/* Load gsGensatImageSectionDate from file. */
{
struct gsGensatImageSectionDate *obj;
xapParseAny(fileName, "GensatImage_section-date", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatImageAnnotationsFree(struct gsGensatImageAnnotations **pObj)
/* Free up gsGensatImageAnnotations. */
{
struct gsGensatImageAnnotations *obj = *pObj;
if (obj == NULL) return;
gsGensatAnnotationFreeList(&obj->gsGensatAnnotation);
freez(pObj);
}

void gsGensatImageAnnotationsFreeList(struct gsGensatImageAnnotations **pList)
/* Free up list of gsGensatImageAnnotations. */
{
struct gsGensatImageAnnotations *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    gsGensatImageAnnotationsFree(&el);
    el = next;
    }
}

void gsGensatImageAnnotationsSave(struct gsGensatImageAnnotations *obj, int indent, FILE *f)
/* Save gsGensatImageAnnotations to file. */
{
struct gsGensatAnnotation *gsGensatAnnotation;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_annotations>");
for (gsGensatAnnotation = obj->gsGensatAnnotation; gsGensatAnnotation != NULL; gsGensatAnnotation = gsGensatAnnotation->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   gsGensatAnnotationSave(gsGensatAnnotation, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</GensatImage_annotations>\n");
}

struct gsGensatImageAnnotations *gsGensatImageAnnotationsLoad(char *fileName)
/* Load gsGensatImageAnnotations from file. */
{
struct gsGensatImageAnnotations *obj;
xapParseAny(fileName, "GensatImage_annotations", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void *gsStartHandler(struct xap *xp, char *name, char **atts)
/* Called by expat with start tag.  Does most of the parsing work. */
{
struct xapStack *st = xp->stack+1;
int depth = xp->stackDepth;
int i;

if (sameString(name, "GensatImageSet"))
    {
    struct gsGensatImageSet *obj;
    AllocVar(obj);
    return obj;
    }
else if (sameString(name, "Date_std"))
    {
    struct gsDateStd *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Date"))
            {
            struct gsDate *parent = st->object;
            slAddHead(&parent->gsDateStd, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Date"))
    {
    struct gsDate *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation_create-date"))
            {
            struct gsGensatAnnotationCreateDate *parent = st->object;
            slAddHead(&parent->gsDate, obj);
            }
        else if (sameString(st->elName, "GensatAnnotation_modified-date"))
            {
            struct gsGensatAnnotationModifiedDate *parent = st->object;
            slAddHead(&parent->gsDate, obj);
            }
        else if (sameString(st->elName, "GensatImage_sacrifice-date"))
            {
            struct gsGensatImageSacrificeDate *parent = st->object;
            slAddHead(&parent->gsDate, obj);
            }
        else if (sameString(st->elName, "GensatImage_section-date"))
            {
            struct gsGensatImageSectionDate *parent = st->object;
            slAddHead(&parent->gsDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation"))
    {
    struct gsGensatAnnotation *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_annotations"))
            {
            struct gsGensatImageAnnotations *parent = st->object;
            slAddHead(&parent->gsGensatAnnotation, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_expression-level"))
    {
    struct gsGensatAnnotationExpressionLevel *obj;
    AllocVar(obj);
    obj->value = "unknown";
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "value"))
            obj->value = cloneString(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatAnnotation *parent = st->object;
            slAddHead(&parent->gsGensatAnnotationExpressionLevel, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_expression-pattern"))
    {
    struct gsGensatAnnotationExpressionPattern *obj;
    AllocVar(obj);
    obj->value = "unknown";
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "value"))
            obj->value = cloneString(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatAnnotation *parent = st->object;
            slAddHead(&parent->gsGensatAnnotationExpressionPattern, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_region"))
    {
    struct gsGensatAnnotationRegion *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatAnnotation *parent = st->object;
            slAddHead(&parent->gsGensatAnnotationRegion, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_cell-type"))
    {
    struct gsGensatAnnotationCellType *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatAnnotation *parent = st->object;
            slAddHead(&parent->gsGensatAnnotationCellType, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_cell-subtype"))
    {
    struct gsGensatAnnotationCellSubtype *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatAnnotation *parent = st->object;
            slAddHead(&parent->gsGensatAnnotationCellSubtype, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_create-date"))
    {
    struct gsGensatAnnotationCreateDate *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatAnnotation *parent = st->object;
            slAddHead(&parent->gsGensatAnnotationCreateDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_modified-date"))
    {
    struct gsGensatAnnotationModifiedDate *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatAnnotation *parent = st->object;
            slAddHead(&parent->gsGensatAnnotationModifiedDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImageInfo"))
    {
    struct gsGensatImageInfo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info_full-img"))
            {
            struct gsGensatImageImageInfoFullImg *parent = st->object;
            slAddHead(&parent->gsGensatImageInfo, obj);
            }
        else if (sameString(st->elName, "GensatImage_image-info_med-img"))
            {
            struct gsGensatImageImageInfoMedImg *parent = st->object;
            slAddHead(&parent->gsGensatImageInfo, obj);
            }
        else if (sameString(st->elName, "GensatImage_image-info_icon-img"))
            {
            struct gsGensatImageImageInfoIconImg *parent = st->object;
            slAddHead(&parent->gsGensatImageInfo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImageInfo_filename"))
    {
    struct gsGensatImageInfoFilename *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImageInfo"))
            {
            struct gsGensatImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatImageInfoFilename, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImageInfo_width"))
    {
    struct gsGensatImageInfoWidth *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImageInfo"))
            {
            struct gsGensatImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatImageInfoWidth, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImageInfo_height"))
    {
    struct gsGensatImageInfoHeight *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImageInfo"))
            {
            struct gsGensatImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatImageInfoHeight, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo"))
    {
    struct gsGensatGeneInfo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_gene-info"))
            {
            struct gsGensatImageGeneInfo *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo_gene-id"))
    {
    struct gsGensatGeneInfoGeneId *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatGeneInfo"))
            {
            struct gsGensatGeneInfo *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfoGeneId, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo_gene-symbol"))
    {
    struct gsGensatGeneInfoGeneSymbol *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatGeneInfo"))
            {
            struct gsGensatGeneInfo *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfoGeneSymbol, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo_gene-name"))
    {
    struct gsGensatGeneInfoGeneName *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatGeneInfo"))
            {
            struct gsGensatGeneInfo *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfoGeneName, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo_gene-aliases"))
    {
    struct gsGensatGeneInfoGeneAliases *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatGeneInfo"))
            {
            struct gsGensatGeneInfo *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfoGeneAliases, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo_gene-aliases_E"))
    {
    struct gsGensatGeneInfoGeneAliasesE *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatGeneInfo_gene-aliases"))
            {
            struct gsGensatGeneInfoGeneAliases *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfoGeneAliasesE, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo_bac-name"))
    {
    struct gsGensatGeneInfoBacName *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatGeneInfo"))
            {
            struct gsGensatGeneInfo *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfoBacName, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo_bac-address"))
    {
    struct gsGensatGeneInfoBacAddress *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatGeneInfo"))
            {
            struct gsGensatGeneInfo *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfoBacAddress, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo_bac-marker"))
    {
    struct gsGensatGeneInfoBacMarker *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatGeneInfo"))
            {
            struct gsGensatGeneInfo *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfoBacMarker, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatGeneInfo_bac-comment"))
    {
    struct gsGensatGeneInfoBacComment *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatGeneInfo"))
            {
            struct gsGensatGeneInfo *parent = st->object;
            slAddHead(&parent->gsGensatGeneInfoBacComment, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatSequenceInfo"))
    {
    struct gsGensatSequenceInfo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_seq-info"))
            {
            struct gsGensatImageSeqInfo *parent = st->object;
            slAddHead(&parent->gsGensatSequenceInfo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatSequenceInfo_gi"))
    {
    struct gsGensatSequenceInfoGi *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatSequenceInfo"))
            {
            struct gsGensatSequenceInfo *parent = st->object;
            slAddHead(&parent->gsGensatSequenceInfoGi, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatSequenceInfo_accession"))
    {
    struct gsGensatSequenceInfoAccession *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatSequenceInfo"))
            {
            struct gsGensatSequenceInfo *parent = st->object;
            slAddHead(&parent->gsGensatSequenceInfoAccession, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatSequenceInfo_tax-id"))
    {
    struct gsGensatSequenceInfoTaxId *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatSequenceInfo"))
            {
            struct gsGensatSequenceInfo *parent = st->object;
            slAddHead(&parent->gsGensatSequenceInfoTaxId, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage"))
    {
    struct gsGensatImage *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImageSet"))
            {
            struct gsGensatImageSet *parent = st->object;
            slAddHead(&parent->gsGensatImage, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_id"))
    {
    struct gsGensatImageId *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageId, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info"))
    {
    struct gsGensatImageImageInfo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageImageInfo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info_directory"))
    {
    struct gsGensatImageImageInfoDirectory *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info"))
            {
            struct gsGensatImageImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatImageImageInfoDirectory, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info_submittor-id"))
    {
    struct gsGensatImageImageInfoSubmittorId *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info"))
            {
            struct gsGensatImageImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatImageImageInfoSubmittorId, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info_full-img"))
    {
    struct gsGensatImageImageInfoFullImg *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info"))
            {
            struct gsGensatImageImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatImageImageInfoFullImg, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info_med-img"))
    {
    struct gsGensatImageImageInfoMedImg *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info"))
            {
            struct gsGensatImageImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatImageImageInfoMedImg, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info_icon-img"))
    {
    struct gsGensatImageImageInfoIconImg *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info"))
            {
            struct gsGensatImageImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatImageImageInfoIconImg, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-technique"))
    {
    struct gsGensatImageImageTechnique *obj;
    AllocVar(obj);
    obj->value = "bac-brightfield";
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "value"))
            obj->value = cloneString(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageImageTechnique, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_age"))
    {
    struct gsGensatImageAge *obj;
    AllocVar(obj);
    obj->value = "unknown";
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "value"))
            obj->value = cloneString(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageAge, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_sex"))
    {
    struct gsGensatImageSex *obj;
    AllocVar(obj);
    obj->value = "unknown";
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "value"))
            obj->value = cloneString(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageSex, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_gene-info"))
    {
    struct gsGensatImageGeneInfo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageGeneInfo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_seq-info"))
    {
    struct gsGensatImageSeqInfo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageSeqInfo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_section-plane"))
    {
    struct gsGensatImageSectionPlane *obj;
    AllocVar(obj);
    obj->value = "unknown";
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "value"))
            obj->value = cloneString(val);
        }
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageSectionPlane, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_section-level"))
    {
    struct gsGensatImageSectionLevel *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageSectionLevel, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_sacrifice-date"))
    {
    struct gsGensatImageSacrificeDate *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageSacrificeDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_section-date"))
    {
    struct gsGensatImageSectionDate *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageSectionDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_annotations"))
    {
    struct gsGensatImageAnnotations *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatImage *parent = st->object;
            slAddHead(&parent->gsGensatImageAnnotations, obj);
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

void gsEndHandler(struct xap *xp, char *name)
/* Called by expat with end tag.  Checks all required children are loaded. */
{
struct xapStack *stack = xp->stack;
if (sameString(name, "GensatImageSet"))
    {
    struct gsGensatImageSet *obj = stack->object;
    slReverse(&obj->gsGensatImage);
    }
else if (sameString(name, "Date_std"))
    {
    struct gsDateStd *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Date"))
    {
    struct gsDate *obj = stack->object;
    if (obj->gsDateStd == NULL)
        xapError(xp, "Missing Date_std");
    if (obj->gsDateStd->next != NULL)
        xapError(xp, "Multiple Date_std");
    }
else if (sameString(name, "GensatAnnotation"))
    {
    struct gsGensatAnnotation *obj = stack->object;
    if (obj->gsGensatAnnotationExpressionLevel == NULL)
        xapError(xp, "Missing GensatAnnotation_expression-level");
    if (obj->gsGensatAnnotationExpressionLevel->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_expression-level");
    if (obj->gsGensatAnnotationExpressionPattern == NULL)
        xapError(xp, "Missing GensatAnnotation_expression-pattern");
    if (obj->gsGensatAnnotationExpressionPattern->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_expression-pattern");
    if (obj->gsGensatAnnotationRegion != NULL && obj->gsGensatAnnotationRegion->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_region");
    if (obj->gsGensatAnnotationCellType != NULL && obj->gsGensatAnnotationCellType->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_cell-type");
    if (obj->gsGensatAnnotationCellSubtype != NULL && obj->gsGensatAnnotationCellSubtype->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_cell-subtype");
    if (obj->gsGensatAnnotationCreateDate == NULL)
        xapError(xp, "Missing GensatAnnotation_create-date");
    if (obj->gsGensatAnnotationCreateDate->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_create-date");
    if (obj->gsGensatAnnotationModifiedDate == NULL)
        xapError(xp, "Missing GensatAnnotation_modified-date");
    if (obj->gsGensatAnnotationModifiedDate->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_modified-date");
    }
else if (sameString(name, "GensatAnnotation_region"))
    {
    struct gsGensatAnnotationRegion *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatAnnotation_cell-type"))
    {
    struct gsGensatAnnotationCellType *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatAnnotation_cell-subtype"))
    {
    struct gsGensatAnnotationCellSubtype *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatAnnotation_create-date"))
    {
    struct gsGensatAnnotationCreateDate *obj = stack->object;
    if (obj->gsDate == NULL)
        xapError(xp, "Missing Date");
    if (obj->gsDate->next != NULL)
        xapError(xp, "Multiple Date");
    }
else if (sameString(name, "GensatAnnotation_modified-date"))
    {
    struct gsGensatAnnotationModifiedDate *obj = stack->object;
    if (obj->gsDate == NULL)
        xapError(xp, "Missing Date");
    if (obj->gsDate->next != NULL)
        xapError(xp, "Multiple Date");
    }
else if (sameString(name, "GensatImageInfo"))
    {
    struct gsGensatImageInfo *obj = stack->object;
    if (obj->gsGensatImageInfoFilename == NULL)
        xapError(xp, "Missing GensatImageInfo_filename");
    if (obj->gsGensatImageInfoFilename->next != NULL)
        xapError(xp, "Multiple GensatImageInfo_filename");
    if (obj->gsGensatImageInfoWidth == NULL)
        xapError(xp, "Missing GensatImageInfo_width");
    if (obj->gsGensatImageInfoWidth->next != NULL)
        xapError(xp, "Multiple GensatImageInfo_width");
    if (obj->gsGensatImageInfoHeight == NULL)
        xapError(xp, "Missing GensatImageInfo_height");
    if (obj->gsGensatImageInfoHeight->next != NULL)
        xapError(xp, "Multiple GensatImageInfo_height");
    }
else if (sameString(name, "GensatImageInfo_filename"))
    {
    struct gsGensatImageInfoFilename *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatImageInfo_width"))
    {
    struct gsGensatImageInfoWidth *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatImageInfo_height"))
    {
    struct gsGensatImageInfoHeight *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatGeneInfo"))
    {
    struct gsGensatGeneInfo *obj = stack->object;
    if (obj->gsGensatGeneInfoGeneId != NULL && obj->gsGensatGeneInfoGeneId->next != NULL)
        xapError(xp, "Multiple GensatGeneInfo_gene-id");
    if (obj->gsGensatGeneInfoGeneSymbol != NULL && obj->gsGensatGeneInfoGeneSymbol->next != NULL)
        xapError(xp, "Multiple GensatGeneInfo_gene-symbol");
    if (obj->gsGensatGeneInfoGeneName != NULL && obj->gsGensatGeneInfoGeneName->next != NULL)
        xapError(xp, "Multiple GensatGeneInfo_gene-name");
    if (obj->gsGensatGeneInfoGeneAliases != NULL && obj->gsGensatGeneInfoGeneAliases->next != NULL)
        xapError(xp, "Multiple GensatGeneInfo_gene-aliases");
    if (obj->gsGensatGeneInfoBacName != NULL && obj->gsGensatGeneInfoBacName->next != NULL)
        xapError(xp, "Multiple GensatGeneInfo_bac-name");
    if (obj->gsGensatGeneInfoBacAddress != NULL && obj->gsGensatGeneInfoBacAddress->next != NULL)
        xapError(xp, "Multiple GensatGeneInfo_bac-address");
    if (obj->gsGensatGeneInfoBacMarker != NULL && obj->gsGensatGeneInfoBacMarker->next != NULL)
        xapError(xp, "Multiple GensatGeneInfo_bac-marker");
    if (obj->gsGensatGeneInfoBacComment != NULL && obj->gsGensatGeneInfoBacComment->next != NULL)
        xapError(xp, "Multiple GensatGeneInfo_bac-comment");
    }
else if (sameString(name, "GensatGeneInfo_gene-id"))
    {
    struct gsGensatGeneInfoGeneId *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatGeneInfo_gene-symbol"))
    {
    struct gsGensatGeneInfoGeneSymbol *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatGeneInfo_gene-name"))
    {
    struct gsGensatGeneInfoGeneName *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatGeneInfo_gene-aliases"))
    {
    struct gsGensatGeneInfoGeneAliases *obj = stack->object;
    slReverse(&obj->gsGensatGeneInfoGeneAliasesE);
    }
else if (sameString(name, "GensatGeneInfo_gene-aliases_E"))
    {
    struct gsGensatGeneInfoGeneAliasesE *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatGeneInfo_bac-name"))
    {
    struct gsGensatGeneInfoBacName *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatGeneInfo_bac-address"))
    {
    struct gsGensatGeneInfoBacAddress *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatGeneInfo_bac-marker"))
    {
    struct gsGensatGeneInfoBacMarker *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatGeneInfo_bac-comment"))
    {
    struct gsGensatGeneInfoBacComment *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatSequenceInfo"))
    {
    struct gsGensatSequenceInfo *obj = stack->object;
    if (obj->gsGensatSequenceInfoGi != NULL && obj->gsGensatSequenceInfoGi->next != NULL)
        xapError(xp, "Multiple GensatSequenceInfo_gi");
    if (obj->gsGensatSequenceInfoAccession != NULL && obj->gsGensatSequenceInfoAccession->next != NULL)
        xapError(xp, "Multiple GensatSequenceInfo_accession");
    if (obj->gsGensatSequenceInfoTaxId != NULL && obj->gsGensatSequenceInfoTaxId->next != NULL)
        xapError(xp, "Multiple GensatSequenceInfo_tax-id");
    }
else if (sameString(name, "GensatSequenceInfo_gi"))
    {
    struct gsGensatSequenceInfoGi *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatSequenceInfo_accession"))
    {
    struct gsGensatSequenceInfoAccession *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatSequenceInfo_tax-id"))
    {
    struct gsGensatSequenceInfoTaxId *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatImage"))
    {
    struct gsGensatImage *obj = stack->object;
    if (obj->gsGensatImageId == NULL)
        xapError(xp, "Missing GensatImage_id");
    if (obj->gsGensatImageId->next != NULL)
        xapError(xp, "Multiple GensatImage_id");
    if (obj->gsGensatImageImageInfo == NULL)
        xapError(xp, "Missing GensatImage_image-info");
    if (obj->gsGensatImageImageInfo->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info");
    if (obj->gsGensatImageImageTechnique == NULL)
        xapError(xp, "Missing GensatImage_image-technique");
    if (obj->gsGensatImageImageTechnique->next != NULL)
        xapError(xp, "Multiple GensatImage_image-technique");
    if (obj->gsGensatImageAge == NULL)
        xapError(xp, "Missing GensatImage_age");
    if (obj->gsGensatImageAge->next != NULL)
        xapError(xp, "Multiple GensatImage_age");
    if (obj->gsGensatImageSex == NULL)
        xapError(xp, "Missing GensatImage_sex");
    if (obj->gsGensatImageSex->next != NULL)
        xapError(xp, "Multiple GensatImage_sex");
    if (obj->gsGensatImageGeneInfo == NULL)
        xapError(xp, "Missing GensatImage_gene-info");
    if (obj->gsGensatImageGeneInfo->next != NULL)
        xapError(xp, "Multiple GensatImage_gene-info");
    if (obj->gsGensatImageSeqInfo == NULL)
        xapError(xp, "Missing GensatImage_seq-info");
    if (obj->gsGensatImageSeqInfo->next != NULL)
        xapError(xp, "Multiple GensatImage_seq-info");
    if (obj->gsGensatImageSectionPlane == NULL)
        xapError(xp, "Missing GensatImage_section-plane");
    if (obj->gsGensatImageSectionPlane->next != NULL)
        xapError(xp, "Multiple GensatImage_section-plane");
    if (obj->gsGensatImageSectionLevel == NULL)
        xapError(xp, "Missing GensatImage_section-level");
    if (obj->gsGensatImageSectionLevel->next != NULL)
        xapError(xp, "Multiple GensatImage_section-level");
    if (obj->gsGensatImageSacrificeDate == NULL)
        xapError(xp, "Missing GensatImage_sacrifice-date");
    if (obj->gsGensatImageSacrificeDate->next != NULL)
        xapError(xp, "Multiple GensatImage_sacrifice-date");
    if (obj->gsGensatImageSectionDate == NULL)
        xapError(xp, "Missing GensatImage_section-date");
    if (obj->gsGensatImageSectionDate->next != NULL)
        xapError(xp, "Multiple GensatImage_section-date");
    if (obj->gsGensatImageAnnotations != NULL && obj->gsGensatImageAnnotations->next != NULL)
        xapError(xp, "Multiple GensatImage_annotations");
    }
else if (sameString(name, "GensatImage_id"))
    {
    struct gsGensatImageId *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatImage_image-info"))
    {
    struct gsGensatImageImageInfo *obj = stack->object;
    if (obj->gsGensatImageImageInfoDirectory == NULL)
        xapError(xp, "Missing GensatImage_image-info_directory");
    if (obj->gsGensatImageImageInfoDirectory->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info_directory");
    if (obj->gsGensatImageImageInfoSubmittorId == NULL)
        xapError(xp, "Missing GensatImage_image-info_submittor-id");
    if (obj->gsGensatImageImageInfoSubmittorId->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info_submittor-id");
    if (obj->gsGensatImageImageInfoFullImg == NULL)
        xapError(xp, "Missing GensatImage_image-info_full-img");
    if (obj->gsGensatImageImageInfoFullImg->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info_full-img");
    if (obj->gsGensatImageImageInfoMedImg == NULL)
        xapError(xp, "Missing GensatImage_image-info_med-img");
    if (obj->gsGensatImageImageInfoMedImg->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info_med-img");
    if (obj->gsGensatImageImageInfoIconImg == NULL)
        xapError(xp, "Missing GensatImage_image-info_icon-img");
    if (obj->gsGensatImageImageInfoIconImg->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info_icon-img");
    }
else if (sameString(name, "GensatImage_image-info_directory"))
    {
    struct gsGensatImageImageInfoDirectory *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatImage_image-info_submittor-id"))
    {
    struct gsGensatImageImageInfoSubmittorId *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatImage_image-info_full-img"))
    {
    struct gsGensatImageImageInfoFullImg *obj = stack->object;
    if (obj->gsGensatImageInfo == NULL)
        xapError(xp, "Missing GensatImageInfo");
    if (obj->gsGensatImageInfo->next != NULL)
        xapError(xp, "Multiple GensatImageInfo");
    }
else if (sameString(name, "GensatImage_image-info_med-img"))
    {
    struct gsGensatImageImageInfoMedImg *obj = stack->object;
    if (obj->gsGensatImageInfo == NULL)
        xapError(xp, "Missing GensatImageInfo");
    if (obj->gsGensatImageInfo->next != NULL)
        xapError(xp, "Multiple GensatImageInfo");
    }
else if (sameString(name, "GensatImage_image-info_icon-img"))
    {
    struct gsGensatImageImageInfoIconImg *obj = stack->object;
    if (obj->gsGensatImageInfo == NULL)
        xapError(xp, "Missing GensatImageInfo");
    if (obj->gsGensatImageInfo->next != NULL)
        xapError(xp, "Multiple GensatImageInfo");
    }
else if (sameString(name, "GensatImage_gene-info"))
    {
    struct gsGensatImageGeneInfo *obj = stack->object;
    if (obj->gsGensatGeneInfo == NULL)
        xapError(xp, "Missing GensatGeneInfo");
    if (obj->gsGensatGeneInfo->next != NULL)
        xapError(xp, "Multiple GensatGeneInfo");
    }
else if (sameString(name, "GensatImage_seq-info"))
    {
    struct gsGensatImageSeqInfo *obj = stack->object;
    if (obj->gsGensatSequenceInfo == NULL)
        xapError(xp, "Missing GensatSequenceInfo");
    if (obj->gsGensatSequenceInfo->next != NULL)
        xapError(xp, "Multiple GensatSequenceInfo");
    }
else if (sameString(name, "GensatImage_section-level"))
    {
    struct gsGensatImageSectionLevel *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatImage_sacrifice-date"))
    {
    struct gsGensatImageSacrificeDate *obj = stack->object;
    if (obj->gsDate == NULL)
        xapError(xp, "Missing Date");
    if (obj->gsDate->next != NULL)
        xapError(xp, "Multiple Date");
    }
else if (sameString(name, "GensatImage_section-date"))
    {
    struct gsGensatImageSectionDate *obj = stack->object;
    if (obj->gsDate == NULL)
        xapError(xp, "Missing Date");
    if (obj->gsDate->next != NULL)
        xapError(xp, "Multiple Date");
    }
else if (sameString(name, "GensatImage_annotations"))
    {
    struct gsGensatImageAnnotations *obj = stack->object;
    slReverse(&obj->gsGensatAnnotation);
    }
}

#ifdef TESTING
int main(int argc, char *argv[])
/* Test driver for gs routines */
{
struct gsGensatImageSet *obj;
if (argc != 2)
    errAbort("Please run again with a xml filename.");
obj = gsGensatImageSetLoad(argv[1]);
gsGensatImageSetSave(obj, 0, stdout);
gsGensatImageSetFree(&obj);
return 0;
}
#endif /* TESTING */



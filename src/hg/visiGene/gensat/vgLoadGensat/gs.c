/* gs.c autoXml generated file */

#include "common.h"
#include "xap.h"
#include "gs.h"

void *gsStartHandler(struct xap *xp, char *name, char **atts);
/* Called by expat with start tag.  Does most of the parsing work. */

void gsEndHandler(struct xap *xp, char *name);
/* Called by expat with end tag.  Checks all required children are loaded. */


void gsDateStdSave(struct gsDateStd *obj, int indent, FILE *f)
/* Save gsDateStd to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, " ->\n");
}

struct gsDateStd *gsDateStdLoad(char *fileName)
/* Load gsDateStd from file. */
{
struct gsDateStd *obj;
xapParseAny(fileName, "Date_std", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
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

void gsGensatannotationSave(struct gsGensatannotation *obj, int indent, FILE *f)
/* Save gsGensatannotation to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation>");
fprintf(f, "\n");
gsGensatannotationExpressionLevelSave(obj->gsGensatannotationExpressionLevel, indent+2, f);
gsGensatannotationExpressionPatternSave(obj->gsGensatannotationExpressionPattern, indent+2, f);
gsGensatannotationRegionSave(obj->gsGensatannotationRegion, indent+2, f);
gsGensatannotationCellTypeSave(obj->gsGensatannotationCellType, indent+2, f);
gsGensatannotationCellSubtypeSave(obj->gsGensatannotationCellSubtype, indent+2, f);
gsGensatannotationCreateDateSave(obj->gsGensatannotationCreateDate, indent+2, f);
gsGensatannotationModifiedDateSave(obj->gsGensatannotationModifiedDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatAnnotation>\n");
}

struct gsGensatannotation *gsGensatannotationLoad(char *fileName)
/* Load gsGensatannotation from file. */
{
struct gsGensatannotation *obj;
xapParseAny(fileName, "GensatAnnotation", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatannotationExpressionLevelSave(struct gsGensatannotationExpressionLevel *obj, int indent, FILE *f)
/* Save gsGensatannotationExpressionLevel to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_expression-level");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatannotationExpressionLevel *gsGensatannotationExpressionLevelLoad(char *fileName)
/* Load gsGensatannotationExpressionLevel from file. */
{
struct gsGensatannotationExpressionLevel *obj;
xapParseAny(fileName, "GensatAnnotation_expression-level", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatannotationExpressionPatternSave(struct gsGensatannotationExpressionPattern *obj, int indent, FILE *f)
/* Save gsGensatannotationExpressionPattern to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_expression-pattern");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatannotationExpressionPattern *gsGensatannotationExpressionPatternLoad(char *fileName)
/* Load gsGensatannotationExpressionPattern from file. */
{
struct gsGensatannotationExpressionPattern *obj;
xapParseAny(fileName, "GensatAnnotation_expression-pattern", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatannotationRegionSave(struct gsGensatannotationRegion *obj, int indent, FILE *f)
/* Save gsGensatannotationRegion to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_region>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatAnnotation_region>\n");
}

struct gsGensatannotationRegion *gsGensatannotationRegionLoad(char *fileName)
/* Load gsGensatannotationRegion from file. */
{
struct gsGensatannotationRegion *obj;
xapParseAny(fileName, "GensatAnnotation_region", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatannotationCellTypeSave(struct gsGensatannotationCellType *obj, int indent, FILE *f)
/* Save gsGensatannotationCellType to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_cell-type>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatAnnotation_cell-type>\n");
}

struct gsGensatannotationCellType *gsGensatannotationCellTypeLoad(char *fileName)
/* Load gsGensatannotationCellType from file. */
{
struct gsGensatannotationCellType *obj;
xapParseAny(fileName, "GensatAnnotation_cell-type", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatannotationCellSubtypeSave(struct gsGensatannotationCellSubtype *obj, int indent, FILE *f)
/* Save gsGensatannotationCellSubtype to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_cell-subtype>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatAnnotation_cell-subtype>\n");
}

struct gsGensatannotationCellSubtype *gsGensatannotationCellSubtypeLoad(char *fileName)
/* Load gsGensatannotationCellSubtype from file. */
{
struct gsGensatannotationCellSubtype *obj;
xapParseAny(fileName, "GensatAnnotation_cell-subtype", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatannotationCreateDateSave(struct gsGensatannotationCreateDate *obj, int indent, FILE *f)
/* Save gsGensatannotationCreateDate to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_create-date>");
fprintf(f, "\n");
gsDateSave(obj->gsDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatAnnotation_create-date>\n");
}

struct gsGensatannotationCreateDate *gsGensatannotationCreateDateLoad(char *fileName)
/* Load gsGensatannotationCreateDate from file. */
{
struct gsGensatannotationCreateDate *obj;
xapParseAny(fileName, "GensatAnnotation_create-date", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatannotationModifiedDateSave(struct gsGensatannotationModifiedDate *obj, int indent, FILE *f)
/* Save gsGensatannotationModifiedDate to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatAnnotation_modified-date>");
fprintf(f, "\n");
gsDateSave(obj->gsDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatAnnotation_modified-date>\n");
}

struct gsGensatannotationModifiedDate *gsGensatannotationModifiedDateLoad(char *fileName)
/* Load gsGensatannotationModifiedDate from file. */
{
struct gsGensatannotationModifiedDate *obj;
xapParseAny(fileName, "GensatAnnotation_modified-date", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageinfoSave(struct gsGensatimageinfo *obj, int indent, FILE *f)
/* Save gsGensatimageinfo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageInfo>");
fprintf(f, "\n");
gsGensatimageinfoFilenameSave(obj->gsGensatimageinfoFilename, indent+2, f);
gsGensatimageinfoWidthSave(obj->gsGensatimageinfoWidth, indent+2, f);
gsGensatimageinfoHeightSave(obj->gsGensatimageinfoHeight, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImageInfo>\n");
}

struct gsGensatimageinfo *gsGensatimageinfoLoad(char *fileName)
/* Load gsGensatimageinfo from file. */
{
struct gsGensatimageinfo *obj;
xapParseAny(fileName, "GensatImageInfo", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageinfoFilenameSave(struct gsGensatimageinfoFilename *obj, int indent, FILE *f)
/* Save gsGensatimageinfoFilename to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageInfo_filename>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatImageInfo_filename>\n");
}

struct gsGensatimageinfoFilename *gsGensatimageinfoFilenameLoad(char *fileName)
/* Load gsGensatimageinfoFilename from file. */
{
struct gsGensatimageinfoFilename *obj;
xapParseAny(fileName, "GensatImageInfo_filename", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageinfoWidthSave(struct gsGensatimageinfoWidth *obj, int indent, FILE *f)
/* Save gsGensatimageinfoWidth to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageInfo_width>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatImageInfo_width>\n");
}

struct gsGensatimageinfoWidth *gsGensatimageinfoWidthLoad(char *fileName)
/* Load gsGensatimageinfoWidth from file. */
{
struct gsGensatimageinfoWidth *obj;
xapParseAny(fileName, "GensatImageInfo_width", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageinfoHeightSave(struct gsGensatimageinfoHeight *obj, int indent, FILE *f)
/* Save gsGensatimageinfoHeight to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageInfo_height>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatImageInfo_height>\n");
}

struct gsGensatimageinfoHeight *gsGensatimageinfoHeightLoad(char *fileName)
/* Load gsGensatimageinfoHeight from file. */
{
struct gsGensatimageinfoHeight *obj;
xapParseAny(fileName, "GensatImageInfo_height", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageSave(struct gsGensatimage *obj, int indent, FILE *f)
/* Save gsGensatimage to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage>");
fprintf(f, "\n");
gsGensatimageIdSave(obj->gsGensatimageId, indent+2, f);
gsGensatimageImageInfoSave(obj->gsGensatimageImageInfo, indent+2, f);
gsGensatimageImageTechniqueSave(obj->gsGensatimageImageTechnique, indent+2, f);
gsGensatimageAgeSave(obj->gsGensatimageAge, indent+2, f);
gsGensatimageGeneSymbolSave(obj->gsGensatimageGeneSymbol, indent+2, f);
gsGensatimageGeneNameSave(obj->gsGensatimageGeneName, indent+2, f);
gsGensatimageGeneIdSave(obj->gsGensatimageGeneId, indent+2, f);
gsGensatimageGenbankAccSave(obj->gsGensatimageGenbankAcc, indent+2, f);
gsGensatimageSectionPlaneSave(obj->gsGensatimageSectionPlane, indent+2, f);
gsGensatimageSectionLevelSave(obj->gsGensatimageSectionLevel, indent+2, f);
gsGensatimageSacrificeDateSave(obj->gsGensatimageSacrificeDate, indent+2, f);
gsGensatimageSectionDateSave(obj->gsGensatimageSectionDate, indent+2, f);
gsGensatimageAnnotationsSave(obj->gsGensatimageAnnotations, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage>\n");
}

struct gsGensatimage *gsGensatimageLoad(char *fileName)
/* Load gsGensatimage from file. */
{
struct gsGensatimage *obj;
xapParseAny(fileName, "GensatImage", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageIdSave(struct gsGensatimageId *obj, int indent, FILE *f)
/* Save gsGensatimageId to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_id>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatImage_id>\n");
}

struct gsGensatimageId *gsGensatimageIdLoad(char *fileName)
/* Load gsGensatimageId from file. */
{
struct gsGensatimageId *obj;
xapParseAny(fileName, "GensatImage_id", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageImageInfoSave(struct gsGensatimageImageInfo *obj, int indent, FILE *f)
/* Save gsGensatimageImageInfo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info>");
fprintf(f, "\n");
gsGensatimageImageInfoFullImgSave(obj->gsGensatimageImageInfoFullImg, indent+2, f);
gsGensatimageImageInfoMedImgSave(obj->gsGensatimageImageInfoMedImg, indent+2, f);
gsGensatimageImageInfoIconImgSave(obj->gsGensatimageImageInfoIconImg, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_image-info>\n");
}

struct gsGensatimageImageInfo *gsGensatimageImageInfoLoad(char *fileName)
/* Load gsGensatimageImageInfo from file. */
{
struct gsGensatimageImageInfo *obj;
xapParseAny(fileName, "GensatImage_image-info", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageImageInfoFullImgSave(struct gsGensatimageImageInfoFullImg *obj, int indent, FILE *f)
/* Save gsGensatimageImageInfoFullImg to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info_full-img>");
fprintf(f, "\n");
gsGensatimageinfoSave(obj->gsGensatimageinfo, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_image-info_full-img>\n");
}

struct gsGensatimageImageInfoFullImg *gsGensatimageImageInfoFullImgLoad(char *fileName)
/* Load gsGensatimageImageInfoFullImg from file. */
{
struct gsGensatimageImageInfoFullImg *obj;
xapParseAny(fileName, "GensatImage_image-info_full-img", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageImageInfoMedImgSave(struct gsGensatimageImageInfoMedImg *obj, int indent, FILE *f)
/* Save gsGensatimageImageInfoMedImg to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info_med-img>");
fprintf(f, "\n");
gsGensatimageinfoSave(obj->gsGensatimageinfo, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_image-info_med-img>\n");
}

struct gsGensatimageImageInfoMedImg *gsGensatimageImageInfoMedImgLoad(char *fileName)
/* Load gsGensatimageImageInfoMedImg from file. */
{
struct gsGensatimageImageInfoMedImg *obj;
xapParseAny(fileName, "GensatImage_image-info_med-img", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageImageInfoIconImgSave(struct gsGensatimageImageInfoIconImg *obj, int indent, FILE *f)
/* Save gsGensatimageImageInfoIconImg to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-info_icon-img>");
fprintf(f, "\n");
gsGensatimageinfoSave(obj->gsGensatimageinfo, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_image-info_icon-img>\n");
}

struct gsGensatimageImageInfoIconImg *gsGensatimageImageInfoIconImgLoad(char *fileName)
/* Load gsGensatimageImageInfoIconImg from file. */
{
struct gsGensatimageImageInfoIconImg *obj;
xapParseAny(fileName, "GensatImage_image-info_icon-img", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageImageTechniqueSave(struct gsGensatimageImageTechnique *obj, int indent, FILE *f)
/* Save gsGensatimageImageTechnique to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_image-technique");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatimageImageTechnique *gsGensatimageImageTechniqueLoad(char *fileName)
/* Load gsGensatimageImageTechnique from file. */
{
struct gsGensatimageImageTechnique *obj;
xapParseAny(fileName, "GensatImage_image-technique", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageAgeSave(struct gsGensatimageAge *obj, int indent, FILE *f)
/* Save gsGensatimageAge to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_age");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatimageAge *gsGensatimageAgeLoad(char *fileName)
/* Load gsGensatimageAge from file. */
{
struct gsGensatimageAge *obj;
xapParseAny(fileName, "GensatImage_age", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageGeneSymbolSave(struct gsGensatimageGeneSymbol *obj, int indent, FILE *f)
/* Save gsGensatimageGeneSymbol to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_gene-symbol>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatImage_gene-symbol>\n");
}

struct gsGensatimageGeneSymbol *gsGensatimageGeneSymbolLoad(char *fileName)
/* Load gsGensatimageGeneSymbol from file. */
{
struct gsGensatimageGeneSymbol *obj;
xapParseAny(fileName, "GensatImage_gene-symbol", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageGeneNameSave(struct gsGensatimageGeneName *obj, int indent, FILE *f)
/* Save gsGensatimageGeneName to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_gene-name>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatImage_gene-name>\n");
}

struct gsGensatimageGeneName *gsGensatimageGeneNameLoad(char *fileName)
/* Load gsGensatimageGeneName from file. */
{
struct gsGensatimageGeneName *obj;
xapParseAny(fileName, "GensatImage_gene-name", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageGeneIdSave(struct gsGensatimageGeneId *obj, int indent, FILE *f)
/* Save gsGensatimageGeneId to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_gene-id>");
fprintf(f, "%d", obj->text);
fprintf(f, "</GensatImage_gene-id>\n");
}

struct gsGensatimageGeneId *gsGensatimageGeneIdLoad(char *fileName)
/* Load gsGensatimageGeneId from file. */
{
struct gsGensatimageGeneId *obj;
xapParseAny(fileName, "GensatImage_gene-id", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageGenbankAccSave(struct gsGensatimageGenbankAcc *obj, int indent, FILE *f)
/* Save gsGensatimageGenbankAcc to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_genbank-acc>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatImage_genbank-acc>\n");
}

struct gsGensatimageGenbankAcc *gsGensatimageGenbankAccLoad(char *fileName)
/* Load gsGensatimageGenbankAcc from file. */
{
struct gsGensatimageGenbankAcc *obj;
xapParseAny(fileName, "GensatImage_genbank-acc", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageSectionPlaneSave(struct gsGensatimageSectionPlane *obj, int indent, FILE *f)
/* Save gsGensatimageSectionPlane to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_section-plane");
if (obj->value != NULL)
    fprintf(f, " value=\"%s\"", obj->value);
fprintf(f, "->\n");
}

struct gsGensatimageSectionPlane *gsGensatimageSectionPlaneLoad(char *fileName)
/* Load gsGensatimageSectionPlane from file. */
{
struct gsGensatimageSectionPlane *obj;
xapParseAny(fileName, "GensatImage_section-plane", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageSectionLevelSave(struct gsGensatimageSectionLevel *obj, int indent, FILE *f)
/* Save gsGensatimageSectionLevel to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_section-level>");
fprintf(f, "%s", obj->text);
fprintf(f, "</GensatImage_section-level>\n");
}

struct gsGensatimageSectionLevel *gsGensatimageSectionLevelLoad(char *fileName)
/* Load gsGensatimageSectionLevel from file. */
{
struct gsGensatimageSectionLevel *obj;
xapParseAny(fileName, "GensatImage_section-level", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageSacrificeDateSave(struct gsGensatimageSacrificeDate *obj, int indent, FILE *f)
/* Save gsGensatimageSacrificeDate to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_sacrifice-date>");
fprintf(f, "\n");
gsDateSave(obj->gsDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_sacrifice-date>\n");
}

struct gsGensatimageSacrificeDate *gsGensatimageSacrificeDateLoad(char *fileName)
/* Load gsGensatimageSacrificeDate from file. */
{
struct gsGensatimageSacrificeDate *obj;
xapParseAny(fileName, "GensatImage_sacrifice-date", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageSectionDateSave(struct gsGensatimageSectionDate *obj, int indent, FILE *f)
/* Save gsGensatimageSectionDate to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_section-date>");
fprintf(f, "\n");
gsDateSave(obj->gsDate, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</GensatImage_section-date>\n");
}

struct gsGensatimageSectionDate *gsGensatimageSectionDateLoad(char *fileName)
/* Load gsGensatimageSectionDate from file. */
{
struct gsGensatimageSectionDate *obj;
xapParseAny(fileName, "GensatImage_section-date", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimageAnnotationsSave(struct gsGensatimageAnnotations *obj, int indent, FILE *f)
/* Save gsGensatimageAnnotations to file. */
{
struct gsGensatannotation *gsGensatannotation;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImage_annotations>");
for (gsGensatannotation = obj->gsGensatannotation; gsGensatannotation != NULL; gsGensatannotation = gsGensatannotation->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   gsGensatannotationSave(gsGensatannotation, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</GensatImage_annotations>\n");
}

struct gsGensatimageAnnotations *gsGensatimageAnnotationsLoad(char *fileName)
/* Load gsGensatimageAnnotations from file. */
{
struct gsGensatimageAnnotations *obj;
xapParseAny(fileName, "GensatImage_annotations", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void gsGensatimagesetSave(struct gsGensatimageset *obj, int indent, FILE *f)
/* Save gsGensatimageset to file. */
{
struct gsGensatimage *gsGensatimage;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<GensatImageSet>");
for (gsGensatimage = obj->gsGensatimage; gsGensatimage != NULL; gsGensatimage = gsGensatimage->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   gsGensatimageSave(gsGensatimage, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</GensatImageSet>\n");
}

struct gsGensatimageset *gsGensatimagesetLoad(char *fileName)
/* Load gsGensatimageset from file. */
{
struct gsGensatimageset *obj;
xapParseAny(fileName, "GensatImageSet", gsStartHandler, gsEndHandler, NULL, &obj);
return obj;
}

void *gsStartHandler(struct xap *xp, char *name, char **atts)
/* Called by expat with start tag.  Does most of the parsing work. */
{
struct xapStack *st = xp->stack+1;
int depth = xp->stackDepth;
int i;

if (sameString(name, "Date_std"))
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
            struct gsGensatannotationCreateDate *parent = st->object;
            slAddHead(&parent->gsDate, obj);
            }
        else if (sameString(st->elName, "GensatAnnotation_modified-date"))
            {
            struct gsGensatannotationModifiedDate *parent = st->object;
            slAddHead(&parent->gsDate, obj);
            }
        else if (sameString(st->elName, "GensatImage_sacrifice-date"))
            {
            struct gsGensatimageSacrificeDate *parent = st->object;
            slAddHead(&parent->gsDate, obj);
            }
        else if (sameString(st->elName, "GensatImage_section-date"))
            {
            struct gsGensatimageSectionDate *parent = st->object;
            slAddHead(&parent->gsDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation"))
    {
    struct gsGensatannotation *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_annotations"))
            {
            struct gsGensatimageAnnotations *parent = st->object;
            slAddHead(&parent->gsGensatannotation, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_expression-level"))
    {
    struct gsGensatannotationExpressionLevel *obj;
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
            struct gsGensatannotation *parent = st->object;
            slAddHead(&parent->gsGensatannotationExpressionLevel, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_expression-pattern"))
    {
    struct gsGensatannotationExpressionPattern *obj;
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
            struct gsGensatannotation *parent = st->object;
            slAddHead(&parent->gsGensatannotationExpressionPattern, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_region"))
    {
    struct gsGensatannotationRegion *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatannotation *parent = st->object;
            slAddHead(&parent->gsGensatannotationRegion, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_cell-type"))
    {
    struct gsGensatannotationCellType *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatannotation *parent = st->object;
            slAddHead(&parent->gsGensatannotationCellType, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_cell-subtype"))
    {
    struct gsGensatannotationCellSubtype *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatannotation *parent = st->object;
            slAddHead(&parent->gsGensatannotationCellSubtype, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_create-date"))
    {
    struct gsGensatannotationCreateDate *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatannotation *parent = st->object;
            slAddHead(&parent->gsGensatannotationCreateDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatAnnotation_modified-date"))
    {
    struct gsGensatannotationModifiedDate *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatAnnotation"))
            {
            struct gsGensatannotation *parent = st->object;
            slAddHead(&parent->gsGensatannotationModifiedDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImageInfo"))
    {
    struct gsGensatimageinfo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info_full-img"))
            {
            struct gsGensatimageImageInfoFullImg *parent = st->object;
            slAddHead(&parent->gsGensatimageinfo, obj);
            }
        else if (sameString(st->elName, "GensatImage_image-info_med-img"))
            {
            struct gsGensatimageImageInfoMedImg *parent = st->object;
            slAddHead(&parent->gsGensatimageinfo, obj);
            }
        else if (sameString(st->elName, "GensatImage_image-info_icon-img"))
            {
            struct gsGensatimageImageInfoIconImg *parent = st->object;
            slAddHead(&parent->gsGensatimageinfo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImageInfo_filename"))
    {
    struct gsGensatimageinfoFilename *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImageInfo"))
            {
            struct gsGensatimageinfo *parent = st->object;
            slAddHead(&parent->gsGensatimageinfoFilename, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImageInfo_width"))
    {
    struct gsGensatimageinfoWidth *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImageInfo"))
            {
            struct gsGensatimageinfo *parent = st->object;
            slAddHead(&parent->gsGensatimageinfoWidth, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImageInfo_height"))
    {
    struct gsGensatimageinfoHeight *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImageInfo"))
            {
            struct gsGensatimageinfo *parent = st->object;
            slAddHead(&parent->gsGensatimageinfoHeight, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage"))
    {
    struct gsGensatimage *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImageSet"))
            {
            struct gsGensatimageset *parent = st->object;
            slAddHead(&parent->gsGensatimage, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_id"))
    {
    struct gsGensatimageId *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageId, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info"))
    {
    struct gsGensatimageImageInfo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageImageInfo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info_full-img"))
    {
    struct gsGensatimageImageInfoFullImg *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info"))
            {
            struct gsGensatimageImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatimageImageInfoFullImg, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info_med-img"))
    {
    struct gsGensatimageImageInfoMedImg *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info"))
            {
            struct gsGensatimageImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatimageImageInfoMedImg, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-info_icon-img"))
    {
    struct gsGensatimageImageInfoIconImg *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage_image-info"))
            {
            struct gsGensatimageImageInfo *parent = st->object;
            slAddHead(&parent->gsGensatimageImageInfoIconImg, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_image-technique"))
    {
    struct gsGensatimageImageTechnique *obj;
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
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageImageTechnique, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_age"))
    {
    struct gsGensatimageAge *obj;
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
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageAge, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_gene-symbol"))
    {
    struct gsGensatimageGeneSymbol *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageGeneSymbol, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_gene-name"))
    {
    struct gsGensatimageGeneName *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageGeneName, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_gene-id"))
    {
    struct gsGensatimageGeneId *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageGeneId, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_genbank-acc"))
    {
    struct gsGensatimageGenbankAcc *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageGenbankAcc, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_section-plane"))
    {
    struct gsGensatimageSectionPlane *obj;
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
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageSectionPlane, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_section-level"))
    {
    struct gsGensatimageSectionLevel *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageSectionLevel, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_sacrifice-date"))
    {
    struct gsGensatimageSacrificeDate *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageSacrificeDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_section-date"))
    {
    struct gsGensatimageSectionDate *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageSectionDate, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImage_annotations"))
    {
    struct gsGensatimageAnnotations *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "GensatImage"))
            {
            struct gsGensatimage *parent = st->object;
            slAddHead(&parent->gsGensatimageAnnotations, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "GensatImageSet"))
    {
    struct gsGensatimageset *obj;
    AllocVar(obj);
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
if (sameString(name, "Date"))
    {
    struct gsDate *obj = stack->object;
    if (obj->gsDateStd == NULL)
        xapError(xp, "Missing Date_std");
    if (obj->gsDateStd->next != NULL)
        xapError(xp, "Multiple Date_std");
    }
else if (sameString(name, "GensatAnnotation"))
    {
    struct gsGensatannotation *obj = stack->object;
    if (obj->gsGensatannotationExpressionLevel == NULL)
        xapError(xp, "Missing GensatAnnotation_expression-level");
    if (obj->gsGensatannotationExpressionLevel->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_expression-level");
    if (obj->gsGensatannotationExpressionPattern == NULL)
        xapError(xp, "Missing GensatAnnotation_expression-pattern");
    if (obj->gsGensatannotationExpressionPattern->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_expression-pattern");
    if (obj->gsGensatannotationRegion != NULL && obj->gsGensatannotationRegion->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_region");
    if (obj->gsGensatannotationCellType != NULL && obj->gsGensatannotationCellType->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_cell-type");
    if (obj->gsGensatannotationCellSubtype != NULL && obj->gsGensatannotationCellSubtype->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_cell-subtype");
    if (obj->gsGensatannotationCreateDate == NULL)
        xapError(xp, "Missing GensatAnnotation_create-date");
    if (obj->gsGensatannotationCreateDate->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_create-date");
    if (obj->gsGensatannotationModifiedDate == NULL)
        xapError(xp, "Missing GensatAnnotation_modified-date");
    if (obj->gsGensatannotationModifiedDate->next != NULL)
        xapError(xp, "Multiple GensatAnnotation_modified-date");
    }
else if (sameString(name, "GensatAnnotation_region"))
    {
    struct gsGensatannotationRegion *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatAnnotation_cell-type"))
    {
    struct gsGensatannotationCellType *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatAnnotation_cell-subtype"))
    {
    struct gsGensatannotationCellSubtype *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatAnnotation_create-date"))
    {
    struct gsGensatannotationCreateDate *obj = stack->object;
    if (obj->gsDate == NULL)
        xapError(xp, "Missing Date");
    if (obj->gsDate->next != NULL)
        xapError(xp, "Multiple Date");
    }
else if (sameString(name, "GensatAnnotation_modified-date"))
    {
    struct gsGensatannotationModifiedDate *obj = stack->object;
    if (obj->gsDate == NULL)
        xapError(xp, "Missing Date");
    if (obj->gsDate->next != NULL)
        xapError(xp, "Multiple Date");
    }
else if (sameString(name, "GensatImageInfo"))
    {
    struct gsGensatimageinfo *obj = stack->object;
    if (obj->gsGensatimageinfoFilename == NULL)
        xapError(xp, "Missing GensatImageInfo_filename");
    if (obj->gsGensatimageinfoFilename->next != NULL)
        xapError(xp, "Multiple GensatImageInfo_filename");
    if (obj->gsGensatimageinfoWidth == NULL)
        xapError(xp, "Missing GensatImageInfo_width");
    if (obj->gsGensatimageinfoWidth->next != NULL)
        xapError(xp, "Multiple GensatImageInfo_width");
    if (obj->gsGensatimageinfoHeight == NULL)
        xapError(xp, "Missing GensatImageInfo_height");
    if (obj->gsGensatimageinfoHeight->next != NULL)
        xapError(xp, "Multiple GensatImageInfo_height");
    }
else if (sameString(name, "GensatImageInfo_filename"))
    {
    struct gsGensatimageinfoFilename *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatImageInfo_width"))
    {
    struct gsGensatimageinfoWidth *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatImageInfo_height"))
    {
    struct gsGensatimageinfoHeight *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatImage"))
    {
    struct gsGensatimage *obj = stack->object;
    if (obj->gsGensatimageId == NULL)
        xapError(xp, "Missing GensatImage_id");
    if (obj->gsGensatimageId->next != NULL)
        xapError(xp, "Multiple GensatImage_id");
    if (obj->gsGensatimageImageInfo == NULL)
        xapError(xp, "Missing GensatImage_image-info");
    if (obj->gsGensatimageImageInfo->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info");
    if (obj->gsGensatimageImageTechnique == NULL)
        xapError(xp, "Missing GensatImage_image-technique");
    if (obj->gsGensatimageImageTechnique->next != NULL)
        xapError(xp, "Multiple GensatImage_image-technique");
    if (obj->gsGensatimageAge == NULL)
        xapError(xp, "Missing GensatImage_age");
    if (obj->gsGensatimageAge->next != NULL)
        xapError(xp, "Multiple GensatImage_age");
    if (obj->gsGensatimageGeneSymbol == NULL)
        xapError(xp, "Missing GensatImage_gene-symbol");
    if (obj->gsGensatimageGeneSymbol->next != NULL)
        xapError(xp, "Multiple GensatImage_gene-symbol");
    if (obj->gsGensatimageGeneName == NULL)
        xapError(xp, "Missing GensatImage_gene-name");
    if (obj->gsGensatimageGeneName->next != NULL)
        xapError(xp, "Multiple GensatImage_gene-name");
    if (obj->gsGensatimageGeneId != NULL && obj->gsGensatimageGeneId->next != NULL)
        xapError(xp, "Multiple GensatImage_gene-id");
    if (obj->gsGensatimageGenbankAcc != NULL && obj->gsGensatimageGenbankAcc->next != NULL)
        xapError(xp, "Multiple GensatImage_genbank-acc");
    if (obj->gsGensatimageSectionPlane == NULL)
        xapError(xp, "Missing GensatImage_section-plane");
    if (obj->gsGensatimageSectionPlane->next != NULL)
        xapError(xp, "Multiple GensatImage_section-plane");
    if (obj->gsGensatimageSectionLevel == NULL)
        xapError(xp, "Missing GensatImage_section-level");
    if (obj->gsGensatimageSectionLevel->next != NULL)
        xapError(xp, "Multiple GensatImage_section-level");
    if (obj->gsGensatimageSacrificeDate == NULL)
        xapError(xp, "Missing GensatImage_sacrifice-date");
    if (obj->gsGensatimageSacrificeDate->next != NULL)
        xapError(xp, "Multiple GensatImage_sacrifice-date");
    if (obj->gsGensatimageSectionDate == NULL)
        xapError(xp, "Missing GensatImage_section-date");
    if (obj->gsGensatimageSectionDate->next != NULL)
        xapError(xp, "Multiple GensatImage_section-date");
    if (obj->gsGensatimageAnnotations != NULL && obj->gsGensatimageAnnotations->next != NULL)
        xapError(xp, "Multiple GensatImage_annotations");
    }
else if (sameString(name, "GensatImage_id"))
    {
    struct gsGensatimageId *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatImage_image-info"))
    {
    struct gsGensatimageImageInfo *obj = stack->object;
    if (obj->gsGensatimageImageInfoFullImg == NULL)
        xapError(xp, "Missing GensatImage_image-info_full-img");
    if (obj->gsGensatimageImageInfoFullImg->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info_full-img");
    if (obj->gsGensatimageImageInfoMedImg == NULL)
        xapError(xp, "Missing GensatImage_image-info_med-img");
    if (obj->gsGensatimageImageInfoMedImg->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info_med-img");
    if (obj->gsGensatimageImageInfoIconImg == NULL)
        xapError(xp, "Missing GensatImage_image-info_icon-img");
    if (obj->gsGensatimageImageInfoIconImg->next != NULL)
        xapError(xp, "Multiple GensatImage_image-info_icon-img");
    }
else if (sameString(name, "GensatImage_image-info_full-img"))
    {
    struct gsGensatimageImageInfoFullImg *obj = stack->object;
    if (obj->gsGensatimageinfo == NULL)
        xapError(xp, "Missing GensatImageInfo");
    if (obj->gsGensatimageinfo->next != NULL)
        xapError(xp, "Multiple GensatImageInfo");
    }
else if (sameString(name, "GensatImage_image-info_med-img"))
    {
    struct gsGensatimageImageInfoMedImg *obj = stack->object;
    if (obj->gsGensatimageinfo == NULL)
        xapError(xp, "Missing GensatImageInfo");
    if (obj->gsGensatimageinfo->next != NULL)
        xapError(xp, "Multiple GensatImageInfo");
    }
else if (sameString(name, "GensatImage_image-info_icon-img"))
    {
    struct gsGensatimageImageInfoIconImg *obj = stack->object;
    if (obj->gsGensatimageinfo == NULL)
        xapError(xp, "Missing GensatImageInfo");
    if (obj->gsGensatimageinfo->next != NULL)
        xapError(xp, "Multiple GensatImageInfo");
    }
else if (sameString(name, "GensatImage_gene-symbol"))
    {
    struct gsGensatimageGeneSymbol *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatImage_gene-name"))
    {
    struct gsGensatimageGeneName *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatImage_gene-id"))
    {
    struct gsGensatimageGeneId *obj = stack->object;
    obj->text = atoi(stack->text->string);
    }
else if (sameString(name, "GensatImage_genbank-acc"))
    {
    struct gsGensatimageGenbankAcc *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatImage_section-level"))
    {
    struct gsGensatimageSectionLevel *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "GensatImage_sacrifice-date"))
    {
    struct gsGensatimageSacrificeDate *obj = stack->object;
    if (obj->gsDate == NULL)
        xapError(xp, "Missing Date");
    if (obj->gsDate->next != NULL)
        xapError(xp, "Multiple Date");
    }
else if (sameString(name, "GensatImage_section-date"))
    {
    struct gsGensatimageSectionDate *obj = stack->object;
    if (obj->gsDate == NULL)
        xapError(xp, "Missing Date");
    if (obj->gsDate->next != NULL)
        xapError(xp, "Multiple Date");
    }
else if (sameString(name, "GensatImage_annotations"))
    {
    struct gsGensatimageAnnotations *obj = stack->object;
    slReverse(&obj->gsGensatannotation);
    }
else if (sameString(name, "GensatImageSet"))
    {
    struct gsGensatimageset *obj = stack->object;
    slReverse(&obj->gsGensatimage);
    }
}


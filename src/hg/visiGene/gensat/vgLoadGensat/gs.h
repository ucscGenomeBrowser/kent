/* gs.h autoXml generated file */
#ifndef GS_H
#define GS_H

struct gsDateStd
    {
    struct gsDateStd *next;
    };

void gsDateStdSave(struct gsDateStd *obj, int indent, FILE *f);
/* Save gsDateStd to file. */

struct gsDateStd *gsDateStdLoad(char *fileName);
/* Load gsDateStd from file. */

struct gsDate
    {
    struct gsDate *next;
    struct gsDateStd *gsDateStd;	/** Single instance required. **/
    };

void gsDateSave(struct gsDate *obj, int indent, FILE *f);
/* Save gsDate to file. */

struct gsDate *gsDateLoad(char *fileName);
/* Load gsDate from file. */

struct gsGensatannotation
    {
    struct gsGensatannotation *next;
    struct gsGensatannotationExpressionLevel *gsGensatannotationExpressionLevel;	/** Single instance required. **/
    struct gsGensatannotationExpressionPattern *gsGensatannotationExpressionPattern;	/** Single instance required. **/
    struct gsGensatannotationRegion *gsGensatannotationRegion;	/** Optional (may be NULL). **/
    struct gsGensatannotationCellType *gsGensatannotationCellType;	/** Optional (may be NULL). **/
    struct gsGensatannotationCellSubtype *gsGensatannotationCellSubtype;	/** Optional (may be NULL). **/
    struct gsGensatannotationCreateDate *gsGensatannotationCreateDate;	/** Single instance required. **/
    struct gsGensatannotationModifiedDate *gsGensatannotationModifiedDate;	/** Single instance required. **/
    };

void gsGensatannotationSave(struct gsGensatannotation *obj, int indent, FILE *f);
/* Save gsGensatannotation to file. */

struct gsGensatannotation *gsGensatannotationLoad(char *fileName);
/* Load gsGensatannotation from file. */

struct gsGensatannotationExpressionLevel
    {
    struct gsGensatannotationExpressionLevel *next;
    char *value;	/* Defaults to unknown */
    };

void gsGensatannotationExpressionLevelSave(struct gsGensatannotationExpressionLevel *obj, int indent, FILE *f);
/* Save gsGensatannotationExpressionLevel to file. */

struct gsGensatannotationExpressionLevel *gsGensatannotationExpressionLevelLoad(char *fileName);
/* Load gsGensatannotationExpressionLevel from file. */

struct gsGensatannotationExpressionPattern
    {
    struct gsGensatannotationExpressionPattern *next;
    char *value;	/* Defaults to unknown */
    };

void gsGensatannotationExpressionPatternSave(struct gsGensatannotationExpressionPattern *obj, int indent, FILE *f);
/* Save gsGensatannotationExpressionPattern to file. */

struct gsGensatannotationExpressionPattern *gsGensatannotationExpressionPatternLoad(char *fileName);
/* Load gsGensatannotationExpressionPattern from file. */

struct gsGensatannotationRegion
    {
    struct gsGensatannotationRegion *next;
    char *text;
    };

void gsGensatannotationRegionSave(struct gsGensatannotationRegion *obj, int indent, FILE *f);
/* Save gsGensatannotationRegion to file. */

struct gsGensatannotationRegion *gsGensatannotationRegionLoad(char *fileName);
/* Load gsGensatannotationRegion from file. */

struct gsGensatannotationCellType
    {
    struct gsGensatannotationCellType *next;
    char *text;
    };

void gsGensatannotationCellTypeSave(struct gsGensatannotationCellType *obj, int indent, FILE *f);
/* Save gsGensatannotationCellType to file. */

struct gsGensatannotationCellType *gsGensatannotationCellTypeLoad(char *fileName);
/* Load gsGensatannotationCellType from file. */

struct gsGensatannotationCellSubtype
    {
    struct gsGensatannotationCellSubtype *next;
    char *text;
    };

void gsGensatannotationCellSubtypeSave(struct gsGensatannotationCellSubtype *obj, int indent, FILE *f);
/* Save gsGensatannotationCellSubtype to file. */

struct gsGensatannotationCellSubtype *gsGensatannotationCellSubtypeLoad(char *fileName);
/* Load gsGensatannotationCellSubtype from file. */

struct gsGensatannotationCreateDate
    {
    struct gsGensatannotationCreateDate *next;
    struct gsDate *gsDate;	/** Single instance required. **/
    };

void gsGensatannotationCreateDateSave(struct gsGensatannotationCreateDate *obj, int indent, FILE *f);
/* Save gsGensatannotationCreateDate to file. */

struct gsGensatannotationCreateDate *gsGensatannotationCreateDateLoad(char *fileName);
/* Load gsGensatannotationCreateDate from file. */

struct gsGensatannotationModifiedDate
    {
    struct gsGensatannotationModifiedDate *next;
    struct gsDate *gsDate;	/** Single instance required. **/
    };

void gsGensatannotationModifiedDateSave(struct gsGensatannotationModifiedDate *obj, int indent, FILE *f);
/* Save gsGensatannotationModifiedDate to file. */

struct gsGensatannotationModifiedDate *gsGensatannotationModifiedDateLoad(char *fileName);
/* Load gsGensatannotationModifiedDate from file. */

struct gsGensatimageinfo
    {
    struct gsGensatimageinfo *next;
    struct gsGensatimageinfoFilename *gsGensatimageinfoFilename;	/** Single instance required. **/
    struct gsGensatimageinfoWidth *gsGensatimageinfoWidth;	/** Single instance required. **/
    struct gsGensatimageinfoHeight *gsGensatimageinfoHeight;	/** Single instance required. **/
    };

void gsGensatimageinfoSave(struct gsGensatimageinfo *obj, int indent, FILE *f);
/* Save gsGensatimageinfo to file. */

struct gsGensatimageinfo *gsGensatimageinfoLoad(char *fileName);
/* Load gsGensatimageinfo from file. */

struct gsGensatimageinfoFilename
    {
    struct gsGensatimageinfoFilename *next;
    char *text;
    };

void gsGensatimageinfoFilenameSave(struct gsGensatimageinfoFilename *obj, int indent, FILE *f);
/* Save gsGensatimageinfoFilename to file. */

struct gsGensatimageinfoFilename *gsGensatimageinfoFilenameLoad(char *fileName);
/* Load gsGensatimageinfoFilename from file. */

struct gsGensatimageinfoWidth
    {
    struct gsGensatimageinfoWidth *next;
    int text;
    };

void gsGensatimageinfoWidthSave(struct gsGensatimageinfoWidth *obj, int indent, FILE *f);
/* Save gsGensatimageinfoWidth to file. */

struct gsGensatimageinfoWidth *gsGensatimageinfoWidthLoad(char *fileName);
/* Load gsGensatimageinfoWidth from file. */

struct gsGensatimageinfoHeight
    {
    struct gsGensatimageinfoHeight *next;
    int text;
    };

void gsGensatimageinfoHeightSave(struct gsGensatimageinfoHeight *obj, int indent, FILE *f);
/* Save gsGensatimageinfoHeight to file. */

struct gsGensatimageinfoHeight *gsGensatimageinfoHeightLoad(char *fileName);
/* Load gsGensatimageinfoHeight from file. */

struct gsGensatimage
    {
    struct gsGensatimage *next;
    struct gsGensatimageId *gsGensatimageId;	/** Single instance required. **/
    struct gsGensatimageImageInfo *gsGensatimageImageInfo;	/** Single instance required. **/
    struct gsGensatimageImageTechnique *gsGensatimageImageTechnique;	/** Single instance required. **/
    struct gsGensatimageAge *gsGensatimageAge;	/** Single instance required. **/
    struct gsGensatimageGeneSymbol *gsGensatimageGeneSymbol;	/** Single instance required. **/
    struct gsGensatimageGeneName *gsGensatimageGeneName;	/** Single instance required. **/
    struct gsGensatimageGeneId *gsGensatimageGeneId;	/** Optional (may be NULL). **/
    struct gsGensatimageGenbankAcc *gsGensatimageGenbankAcc;	/** Optional (may be NULL). **/
    struct gsGensatimageSectionPlane *gsGensatimageSectionPlane;	/** Single instance required. **/
    struct gsGensatimageSectionLevel *gsGensatimageSectionLevel;	/** Single instance required. **/
    struct gsGensatimageSacrificeDate *gsGensatimageSacrificeDate;	/** Single instance required. **/
    struct gsGensatimageSectionDate *gsGensatimageSectionDate;	/** Single instance required. **/
    struct gsGensatimageAnnotations *gsGensatimageAnnotations;	/** Optional (may be NULL). **/
    };

void gsGensatimageSave(struct gsGensatimage *obj, int indent, FILE *f);
/* Save gsGensatimage to file. */

struct gsGensatimage *gsGensatimageLoad(char *fileName);
/* Load gsGensatimage from file. */

struct gsGensatimageId
    {
    struct gsGensatimageId *next;
    int text;
    };

void gsGensatimageIdSave(struct gsGensatimageId *obj, int indent, FILE *f);
/* Save gsGensatimageId to file. */

struct gsGensatimageId *gsGensatimageIdLoad(char *fileName);
/* Load gsGensatimageId from file. */

struct gsGensatimageImageInfo
    {
    struct gsGensatimageImageInfo *next;
    struct gsGensatimageImageInfoFullImg *gsGensatimageImageInfoFullImg;	/** Single instance required. **/
    struct gsGensatimageImageInfoMedImg *gsGensatimageImageInfoMedImg;	/** Single instance required. **/
    struct gsGensatimageImageInfoIconImg *gsGensatimageImageInfoIconImg;	/** Single instance required. **/
    };

void gsGensatimageImageInfoSave(struct gsGensatimageImageInfo *obj, int indent, FILE *f);
/* Save gsGensatimageImageInfo to file. */

struct gsGensatimageImageInfo *gsGensatimageImageInfoLoad(char *fileName);
/* Load gsGensatimageImageInfo from file. */

struct gsGensatimageImageInfoFullImg
    {
    struct gsGensatimageImageInfoFullImg *next;
    struct gsGensatimageinfo *gsGensatimageinfo;	/** Single instance required. **/
    };

void gsGensatimageImageInfoFullImgSave(struct gsGensatimageImageInfoFullImg *obj, int indent, FILE *f);
/* Save gsGensatimageImageInfoFullImg to file. */

struct gsGensatimageImageInfoFullImg *gsGensatimageImageInfoFullImgLoad(char *fileName);
/* Load gsGensatimageImageInfoFullImg from file. */

struct gsGensatimageImageInfoMedImg
    {
    struct gsGensatimageImageInfoMedImg *next;
    struct gsGensatimageinfo *gsGensatimageinfo;	/** Single instance required. **/
    };

void gsGensatimageImageInfoMedImgSave(struct gsGensatimageImageInfoMedImg *obj, int indent, FILE *f);
/* Save gsGensatimageImageInfoMedImg to file. */

struct gsGensatimageImageInfoMedImg *gsGensatimageImageInfoMedImgLoad(char *fileName);
/* Load gsGensatimageImageInfoMedImg from file. */

struct gsGensatimageImageInfoIconImg
    {
    struct gsGensatimageImageInfoIconImg *next;
    struct gsGensatimageinfo *gsGensatimageinfo;	/** Single instance required. **/
    };

void gsGensatimageImageInfoIconImgSave(struct gsGensatimageImageInfoIconImg *obj, int indent, FILE *f);
/* Save gsGensatimageImageInfoIconImg to file. */

struct gsGensatimageImageInfoIconImg *gsGensatimageImageInfoIconImgLoad(char *fileName);
/* Load gsGensatimageImageInfoIconImg from file. */

struct gsGensatimageImageTechnique
    {
    struct gsGensatimageImageTechnique *next;
    char *value;	/* Defaults to bac-brightfield */
    };

void gsGensatimageImageTechniqueSave(struct gsGensatimageImageTechnique *obj, int indent, FILE *f);
/* Save gsGensatimageImageTechnique to file. */

struct gsGensatimageImageTechnique *gsGensatimageImageTechniqueLoad(char *fileName);
/* Load gsGensatimageImageTechnique from file. */

struct gsGensatimageAge
    {
    struct gsGensatimageAge *next;
    char *value;	/* Defaults to unknown */
    };

void gsGensatimageAgeSave(struct gsGensatimageAge *obj, int indent, FILE *f);
/* Save gsGensatimageAge to file. */

struct gsGensatimageAge *gsGensatimageAgeLoad(char *fileName);
/* Load gsGensatimageAge from file. */

struct gsGensatimageGeneSymbol
    {
    struct gsGensatimageGeneSymbol *next;
    char *text;
    };

void gsGensatimageGeneSymbolSave(struct gsGensatimageGeneSymbol *obj, int indent, FILE *f);
/* Save gsGensatimageGeneSymbol to file. */

struct gsGensatimageGeneSymbol *gsGensatimageGeneSymbolLoad(char *fileName);
/* Load gsGensatimageGeneSymbol from file. */

struct gsGensatimageGeneName
    {
    struct gsGensatimageGeneName *next;
    char *text;
    };

void gsGensatimageGeneNameSave(struct gsGensatimageGeneName *obj, int indent, FILE *f);
/* Save gsGensatimageGeneName to file. */

struct gsGensatimageGeneName *gsGensatimageGeneNameLoad(char *fileName);
/* Load gsGensatimageGeneName from file. */

struct gsGensatimageGeneId
    {
    struct gsGensatimageGeneId *next;
    int text;
    };

void gsGensatimageGeneIdSave(struct gsGensatimageGeneId *obj, int indent, FILE *f);
/* Save gsGensatimageGeneId to file. */

struct gsGensatimageGeneId *gsGensatimageGeneIdLoad(char *fileName);
/* Load gsGensatimageGeneId from file. */

struct gsGensatimageGenbankAcc
    {
    struct gsGensatimageGenbankAcc *next;
    char *text;
    };

void gsGensatimageGenbankAccSave(struct gsGensatimageGenbankAcc *obj, int indent, FILE *f);
/* Save gsGensatimageGenbankAcc to file. */

struct gsGensatimageGenbankAcc *gsGensatimageGenbankAccLoad(char *fileName);
/* Load gsGensatimageGenbankAcc from file. */

struct gsGensatimageSectionPlane
    {
    struct gsGensatimageSectionPlane *next;
    char *value;	/* Defaults to unknown */
    };

void gsGensatimageSectionPlaneSave(struct gsGensatimageSectionPlane *obj, int indent, FILE *f);
/* Save gsGensatimageSectionPlane to file. */

struct gsGensatimageSectionPlane *gsGensatimageSectionPlaneLoad(char *fileName);
/* Load gsGensatimageSectionPlane from file. */

struct gsGensatimageSectionLevel
    {
    struct gsGensatimageSectionLevel *next;
    char *text;
    };

void gsGensatimageSectionLevelSave(struct gsGensatimageSectionLevel *obj, int indent, FILE *f);
/* Save gsGensatimageSectionLevel to file. */

struct gsGensatimageSectionLevel *gsGensatimageSectionLevelLoad(char *fileName);
/* Load gsGensatimageSectionLevel from file. */

struct gsGensatimageSacrificeDate
    {
    struct gsGensatimageSacrificeDate *next;
    struct gsDate *gsDate;	/** Single instance required. **/
    };

void gsGensatimageSacrificeDateSave(struct gsGensatimageSacrificeDate *obj, int indent, FILE *f);
/* Save gsGensatimageSacrificeDate to file. */

struct gsGensatimageSacrificeDate *gsGensatimageSacrificeDateLoad(char *fileName);
/* Load gsGensatimageSacrificeDate from file. */

struct gsGensatimageSectionDate
    {
    struct gsGensatimageSectionDate *next;
    struct gsDate *gsDate;	/** Single instance required. **/
    };

void gsGensatimageSectionDateSave(struct gsGensatimageSectionDate *obj, int indent, FILE *f);
/* Save gsGensatimageSectionDate to file. */

struct gsGensatimageSectionDate *gsGensatimageSectionDateLoad(char *fileName);
/* Load gsGensatimageSectionDate from file. */

struct gsGensatimageAnnotations
    {
    struct gsGensatimageAnnotations *next;
    struct gsGensatannotation *gsGensatannotation;	/** Possibly empty list. **/
    };

void gsGensatimageAnnotationsSave(struct gsGensatimageAnnotations *obj, int indent, FILE *f);
/* Save gsGensatimageAnnotations to file. */

struct gsGensatimageAnnotations *gsGensatimageAnnotationsLoad(char *fileName);
/* Load gsGensatimageAnnotations from file. */

struct gsGensatimageset
    {
    struct gsGensatimageset *next;
    struct gsGensatimage *gsGensatimage;	/** Possibly empty list. **/
    };

void gsGensatimagesetSave(struct gsGensatimageset *obj, int indent, FILE *f);
/* Save gsGensatimageset to file. */

struct gsGensatimageset *gsGensatimagesetLoad(char *fileName);
/* Load gsGensatimageset from file. */

#endif /* GS_H */


/* gs.h autoXml generated file */
#ifndef GS_H
#define GS_H

struct gsGensatImageSet
    {
    struct gsGensatImageSet *next;
    struct gsGensatImage *gsGensatImage;	/** Possibly empty list. **/
    };

void gsGensatImageSetFree(struct gsGensatImageSet **pObj);
/* Free up gsGensatImageSet. */

void gsGensatImageSetFreeList(struct gsGensatImageSet **pList);
/* Free up list of gsGensatImageSet. */

void gsGensatImageSetSave(struct gsGensatImageSet *obj, int indent, FILE *f);
/* Save gsGensatImageSet to file. */

struct gsGensatImageSet *gsGensatImageSetLoad(char *fileName);
/* Load gsGensatImageSet from file. */

struct gsDateStd
    {
    struct gsDateStd *next;
    char *text;
    };

void gsDateStdFree(struct gsDateStd **pObj);
/* Free up gsDateStd. */

void gsDateStdFreeList(struct gsDateStd **pList);
/* Free up list of gsDateStd. */

void gsDateStdSave(struct gsDateStd *obj, int indent, FILE *f);
/* Save gsDateStd to file. */

struct gsDateStd *gsDateStdLoad(char *fileName);
/* Load gsDateStd from file. */

struct gsDate
    {
    struct gsDate *next;
    struct gsDateStd *gsDateStd;	/** Single instance required. **/
    };

void gsDateFree(struct gsDate **pObj);
/* Free up gsDate. */

void gsDateFreeList(struct gsDate **pList);
/* Free up list of gsDate. */

void gsDateSave(struct gsDate *obj, int indent, FILE *f);
/* Save gsDate to file. */

struct gsDate *gsDateLoad(char *fileName);
/* Load gsDate from file. */

struct gsGensatAnnotation
    {
    struct gsGensatAnnotation *next;
    struct gsGensatAnnotationExpressionLevel *gsGensatAnnotationExpressionLevel;	/** Single instance required. **/
    struct gsGensatAnnotationExpressionPattern *gsGensatAnnotationExpressionPattern;	/** Single instance required. **/
    struct gsGensatAnnotationRegion *gsGensatAnnotationRegion;	/** Optional (may be NULL). **/
    struct gsGensatAnnotationCellType *gsGensatAnnotationCellType;	/** Optional (may be NULL). **/
    struct gsGensatAnnotationCellSubtype *gsGensatAnnotationCellSubtype;	/** Optional (may be NULL). **/
    struct gsGensatAnnotationCreateDate *gsGensatAnnotationCreateDate;	/** Single instance required. **/
    struct gsGensatAnnotationModifiedDate *gsGensatAnnotationModifiedDate;	/** Single instance required. **/
    };

void gsGensatAnnotationFree(struct gsGensatAnnotation **pObj);
/* Free up gsGensatAnnotation. */

void gsGensatAnnotationFreeList(struct gsGensatAnnotation **pList);
/* Free up list of gsGensatAnnotation. */

void gsGensatAnnotationSave(struct gsGensatAnnotation *obj, int indent, FILE *f);
/* Save gsGensatAnnotation to file. */

struct gsGensatAnnotation *gsGensatAnnotationLoad(char *fileName);
/* Load gsGensatAnnotation from file. */

struct gsGensatAnnotationExpressionLevel
    {
    struct gsGensatAnnotationExpressionLevel *next;
    char *value;	/* Defaults to unknown */
    };

void gsGensatAnnotationExpressionLevelFree(struct gsGensatAnnotationExpressionLevel **pObj);
/* Free up gsGensatAnnotationExpressionLevel. */

void gsGensatAnnotationExpressionLevelFreeList(struct gsGensatAnnotationExpressionLevel **pList);
/* Free up list of gsGensatAnnotationExpressionLevel. */

void gsGensatAnnotationExpressionLevelSave(struct gsGensatAnnotationExpressionLevel *obj, int indent, FILE *f);
/* Save gsGensatAnnotationExpressionLevel to file. */

struct gsGensatAnnotationExpressionLevel *gsGensatAnnotationExpressionLevelLoad(char *fileName);
/* Load gsGensatAnnotationExpressionLevel from file. */

struct gsGensatAnnotationExpressionPattern
    {
    struct gsGensatAnnotationExpressionPattern *next;
    char *value;	/* Defaults to unknown */
    };

void gsGensatAnnotationExpressionPatternFree(struct gsGensatAnnotationExpressionPattern **pObj);
/* Free up gsGensatAnnotationExpressionPattern. */

void gsGensatAnnotationExpressionPatternFreeList(struct gsGensatAnnotationExpressionPattern **pList);
/* Free up list of gsGensatAnnotationExpressionPattern. */

void gsGensatAnnotationExpressionPatternSave(struct gsGensatAnnotationExpressionPattern *obj, int indent, FILE *f);
/* Save gsGensatAnnotationExpressionPattern to file. */

struct gsGensatAnnotationExpressionPattern *gsGensatAnnotationExpressionPatternLoad(char *fileName);
/* Load gsGensatAnnotationExpressionPattern from file. */

struct gsGensatAnnotationRegion
    {
    struct gsGensatAnnotationRegion *next;
    char *text;
    };

void gsGensatAnnotationRegionFree(struct gsGensatAnnotationRegion **pObj);
/* Free up gsGensatAnnotationRegion. */

void gsGensatAnnotationRegionFreeList(struct gsGensatAnnotationRegion **pList);
/* Free up list of gsGensatAnnotationRegion. */

void gsGensatAnnotationRegionSave(struct gsGensatAnnotationRegion *obj, int indent, FILE *f);
/* Save gsGensatAnnotationRegion to file. */

struct gsGensatAnnotationRegion *gsGensatAnnotationRegionLoad(char *fileName);
/* Load gsGensatAnnotationRegion from file. */

struct gsGensatAnnotationCellType
    {
    struct gsGensatAnnotationCellType *next;
    char *text;
    };

void gsGensatAnnotationCellTypeFree(struct gsGensatAnnotationCellType **pObj);
/* Free up gsGensatAnnotationCellType. */

void gsGensatAnnotationCellTypeFreeList(struct gsGensatAnnotationCellType **pList);
/* Free up list of gsGensatAnnotationCellType. */

void gsGensatAnnotationCellTypeSave(struct gsGensatAnnotationCellType *obj, int indent, FILE *f);
/* Save gsGensatAnnotationCellType to file. */

struct gsGensatAnnotationCellType *gsGensatAnnotationCellTypeLoad(char *fileName);
/* Load gsGensatAnnotationCellType from file. */

struct gsGensatAnnotationCellSubtype
    {
    struct gsGensatAnnotationCellSubtype *next;
    char *text;
    };

void gsGensatAnnotationCellSubtypeFree(struct gsGensatAnnotationCellSubtype **pObj);
/* Free up gsGensatAnnotationCellSubtype. */

void gsGensatAnnotationCellSubtypeFreeList(struct gsGensatAnnotationCellSubtype **pList);
/* Free up list of gsGensatAnnotationCellSubtype. */

void gsGensatAnnotationCellSubtypeSave(struct gsGensatAnnotationCellSubtype *obj, int indent, FILE *f);
/* Save gsGensatAnnotationCellSubtype to file. */

struct gsGensatAnnotationCellSubtype *gsGensatAnnotationCellSubtypeLoad(char *fileName);
/* Load gsGensatAnnotationCellSubtype from file. */

struct gsGensatAnnotationCreateDate
    {
    struct gsGensatAnnotationCreateDate *next;
    struct gsDate *gsDate;	/** Single instance required. **/
    };

void gsGensatAnnotationCreateDateFree(struct gsGensatAnnotationCreateDate **pObj);
/* Free up gsGensatAnnotationCreateDate. */

void gsGensatAnnotationCreateDateFreeList(struct gsGensatAnnotationCreateDate **pList);
/* Free up list of gsGensatAnnotationCreateDate. */

void gsGensatAnnotationCreateDateSave(struct gsGensatAnnotationCreateDate *obj, int indent, FILE *f);
/* Save gsGensatAnnotationCreateDate to file. */

struct gsGensatAnnotationCreateDate *gsGensatAnnotationCreateDateLoad(char *fileName);
/* Load gsGensatAnnotationCreateDate from file. */

struct gsGensatAnnotationModifiedDate
    {
    struct gsGensatAnnotationModifiedDate *next;
    struct gsDate *gsDate;	/** Single instance required. **/
    };

void gsGensatAnnotationModifiedDateFree(struct gsGensatAnnotationModifiedDate **pObj);
/* Free up gsGensatAnnotationModifiedDate. */

void gsGensatAnnotationModifiedDateFreeList(struct gsGensatAnnotationModifiedDate **pList);
/* Free up list of gsGensatAnnotationModifiedDate. */

void gsGensatAnnotationModifiedDateSave(struct gsGensatAnnotationModifiedDate *obj, int indent, FILE *f);
/* Save gsGensatAnnotationModifiedDate to file. */

struct gsGensatAnnotationModifiedDate *gsGensatAnnotationModifiedDateLoad(char *fileName);
/* Load gsGensatAnnotationModifiedDate from file. */

struct gsGensatImageInfo
    {
    struct gsGensatImageInfo *next;
    struct gsGensatImageInfoFilename *gsGensatImageInfoFilename;	/** Single instance required. **/
    struct gsGensatImageInfoWidth *gsGensatImageInfoWidth;	/** Single instance required. **/
    struct gsGensatImageInfoHeight *gsGensatImageInfoHeight;	/** Single instance required. **/
    };

void gsGensatImageInfoFree(struct gsGensatImageInfo **pObj);
/* Free up gsGensatImageInfo. */

void gsGensatImageInfoFreeList(struct gsGensatImageInfo **pList);
/* Free up list of gsGensatImageInfo. */

void gsGensatImageInfoSave(struct gsGensatImageInfo *obj, int indent, FILE *f);
/* Save gsGensatImageInfo to file. */

struct gsGensatImageInfo *gsGensatImageInfoLoad(char *fileName);
/* Load gsGensatImageInfo from file. */

struct gsGensatImageInfoFilename
    {
    struct gsGensatImageInfoFilename *next;
    char *text;
    };

void gsGensatImageInfoFilenameFree(struct gsGensatImageInfoFilename **pObj);
/* Free up gsGensatImageInfoFilename. */

void gsGensatImageInfoFilenameFreeList(struct gsGensatImageInfoFilename **pList);
/* Free up list of gsGensatImageInfoFilename. */

void gsGensatImageInfoFilenameSave(struct gsGensatImageInfoFilename *obj, int indent, FILE *f);
/* Save gsGensatImageInfoFilename to file. */

struct gsGensatImageInfoFilename *gsGensatImageInfoFilenameLoad(char *fileName);
/* Load gsGensatImageInfoFilename from file. */

struct gsGensatImageInfoWidth
    {
    struct gsGensatImageInfoWidth *next;
    int text;
    };

void gsGensatImageInfoWidthFree(struct gsGensatImageInfoWidth **pObj);
/* Free up gsGensatImageInfoWidth. */

void gsGensatImageInfoWidthFreeList(struct gsGensatImageInfoWidth **pList);
/* Free up list of gsGensatImageInfoWidth. */

void gsGensatImageInfoWidthSave(struct gsGensatImageInfoWidth *obj, int indent, FILE *f);
/* Save gsGensatImageInfoWidth to file. */

struct gsGensatImageInfoWidth *gsGensatImageInfoWidthLoad(char *fileName);
/* Load gsGensatImageInfoWidth from file. */

struct gsGensatImageInfoHeight
    {
    struct gsGensatImageInfoHeight *next;
    int text;
    };

void gsGensatImageInfoHeightFree(struct gsGensatImageInfoHeight **pObj);
/* Free up gsGensatImageInfoHeight. */

void gsGensatImageInfoHeightFreeList(struct gsGensatImageInfoHeight **pList);
/* Free up list of gsGensatImageInfoHeight. */

void gsGensatImageInfoHeightSave(struct gsGensatImageInfoHeight *obj, int indent, FILE *f);
/* Save gsGensatImageInfoHeight to file. */

struct gsGensatImageInfoHeight *gsGensatImageInfoHeightLoad(char *fileName);
/* Load gsGensatImageInfoHeight from file. */

struct gsGensatGeneInfo
    {
    struct gsGensatGeneInfo *next;
    struct gsGensatGeneInfoGeneId *gsGensatGeneInfoGeneId;	/** Optional (may be NULL). **/
    struct gsGensatGeneInfoGeneSymbol *gsGensatGeneInfoGeneSymbol;	/** Optional (may be NULL). **/
    struct gsGensatGeneInfoGeneName *gsGensatGeneInfoGeneName;	/** Optional (may be NULL). **/
    struct gsGensatGeneInfoGeneAliases *gsGensatGeneInfoGeneAliases;	/** Optional (may be NULL). **/
    struct gsGensatGeneInfoBacName *gsGensatGeneInfoBacName;	/** Optional (may be NULL). **/
    struct gsGensatGeneInfoBacAddress *gsGensatGeneInfoBacAddress;	/** Optional (may be NULL). **/
    struct gsGensatGeneInfoBacMarker *gsGensatGeneInfoBacMarker;	/** Optional (may be NULL). **/
    struct gsGensatGeneInfoBacComment *gsGensatGeneInfoBacComment;	/** Optional (may be NULL). **/
    };

void gsGensatGeneInfoFree(struct gsGensatGeneInfo **pObj);
/* Free up gsGensatGeneInfo. */

void gsGensatGeneInfoFreeList(struct gsGensatGeneInfo **pList);
/* Free up list of gsGensatGeneInfo. */

void gsGensatGeneInfoSave(struct gsGensatGeneInfo *obj, int indent, FILE *f);
/* Save gsGensatGeneInfo to file. */

struct gsGensatGeneInfo *gsGensatGeneInfoLoad(char *fileName);
/* Load gsGensatGeneInfo from file. */

struct gsGensatGeneInfoGeneId
    {
    struct gsGensatGeneInfoGeneId *next;
    int text;
    };

void gsGensatGeneInfoGeneIdFree(struct gsGensatGeneInfoGeneId **pObj);
/* Free up gsGensatGeneInfoGeneId. */

void gsGensatGeneInfoGeneIdFreeList(struct gsGensatGeneInfoGeneId **pList);
/* Free up list of gsGensatGeneInfoGeneId. */

void gsGensatGeneInfoGeneIdSave(struct gsGensatGeneInfoGeneId *obj, int indent, FILE *f);
/* Save gsGensatGeneInfoGeneId to file. */

struct gsGensatGeneInfoGeneId *gsGensatGeneInfoGeneIdLoad(char *fileName);
/* Load gsGensatGeneInfoGeneId from file. */

struct gsGensatGeneInfoGeneSymbol
    {
    struct gsGensatGeneInfoGeneSymbol *next;
    char *text;
    };

void gsGensatGeneInfoGeneSymbolFree(struct gsGensatGeneInfoGeneSymbol **pObj);
/* Free up gsGensatGeneInfoGeneSymbol. */

void gsGensatGeneInfoGeneSymbolFreeList(struct gsGensatGeneInfoGeneSymbol **pList);
/* Free up list of gsGensatGeneInfoGeneSymbol. */

void gsGensatGeneInfoGeneSymbolSave(struct gsGensatGeneInfoGeneSymbol *obj, int indent, FILE *f);
/* Save gsGensatGeneInfoGeneSymbol to file. */

struct gsGensatGeneInfoGeneSymbol *gsGensatGeneInfoGeneSymbolLoad(char *fileName);
/* Load gsGensatGeneInfoGeneSymbol from file. */

struct gsGensatGeneInfoGeneName
    {
    struct gsGensatGeneInfoGeneName *next;
    char *text;
    };

void gsGensatGeneInfoGeneNameFree(struct gsGensatGeneInfoGeneName **pObj);
/* Free up gsGensatGeneInfoGeneName. */

void gsGensatGeneInfoGeneNameFreeList(struct gsGensatGeneInfoGeneName **pList);
/* Free up list of gsGensatGeneInfoGeneName. */

void gsGensatGeneInfoGeneNameSave(struct gsGensatGeneInfoGeneName *obj, int indent, FILE *f);
/* Save gsGensatGeneInfoGeneName to file. */

struct gsGensatGeneInfoGeneName *gsGensatGeneInfoGeneNameLoad(char *fileName);
/* Load gsGensatGeneInfoGeneName from file. */

struct gsGensatGeneInfoGeneAliases
    {
    struct gsGensatGeneInfoGeneAliases *next;
    struct gsGensatGeneInfoGeneAliasesE *gsGensatGeneInfoGeneAliasesE;	/** Possibly empty list. **/
    };

void gsGensatGeneInfoGeneAliasesFree(struct gsGensatGeneInfoGeneAliases **pObj);
/* Free up gsGensatGeneInfoGeneAliases. */

void gsGensatGeneInfoGeneAliasesFreeList(struct gsGensatGeneInfoGeneAliases **pList);
/* Free up list of gsGensatGeneInfoGeneAliases. */

void gsGensatGeneInfoGeneAliasesSave(struct gsGensatGeneInfoGeneAliases *obj, int indent, FILE *f);
/* Save gsGensatGeneInfoGeneAliases to file. */

struct gsGensatGeneInfoGeneAliases *gsGensatGeneInfoGeneAliasesLoad(char *fileName);
/* Load gsGensatGeneInfoGeneAliases from file. */

struct gsGensatGeneInfoGeneAliasesE
    {
    struct gsGensatGeneInfoGeneAliasesE *next;
    char *text;
    };

void gsGensatGeneInfoGeneAliasesEFree(struct gsGensatGeneInfoGeneAliasesE **pObj);
/* Free up gsGensatGeneInfoGeneAliasesE. */

void gsGensatGeneInfoGeneAliasesEFreeList(struct gsGensatGeneInfoGeneAliasesE **pList);
/* Free up list of gsGensatGeneInfoGeneAliasesE. */

void gsGensatGeneInfoGeneAliasesESave(struct gsGensatGeneInfoGeneAliasesE *obj, int indent, FILE *f);
/* Save gsGensatGeneInfoGeneAliasesE to file. */

struct gsGensatGeneInfoGeneAliasesE *gsGensatGeneInfoGeneAliasesELoad(char *fileName);
/* Load gsGensatGeneInfoGeneAliasesE from file. */

struct gsGensatGeneInfoBacName
    {
    struct gsGensatGeneInfoBacName *next;
    char *text;
    };

void gsGensatGeneInfoBacNameFree(struct gsGensatGeneInfoBacName **pObj);
/* Free up gsGensatGeneInfoBacName. */

void gsGensatGeneInfoBacNameFreeList(struct gsGensatGeneInfoBacName **pList);
/* Free up list of gsGensatGeneInfoBacName. */

void gsGensatGeneInfoBacNameSave(struct gsGensatGeneInfoBacName *obj, int indent, FILE *f);
/* Save gsGensatGeneInfoBacName to file. */

struct gsGensatGeneInfoBacName *gsGensatGeneInfoBacNameLoad(char *fileName);
/* Load gsGensatGeneInfoBacName from file. */

struct gsGensatGeneInfoBacAddress
    {
    struct gsGensatGeneInfoBacAddress *next;
    char *text;
    };

void gsGensatGeneInfoBacAddressFree(struct gsGensatGeneInfoBacAddress **pObj);
/* Free up gsGensatGeneInfoBacAddress. */

void gsGensatGeneInfoBacAddressFreeList(struct gsGensatGeneInfoBacAddress **pList);
/* Free up list of gsGensatGeneInfoBacAddress. */

void gsGensatGeneInfoBacAddressSave(struct gsGensatGeneInfoBacAddress *obj, int indent, FILE *f);
/* Save gsGensatGeneInfoBacAddress to file. */

struct gsGensatGeneInfoBacAddress *gsGensatGeneInfoBacAddressLoad(char *fileName);
/* Load gsGensatGeneInfoBacAddress from file. */

struct gsGensatGeneInfoBacMarker
    {
    struct gsGensatGeneInfoBacMarker *next;
    char *text;
    };

void gsGensatGeneInfoBacMarkerFree(struct gsGensatGeneInfoBacMarker **pObj);
/* Free up gsGensatGeneInfoBacMarker. */

void gsGensatGeneInfoBacMarkerFreeList(struct gsGensatGeneInfoBacMarker **pList);
/* Free up list of gsGensatGeneInfoBacMarker. */

void gsGensatGeneInfoBacMarkerSave(struct gsGensatGeneInfoBacMarker *obj, int indent, FILE *f);
/* Save gsGensatGeneInfoBacMarker to file. */

struct gsGensatGeneInfoBacMarker *gsGensatGeneInfoBacMarkerLoad(char *fileName);
/* Load gsGensatGeneInfoBacMarker from file. */

struct gsGensatGeneInfoBacComment
    {
    struct gsGensatGeneInfoBacComment *next;
    char *text;
    };

void gsGensatGeneInfoBacCommentFree(struct gsGensatGeneInfoBacComment **pObj);
/* Free up gsGensatGeneInfoBacComment. */

void gsGensatGeneInfoBacCommentFreeList(struct gsGensatGeneInfoBacComment **pList);
/* Free up list of gsGensatGeneInfoBacComment. */

void gsGensatGeneInfoBacCommentSave(struct gsGensatGeneInfoBacComment *obj, int indent, FILE *f);
/* Save gsGensatGeneInfoBacComment to file. */

struct gsGensatGeneInfoBacComment *gsGensatGeneInfoBacCommentLoad(char *fileName);
/* Load gsGensatGeneInfoBacComment from file. */

struct gsGensatSequenceInfo
    {
    struct gsGensatSequenceInfo *next;
    struct gsGensatSequenceInfoGi *gsGensatSequenceInfoGi;	/** Optional (may be NULL). **/
    struct gsGensatSequenceInfoAccession *gsGensatSequenceInfoAccession;	/** Optional (may be NULL). **/
    struct gsGensatSequenceInfoTaxId *gsGensatSequenceInfoTaxId;	/** Optional (may be NULL). **/
    };

void gsGensatSequenceInfoFree(struct gsGensatSequenceInfo **pObj);
/* Free up gsGensatSequenceInfo. */

void gsGensatSequenceInfoFreeList(struct gsGensatSequenceInfo **pList);
/* Free up list of gsGensatSequenceInfo. */

void gsGensatSequenceInfoSave(struct gsGensatSequenceInfo *obj, int indent, FILE *f);
/* Save gsGensatSequenceInfo to file. */

struct gsGensatSequenceInfo *gsGensatSequenceInfoLoad(char *fileName);
/* Load gsGensatSequenceInfo from file. */

struct gsGensatSequenceInfoGi
    {
    struct gsGensatSequenceInfoGi *next;
    int text;
    };

void gsGensatSequenceInfoGiFree(struct gsGensatSequenceInfoGi **pObj);
/* Free up gsGensatSequenceInfoGi. */

void gsGensatSequenceInfoGiFreeList(struct gsGensatSequenceInfoGi **pList);
/* Free up list of gsGensatSequenceInfoGi. */

void gsGensatSequenceInfoGiSave(struct gsGensatSequenceInfoGi *obj, int indent, FILE *f);
/* Save gsGensatSequenceInfoGi to file. */

struct gsGensatSequenceInfoGi *gsGensatSequenceInfoGiLoad(char *fileName);
/* Load gsGensatSequenceInfoGi from file. */

struct gsGensatSequenceInfoAccession
    {
    struct gsGensatSequenceInfoAccession *next;
    char *text;
    };

void gsGensatSequenceInfoAccessionFree(struct gsGensatSequenceInfoAccession **pObj);
/* Free up gsGensatSequenceInfoAccession. */

void gsGensatSequenceInfoAccessionFreeList(struct gsGensatSequenceInfoAccession **pList);
/* Free up list of gsGensatSequenceInfoAccession. */

void gsGensatSequenceInfoAccessionSave(struct gsGensatSequenceInfoAccession *obj, int indent, FILE *f);
/* Save gsGensatSequenceInfoAccession to file. */

struct gsGensatSequenceInfoAccession *gsGensatSequenceInfoAccessionLoad(char *fileName);
/* Load gsGensatSequenceInfoAccession from file. */

struct gsGensatSequenceInfoTaxId
    {
    struct gsGensatSequenceInfoTaxId *next;
    int text;
    };

void gsGensatSequenceInfoTaxIdFree(struct gsGensatSequenceInfoTaxId **pObj);
/* Free up gsGensatSequenceInfoTaxId. */

void gsGensatSequenceInfoTaxIdFreeList(struct gsGensatSequenceInfoTaxId **pList);
/* Free up list of gsGensatSequenceInfoTaxId. */

void gsGensatSequenceInfoTaxIdSave(struct gsGensatSequenceInfoTaxId *obj, int indent, FILE *f);
/* Save gsGensatSequenceInfoTaxId to file. */

struct gsGensatSequenceInfoTaxId *gsGensatSequenceInfoTaxIdLoad(char *fileName);
/* Load gsGensatSequenceInfoTaxId from file. */

struct gsGensatImage
    {
    struct gsGensatImage *next;
    struct gsGensatImageId *gsGensatImageId;	/** Single instance required. **/
    struct gsGensatImageImageInfo *gsGensatImageImageInfo;	/** Single instance required. **/
    struct gsGensatImageImageTechnique *gsGensatImageImageTechnique;	/** Single instance required. **/
    struct gsGensatImageAge *gsGensatImageAge;	/** Single instance required. **/
    struct gsGensatImageSex *gsGensatImageSex;	/** Single instance required. **/
    struct gsGensatImageGeneInfo *gsGensatImageGeneInfo;	/** Single instance required. **/
    struct gsGensatImageSeqInfo *gsGensatImageSeqInfo;	/** Single instance required. **/
    struct gsGensatImageSectionPlane *gsGensatImageSectionPlane;	/** Single instance required. **/
    struct gsGensatImageSectionLevel *gsGensatImageSectionLevel;	/** Single instance required. **/
    struct gsGensatImageSacrificeDate *gsGensatImageSacrificeDate;	/** Single instance required. **/
    struct gsGensatImageSectionDate *gsGensatImageSectionDate;	/** Single instance required. **/
    struct gsGensatImageAnnotations *gsGensatImageAnnotations;	/** Optional (may be NULL). **/
    };

void gsGensatImageFree(struct gsGensatImage **pObj);
/* Free up gsGensatImage. */

void gsGensatImageFreeList(struct gsGensatImage **pList);
/* Free up list of gsGensatImage. */

void gsGensatImageSave(struct gsGensatImage *obj, int indent, FILE *f);
/* Save gsGensatImage to file. */

struct gsGensatImage *gsGensatImageLoad(char *fileName);
/* Load gsGensatImage from file. */

struct gsGensatImageId
    {
    struct gsGensatImageId *next;
    int text;
    };

void gsGensatImageIdFree(struct gsGensatImageId **pObj);
/* Free up gsGensatImageId. */

void gsGensatImageIdFreeList(struct gsGensatImageId **pList);
/* Free up list of gsGensatImageId. */

void gsGensatImageIdSave(struct gsGensatImageId *obj, int indent, FILE *f);
/* Save gsGensatImageId to file. */

struct gsGensatImageId *gsGensatImageIdLoad(char *fileName);
/* Load gsGensatImageId from file. */

struct gsGensatImageImageInfo
    {
    struct gsGensatImageImageInfo *next;
    struct gsGensatImageImageInfoDirectory *gsGensatImageImageInfoDirectory;	/** Single instance required. **/
    struct gsGensatImageImageInfoSubmittorId *gsGensatImageImageInfoSubmittorId;	/** Single instance required. **/
    struct gsGensatImageImageInfoFullImg *gsGensatImageImageInfoFullImg;	/** Single instance required. **/
    struct gsGensatImageImageInfoMedImg *gsGensatImageImageInfoMedImg;	/** Single instance required. **/
    struct gsGensatImageImageInfoIconImg *gsGensatImageImageInfoIconImg;	/** Single instance required. **/
    };

void gsGensatImageImageInfoFree(struct gsGensatImageImageInfo **pObj);
/* Free up gsGensatImageImageInfo. */

void gsGensatImageImageInfoFreeList(struct gsGensatImageImageInfo **pList);
/* Free up list of gsGensatImageImageInfo. */

void gsGensatImageImageInfoSave(struct gsGensatImageImageInfo *obj, int indent, FILE *f);
/* Save gsGensatImageImageInfo to file. */

struct gsGensatImageImageInfo *gsGensatImageImageInfoLoad(char *fileName);
/* Load gsGensatImageImageInfo from file. */

struct gsGensatImageImageInfoDirectory
    {
    struct gsGensatImageImageInfoDirectory *next;
    char *text;
    };

void gsGensatImageImageInfoDirectoryFree(struct gsGensatImageImageInfoDirectory **pObj);
/* Free up gsGensatImageImageInfoDirectory. */

void gsGensatImageImageInfoDirectoryFreeList(struct gsGensatImageImageInfoDirectory **pList);
/* Free up list of gsGensatImageImageInfoDirectory. */

void gsGensatImageImageInfoDirectorySave(struct gsGensatImageImageInfoDirectory *obj, int indent, FILE *f);
/* Save gsGensatImageImageInfoDirectory to file. */

struct gsGensatImageImageInfoDirectory *gsGensatImageImageInfoDirectoryLoad(char *fileName);
/* Load gsGensatImageImageInfoDirectory from file. */

struct gsGensatImageImageInfoSubmittorId
    {
    struct gsGensatImageImageInfoSubmittorId *next;
    char *text;
    };

void gsGensatImageImageInfoSubmittorIdFree(struct gsGensatImageImageInfoSubmittorId **pObj);
/* Free up gsGensatImageImageInfoSubmittorId. */

void gsGensatImageImageInfoSubmittorIdFreeList(struct gsGensatImageImageInfoSubmittorId **pList);
/* Free up list of gsGensatImageImageInfoSubmittorId. */

void gsGensatImageImageInfoSubmittorIdSave(struct gsGensatImageImageInfoSubmittorId *obj, int indent, FILE *f);
/* Save gsGensatImageImageInfoSubmittorId to file. */

struct gsGensatImageImageInfoSubmittorId *gsGensatImageImageInfoSubmittorIdLoad(char *fileName);
/* Load gsGensatImageImageInfoSubmittorId from file. */

struct gsGensatImageImageInfoFullImg
    {
    struct gsGensatImageImageInfoFullImg *next;
    struct gsGensatImageInfo *gsGensatImageInfo;	/** Single instance required. **/
    };

void gsGensatImageImageInfoFullImgFree(struct gsGensatImageImageInfoFullImg **pObj);
/* Free up gsGensatImageImageInfoFullImg. */

void gsGensatImageImageInfoFullImgFreeList(struct gsGensatImageImageInfoFullImg **pList);
/* Free up list of gsGensatImageImageInfoFullImg. */

void gsGensatImageImageInfoFullImgSave(struct gsGensatImageImageInfoFullImg *obj, int indent, FILE *f);
/* Save gsGensatImageImageInfoFullImg to file. */

struct gsGensatImageImageInfoFullImg *gsGensatImageImageInfoFullImgLoad(char *fileName);
/* Load gsGensatImageImageInfoFullImg from file. */

struct gsGensatImageImageInfoMedImg
    {
    struct gsGensatImageImageInfoMedImg *next;
    struct gsGensatImageInfo *gsGensatImageInfo;	/** Single instance required. **/
    };

void gsGensatImageImageInfoMedImgFree(struct gsGensatImageImageInfoMedImg **pObj);
/* Free up gsGensatImageImageInfoMedImg. */

void gsGensatImageImageInfoMedImgFreeList(struct gsGensatImageImageInfoMedImg **pList);
/* Free up list of gsGensatImageImageInfoMedImg. */

void gsGensatImageImageInfoMedImgSave(struct gsGensatImageImageInfoMedImg *obj, int indent, FILE *f);
/* Save gsGensatImageImageInfoMedImg to file. */

struct gsGensatImageImageInfoMedImg *gsGensatImageImageInfoMedImgLoad(char *fileName);
/* Load gsGensatImageImageInfoMedImg from file. */

struct gsGensatImageImageInfoIconImg
    {
    struct gsGensatImageImageInfoIconImg *next;
    struct gsGensatImageInfo *gsGensatImageInfo;	/** Single instance required. **/
    };

void gsGensatImageImageInfoIconImgFree(struct gsGensatImageImageInfoIconImg **pObj);
/* Free up gsGensatImageImageInfoIconImg. */

void gsGensatImageImageInfoIconImgFreeList(struct gsGensatImageImageInfoIconImg **pList);
/* Free up list of gsGensatImageImageInfoIconImg. */

void gsGensatImageImageInfoIconImgSave(struct gsGensatImageImageInfoIconImg *obj, int indent, FILE *f);
/* Save gsGensatImageImageInfoIconImg to file. */

struct gsGensatImageImageInfoIconImg *gsGensatImageImageInfoIconImgLoad(char *fileName);
/* Load gsGensatImageImageInfoIconImg from file. */

struct gsGensatImageImageTechnique
    {
    struct gsGensatImageImageTechnique *next;
    char *value;	/* Defaults to bac-brightfield */
    };

void gsGensatImageImageTechniqueFree(struct gsGensatImageImageTechnique **pObj);
/* Free up gsGensatImageImageTechnique. */

void gsGensatImageImageTechniqueFreeList(struct gsGensatImageImageTechnique **pList);
/* Free up list of gsGensatImageImageTechnique. */

void gsGensatImageImageTechniqueSave(struct gsGensatImageImageTechnique *obj, int indent, FILE *f);
/* Save gsGensatImageImageTechnique to file. */

struct gsGensatImageImageTechnique *gsGensatImageImageTechniqueLoad(char *fileName);
/* Load gsGensatImageImageTechnique from file. */

struct gsGensatImageAge
    {
    struct gsGensatImageAge *next;
    char *value;	/* Defaults to unknown */
    };

void gsGensatImageAgeFree(struct gsGensatImageAge **pObj);
/* Free up gsGensatImageAge. */

void gsGensatImageAgeFreeList(struct gsGensatImageAge **pList);
/* Free up list of gsGensatImageAge. */

void gsGensatImageAgeSave(struct gsGensatImageAge *obj, int indent, FILE *f);
/* Save gsGensatImageAge to file. */

struct gsGensatImageAge *gsGensatImageAgeLoad(char *fileName);
/* Load gsGensatImageAge from file. */

struct gsGensatImageSex
    {
    struct gsGensatImageSex *next;
    char *value;	/* Defaults to unknown */
    };

void gsGensatImageSexFree(struct gsGensatImageSex **pObj);
/* Free up gsGensatImageSex. */

void gsGensatImageSexFreeList(struct gsGensatImageSex **pList);
/* Free up list of gsGensatImageSex. */

void gsGensatImageSexSave(struct gsGensatImageSex *obj, int indent, FILE *f);
/* Save gsGensatImageSex to file. */

struct gsGensatImageSex *gsGensatImageSexLoad(char *fileName);
/* Load gsGensatImageSex from file. */

struct gsGensatImageGeneInfo
    {
    struct gsGensatImageGeneInfo *next;
    struct gsGensatGeneInfo *gsGensatGeneInfo;	/** Single instance required. **/
    };

void gsGensatImageGeneInfoFree(struct gsGensatImageGeneInfo **pObj);
/* Free up gsGensatImageGeneInfo. */

void gsGensatImageGeneInfoFreeList(struct gsGensatImageGeneInfo **pList);
/* Free up list of gsGensatImageGeneInfo. */

void gsGensatImageGeneInfoSave(struct gsGensatImageGeneInfo *obj, int indent, FILE *f);
/* Save gsGensatImageGeneInfo to file. */

struct gsGensatImageGeneInfo *gsGensatImageGeneInfoLoad(char *fileName);
/* Load gsGensatImageGeneInfo from file. */

struct gsGensatImageSeqInfo
    {
    struct gsGensatImageSeqInfo *next;
    struct gsGensatSequenceInfo *gsGensatSequenceInfo;	/** Single instance required. **/
    };

void gsGensatImageSeqInfoFree(struct gsGensatImageSeqInfo **pObj);
/* Free up gsGensatImageSeqInfo. */

void gsGensatImageSeqInfoFreeList(struct gsGensatImageSeqInfo **pList);
/* Free up list of gsGensatImageSeqInfo. */

void gsGensatImageSeqInfoSave(struct gsGensatImageSeqInfo *obj, int indent, FILE *f);
/* Save gsGensatImageSeqInfo to file. */

struct gsGensatImageSeqInfo *gsGensatImageSeqInfoLoad(char *fileName);
/* Load gsGensatImageSeqInfo from file. */

struct gsGensatImageSectionPlane
    {
    struct gsGensatImageSectionPlane *next;
    char *value;	/* Defaults to unknown */
    };

void gsGensatImageSectionPlaneFree(struct gsGensatImageSectionPlane **pObj);
/* Free up gsGensatImageSectionPlane. */

void gsGensatImageSectionPlaneFreeList(struct gsGensatImageSectionPlane **pList);
/* Free up list of gsGensatImageSectionPlane. */

void gsGensatImageSectionPlaneSave(struct gsGensatImageSectionPlane *obj, int indent, FILE *f);
/* Save gsGensatImageSectionPlane to file. */

struct gsGensatImageSectionPlane *gsGensatImageSectionPlaneLoad(char *fileName);
/* Load gsGensatImageSectionPlane from file. */

struct gsGensatImageSectionLevel
    {
    struct gsGensatImageSectionLevel *next;
    char *text;
    };

void gsGensatImageSectionLevelFree(struct gsGensatImageSectionLevel **pObj);
/* Free up gsGensatImageSectionLevel. */

void gsGensatImageSectionLevelFreeList(struct gsGensatImageSectionLevel **pList);
/* Free up list of gsGensatImageSectionLevel. */

void gsGensatImageSectionLevelSave(struct gsGensatImageSectionLevel *obj, int indent, FILE *f);
/* Save gsGensatImageSectionLevel to file. */

struct gsGensatImageSectionLevel *gsGensatImageSectionLevelLoad(char *fileName);
/* Load gsGensatImageSectionLevel from file. */

struct gsGensatImageSacrificeDate
    {
    struct gsGensatImageSacrificeDate *next;
    struct gsDate *gsDate;	/** Single instance required. **/
    };

void gsGensatImageSacrificeDateFree(struct gsGensatImageSacrificeDate **pObj);
/* Free up gsGensatImageSacrificeDate. */

void gsGensatImageSacrificeDateFreeList(struct gsGensatImageSacrificeDate **pList);
/* Free up list of gsGensatImageSacrificeDate. */

void gsGensatImageSacrificeDateSave(struct gsGensatImageSacrificeDate *obj, int indent, FILE *f);
/* Save gsGensatImageSacrificeDate to file. */

struct gsGensatImageSacrificeDate *gsGensatImageSacrificeDateLoad(char *fileName);
/* Load gsGensatImageSacrificeDate from file. */

struct gsGensatImageSectionDate
    {
    struct gsGensatImageSectionDate *next;
    struct gsDate *gsDate;	/** Single instance required. **/
    };

void gsGensatImageSectionDateFree(struct gsGensatImageSectionDate **pObj);
/* Free up gsGensatImageSectionDate. */

void gsGensatImageSectionDateFreeList(struct gsGensatImageSectionDate **pList);
/* Free up list of gsGensatImageSectionDate. */

void gsGensatImageSectionDateSave(struct gsGensatImageSectionDate *obj, int indent, FILE *f);
/* Save gsGensatImageSectionDate to file. */

struct gsGensatImageSectionDate *gsGensatImageSectionDateLoad(char *fileName);
/* Load gsGensatImageSectionDate from file. */

struct gsGensatImageAnnotations
    {
    struct gsGensatImageAnnotations *next;
    struct gsGensatAnnotation *gsGensatAnnotation;	/** Possibly empty list. **/
    };

void gsGensatImageAnnotationsFree(struct gsGensatImageAnnotations **pObj);
/* Free up gsGensatImageAnnotations. */

void gsGensatImageAnnotationsFreeList(struct gsGensatImageAnnotations **pList);
/* Free up list of gsGensatImageAnnotations. */

void gsGensatImageAnnotationsSave(struct gsGensatImageAnnotations *obj, int indent, FILE *f);
/* Save gsGensatImageAnnotations to file. */

struct gsGensatImageAnnotations *gsGensatImageAnnotationsLoad(char *fileName);
/* Load gsGensatImageAnnotations from file. */

void *gsStartHandler(struct xap *xp, char *name, char **atts);
/* Called by expat with start tag.  Does most of the parsing work. */

void gsEndHandler(struct xap *xp, char *name);
/* Called by expat with end tag.  Checks all required children are loaded. */

#endif /* GS_H */


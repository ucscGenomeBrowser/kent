/* dasGff.h autoXml generated file */
#ifndef DASGFF_H
#define DASGFF_H

#ifndef XAP_H
#include "xap.h"
#endif

/* The start and end handlers here are used with routines defined in xap.h.
 * In particular if you want to read just parts of the XML file into memory
 * call xapOpen() with these, and then xapNext() with the name of the tag
 * you want to load. */

void *dasGffStartHandler(struct xap *xp, char *name, char **atts);
/* Called by xap with start tag.  Does most of the parsing work. */

void dasGffEndHandler(struct xap *xp, char *name);
/* Called by xap with end tag.  Checks all required children are loaded. */


struct dasGffDasGff
    {
    struct dasGffDasGff *next;
    struct dasGffGff *dasGffGff;	/** Single instance required. **/
    };

void dasGffDasGffFree(struct dasGffDasGff **pObj);
/* Free up dasGffDasGff. */

void dasGffDasGffFreeList(struct dasGffDasGff **pList);
/* Free up list of dasGffDasGff. */

void dasGffDasGffSave(struct dasGffDasGff *obj, int indent, FILE *f);
/* Save dasGffDasGff to file. */

struct dasGffDasGff *dasGffDasGffLoad(char *fileName);
/* Load dasGffDasGff from XML file where it is root element. */

struct dasGffDasGff *dasGffDasGffLoadNext(struct xap *xap);
/* Load next dasGffDasGff element.  Use xapOpen to get xap. */

struct dasGffNumber
    {
    struct dasGffNumber *next;
    int text;
    };

void dasGffNumberFree(struct dasGffNumber **pObj);
/* Free up dasGffNumber. */

void dasGffNumberFreeList(struct dasGffNumber **pList);
/* Free up list of dasGffNumber. */

void dasGffNumberSave(struct dasGffNumber *obj, int indent, FILE *f);
/* Save dasGffNumber to file. */

struct dasGffNumber *dasGffNumberLoad(char *fileName);
/* Load dasGffNumber from XML file where it is root element. */

struct dasGffNumber *dasGffNumberLoadNext(struct xap *xap);
/* Load next dasGffNumber element.  Use xapOpen to get xap. */

struct dasGffGff
    {
    struct dasGffGff *next;
    char *version;	/* Required */
    char *href;	/* Required */
    struct dasGffSegment *dasGffSegment;	/** Non-empty list required. **/
    };

void dasGffGffFree(struct dasGffGff **pObj);
/* Free up dasGffGff. */

void dasGffGffFreeList(struct dasGffGff **pList);
/* Free up list of dasGffGff. */

void dasGffGffSave(struct dasGffGff *obj, int indent, FILE *f);
/* Save dasGffGff to file. */

struct dasGffGff *dasGffGffLoad(char *fileName);
/* Load dasGffGff from XML file where it is root element. */

struct dasGffGff *dasGffGffLoadNext(struct xap *xap);
/* Load next dasGffGff element.  Use xapOpen to get xap. */

struct dasGffSegment
    {
    struct dasGffSegment *next;
    char *id;	/* Required */
    int start;	/* Required */
    int stop;	/* Required */
    double version;	/* Required */
    char *label;	/* Optional */
    struct dasGffFeature *dasGffFeature;	/** Non-empty list required. **/
    };

void dasGffSegmentFree(struct dasGffSegment **pObj);
/* Free up dasGffSegment. */

void dasGffSegmentFreeList(struct dasGffSegment **pList);
/* Free up list of dasGffSegment. */

void dasGffSegmentSave(struct dasGffSegment *obj, int indent, FILE *f);
/* Save dasGffSegment to file. */

struct dasGffSegment *dasGffSegmentLoad(char *fileName);
/* Load dasGffSegment from XML file where it is root element. */

struct dasGffSegment *dasGffSegmentLoadNext(struct xap *xap);
/* Load next dasGffSegment element.  Use xapOpen to get xap. */

struct dasGffFeature
    {
    struct dasGffFeature *next;
    char *id;	/* Required */
    char *label;	/* Optional */
    char *version;	/* Optional */
    struct dasGffType *dasGffType;	/** Single instance required. **/
    struct dasGffMethod *dasGffMethod;	/** Single instance required. **/
    struct dasGffStart *dasGffStart;	/** Single instance required. **/
    struct dasGffEnd *dasGffEnd;	/** Single instance required. **/
    struct dasGffScore *dasGffScore;	/** Single instance required. **/
    struct dasGffOrientation *dasGffOrientation;	/** Single instance required. **/
    struct dasGffPhase *dasGffPhase;	/** Single instance required. **/
    struct dasGffGroup *dasGffGroup;	/** Optional (may be NULL). **/
    };

void dasGffFeatureFree(struct dasGffFeature **pObj);
/* Free up dasGffFeature. */

void dasGffFeatureFreeList(struct dasGffFeature **pList);
/* Free up list of dasGffFeature. */

void dasGffFeatureSave(struct dasGffFeature *obj, int indent, FILE *f);
/* Save dasGffFeature to file. */

struct dasGffFeature *dasGffFeatureLoad(char *fileName);
/* Load dasGffFeature from XML file where it is root element. */

struct dasGffFeature *dasGffFeatureLoadNext(struct xap *xap);
/* Load next dasGffFeature element.  Use xapOpen to get xap. */

struct dasGffType
    {
    struct dasGffType *next;
    char *text;
    char *id;	/* Optional */
    char *category;	/* Optional */
    char *reference;	/* Defaults to no */
    char *subparts;	/* Defaults to no */
    };

void dasGffTypeFree(struct dasGffType **pObj);
/* Free up dasGffType. */

void dasGffTypeFreeList(struct dasGffType **pList);
/* Free up list of dasGffType. */

void dasGffTypeSave(struct dasGffType *obj, int indent, FILE *f);
/* Save dasGffType to file. */

struct dasGffType *dasGffTypeLoad(char *fileName);
/* Load dasGffType from XML file where it is root element. */

struct dasGffType *dasGffTypeLoadNext(struct xap *xap);
/* Load next dasGffType element.  Use xapOpen to get xap. */

struct dasGffMethod
    {
    struct dasGffMethod *next;
    char *text;
    char *id;	/* Optional */
    };

void dasGffMethodFree(struct dasGffMethod **pObj);
/* Free up dasGffMethod. */

void dasGffMethodFreeList(struct dasGffMethod **pList);
/* Free up list of dasGffMethod. */

void dasGffMethodSave(struct dasGffMethod *obj, int indent, FILE *f);
/* Save dasGffMethod to file. */

struct dasGffMethod *dasGffMethodLoad(char *fileName);
/* Load dasGffMethod from XML file where it is root element. */

struct dasGffMethod *dasGffMethodLoadNext(struct xap *xap);
/* Load next dasGffMethod element.  Use xapOpen to get xap. */

struct dasGffStart
    {
    struct dasGffStart *next;
    char *text;
    };

void dasGffStartFree(struct dasGffStart **pObj);
/* Free up dasGffStart. */

void dasGffStartFreeList(struct dasGffStart **pList);
/* Free up list of dasGffStart. */

void dasGffStartSave(struct dasGffStart *obj, int indent, FILE *f);
/* Save dasGffStart to file. */

struct dasGffStart *dasGffStartLoad(char *fileName);
/* Load dasGffStart from XML file where it is root element. */

struct dasGffStart *dasGffStartLoadNext(struct xap *xap);
/* Load next dasGffStart element.  Use xapOpen to get xap. */

struct dasGffEnd
    {
    struct dasGffEnd *next;
    char *text;
    };

void dasGffEndFree(struct dasGffEnd **pObj);
/* Free up dasGffEnd. */

void dasGffEndFreeList(struct dasGffEnd **pList);
/* Free up list of dasGffEnd. */

void dasGffEndSave(struct dasGffEnd *obj, int indent, FILE *f);
/* Save dasGffEnd to file. */

struct dasGffEnd *dasGffEndLoad(char *fileName);
/* Load dasGffEnd from XML file where it is root element. */

struct dasGffEnd *dasGffEndLoadNext(struct xap *xap);
/* Load next dasGffEnd element.  Use xapOpen to get xap. */

struct dasGffScore
    {
    struct dasGffScore *next;
    char *text;
    };

void dasGffScoreFree(struct dasGffScore **pObj);
/* Free up dasGffScore. */

void dasGffScoreFreeList(struct dasGffScore **pList);
/* Free up list of dasGffScore. */

void dasGffScoreSave(struct dasGffScore *obj, int indent, FILE *f);
/* Save dasGffScore to file. */

struct dasGffScore *dasGffScoreLoad(char *fileName);
/* Load dasGffScore from XML file where it is root element. */

struct dasGffScore *dasGffScoreLoadNext(struct xap *xap);
/* Load next dasGffScore element.  Use xapOpen to get xap. */

struct dasGffOrientation
    {
    struct dasGffOrientation *next;
    char *text;
    };

void dasGffOrientationFree(struct dasGffOrientation **pObj);
/* Free up dasGffOrientation. */

void dasGffOrientationFreeList(struct dasGffOrientation **pList);
/* Free up list of dasGffOrientation. */

void dasGffOrientationSave(struct dasGffOrientation *obj, int indent, FILE *f);
/* Save dasGffOrientation to file. */

struct dasGffOrientation *dasGffOrientationLoad(char *fileName);
/* Load dasGffOrientation from XML file where it is root element. */

struct dasGffOrientation *dasGffOrientationLoadNext(struct xap *xap);
/* Load next dasGffOrientation element.  Use xapOpen to get xap. */

struct dasGffPhase
    {
    struct dasGffPhase *next;
    char *text;
    };

void dasGffPhaseFree(struct dasGffPhase **pObj);
/* Free up dasGffPhase. */

void dasGffPhaseFreeList(struct dasGffPhase **pList);
/* Free up list of dasGffPhase. */

void dasGffPhaseSave(struct dasGffPhase *obj, int indent, FILE *f);
/* Save dasGffPhase to file. */

struct dasGffPhase *dasGffPhaseLoad(char *fileName);
/* Load dasGffPhase from XML file where it is root element. */

struct dasGffPhase *dasGffPhaseLoadNext(struct xap *xap);
/* Load next dasGffPhase element.  Use xapOpen to get xap. */

struct dasGffGroup
    {
    struct dasGffGroup *next;
    char *id;	/* Required */
    struct dasGffNote *dasGffNote;	/** Optional (may be NULL). **/
    struct dasGffLink *dasGffLink;	/** Optional (may be NULL). **/
    struct dasGffTarget *dasGffTarget;	/** Optional (may be NULL). **/
    };

void dasGffGroupFree(struct dasGffGroup **pObj);
/* Free up dasGffGroup. */

void dasGffGroupFreeList(struct dasGffGroup **pList);
/* Free up list of dasGffGroup. */

void dasGffGroupSave(struct dasGffGroup *obj, int indent, FILE *f);
/* Save dasGffGroup to file. */

struct dasGffGroup *dasGffGroupLoad(char *fileName);
/* Load dasGffGroup from XML file where it is root element. */

struct dasGffGroup *dasGffGroupLoadNext(struct xap *xap);
/* Load next dasGffGroup element.  Use xapOpen to get xap. */

struct dasGffNote
    {
    struct dasGffNote *next;
    char *text;
    };

void dasGffNoteFree(struct dasGffNote **pObj);
/* Free up dasGffNote. */

void dasGffNoteFreeList(struct dasGffNote **pList);
/* Free up list of dasGffNote. */

void dasGffNoteSave(struct dasGffNote *obj, int indent, FILE *f);
/* Save dasGffNote to file. */

struct dasGffNote *dasGffNoteLoad(char *fileName);
/* Load dasGffNote from XML file where it is root element. */

struct dasGffNote *dasGffNoteLoadNext(struct xap *xap);
/* Load next dasGffNote element.  Use xapOpen to get xap. */

struct dasGffLink
    {
    struct dasGffLink *next;
    char *text;
    char *href;	/* Required */
    };

void dasGffLinkFree(struct dasGffLink **pObj);
/* Free up dasGffLink. */

void dasGffLinkFreeList(struct dasGffLink **pList);
/* Free up list of dasGffLink. */

void dasGffLinkSave(struct dasGffLink *obj, int indent, FILE *f);
/* Save dasGffLink to file. */

struct dasGffLink *dasGffLinkLoad(char *fileName);
/* Load dasGffLink from XML file where it is root element. */

struct dasGffLink *dasGffLinkLoadNext(struct xap *xap);
/* Load next dasGffLink element.  Use xapOpen to get xap. */

struct dasGffTarget
    {
    struct dasGffTarget *next;
    char *text;
    char *id;	/* Required */
    char *start;	/* Required */
    char *stop;	/* Required */
    };

void dasGffTargetFree(struct dasGffTarget **pObj);
/* Free up dasGffTarget. */

void dasGffTargetFreeList(struct dasGffTarget **pList);
/* Free up list of dasGffTarget. */

void dasGffTargetSave(struct dasGffTarget *obj, int indent, FILE *f);
/* Save dasGffTarget to file. */

struct dasGffTarget *dasGffTargetLoad(char *fileName);
/* Load dasGffTarget from XML file where it is root element. */

struct dasGffTarget *dasGffTargetLoadNext(struct xap *xap);
/* Load next dasGffTarget element.  Use xapOpen to get xap. */

#endif /* DASGFF_H */


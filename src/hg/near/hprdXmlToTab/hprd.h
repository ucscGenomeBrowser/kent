/* hprd.h autoXml generated file */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef HPRD_H
#define HPRD_H

#ifndef XAP_H
#include "xap.h"
#endif

/* The start and end handlers here are used with routines defined in xap.h.
 * In particular if you want to read just parts of the XML file into memory
 * call xapOpen() with these, and then xapNext() with the name of the tag
 * you want to load. */

void *hprdStartHandler(struct xap *xp, char *name, char **atts);
/* Called by xap with start tag.  Does most of the parsing work. */

void hprdEndHandler(struct xap *xp, char *name);
/* Called by xap with end tag.  Checks all required children are loaded. */


struct hprdEntrySet
    {
    struct hprdEntrySet *next;
    char *level;	/* Required */
    char *version;	/* Required */
    char *xmlns;	/* Required */
    char *xmlnsxsi;	/* Required */
    char *xsischemaLocation;	/* Required */
    struct hprdEntry *hprdEntry;	/** Single instance required. **/
    };

void hprdEntrySetFree(struct hprdEntrySet **pObj);
/* Free up hprdEntrySet. */

void hprdEntrySetFreeList(struct hprdEntrySet **pList);
/* Free up list of hprdEntrySet. */

void hprdEntrySetSave(struct hprdEntrySet *obj, int indent, FILE *f);
/* Save hprdEntrySet to file. */

struct hprdEntrySet *hprdEntrySetLoad(char *fileName);
/* Load hprdEntrySet from XML file where it is root element. */

struct hprdEntrySet *hprdEntrySetLoadNext(struct xap *xap);
/* Load next hprdEntrySet element.  Use xapOpen to get xap. */

struct hprdEntry
    {
    struct hprdEntry *next;
    struct hprdSource *hprdSource;	/** Single instance required. **/
    struct hprdAvailabilityList *hprdAvailabilityList;	/** Single instance required. **/
    struct hprdExperimentList *hprdExperimentList;	/** Single instance required. **/
    struct hprdInteractorList *hprdInteractorList;	/** Single instance required. **/
    struct hprdInteractionList *hprdInteractionList;	/** Single instance required. **/
    };

void hprdEntryFree(struct hprdEntry **pObj);
/* Free up hprdEntry. */

void hprdEntryFreeList(struct hprdEntry **pList);
/* Free up list of hprdEntry. */

void hprdEntrySave(struct hprdEntry *obj, int indent, FILE *f);
/* Save hprdEntry to file. */

struct hprdEntry *hprdEntryLoad(char *fileName);
/* Load hprdEntry from XML file where it is root element. */

struct hprdEntry *hprdEntryLoadNext(struct xap *xap);
/* Load next hprdEntry element.  Use xapOpen to get xap. */

struct hprdSource
    {
    struct hprdSource *next;
    struct hprdNames *hprdNames;	/** Single instance required. **/
    struct hprdBibref *hprdBibref;	/** Single instance required. **/
    };

void hprdSourceFree(struct hprdSource **pObj);
/* Free up hprdSource. */

void hprdSourceFreeList(struct hprdSource **pList);
/* Free up list of hprdSource. */

void hprdSourceSave(struct hprdSource *obj, int indent, FILE *f);
/* Save hprdSource to file. */

struct hprdSource *hprdSourceLoad(char *fileName);
/* Load hprdSource from XML file where it is root element. */

struct hprdSource *hprdSourceLoadNext(struct xap *xap);
/* Load next hprdSource element.  Use xapOpen to get xap. */

struct hprdNames
    {
    struct hprdNames *next;
    struct hprdShortLabel *hprdShortLabel;	/** Single instance required. **/
    struct hprdFullName *hprdFullName;	/** Optional (may be NULL). **/
    struct hprdAlias *hprdAlias;	/** Possibly empty list. **/
    };

void hprdNamesFree(struct hprdNames **pObj);
/* Free up hprdNames. */

void hprdNamesFreeList(struct hprdNames **pList);
/* Free up list of hprdNames. */

void hprdNamesSave(struct hprdNames *obj, int indent, FILE *f);
/* Save hprdNames to file. */

struct hprdNames *hprdNamesLoad(char *fileName);
/* Load hprdNames from XML file where it is root element. */

struct hprdNames *hprdNamesLoadNext(struct xap *xap);
/* Load next hprdNames element.  Use xapOpen to get xap. */

struct hprdShortLabel
    {
    struct hprdShortLabel *next;
    char *text;
    };

void hprdShortLabelFree(struct hprdShortLabel **pObj);
/* Free up hprdShortLabel. */

void hprdShortLabelFreeList(struct hprdShortLabel **pList);
/* Free up list of hprdShortLabel. */

void hprdShortLabelSave(struct hprdShortLabel *obj, int indent, FILE *f);
/* Save hprdShortLabel to file. */

struct hprdShortLabel *hprdShortLabelLoad(char *fileName);
/* Load hprdShortLabel from XML file where it is root element. */

struct hprdShortLabel *hprdShortLabelLoadNext(struct xap *xap);
/* Load next hprdShortLabel element.  Use xapOpen to get xap. */

struct hprdFullName
    {
    struct hprdFullName *next;
    char *text;
    };

void hprdFullNameFree(struct hprdFullName **pObj);
/* Free up hprdFullName. */

void hprdFullNameFreeList(struct hprdFullName **pList);
/* Free up list of hprdFullName. */

void hprdFullNameSave(struct hprdFullName *obj, int indent, FILE *f);
/* Save hprdFullName to file. */

struct hprdFullName *hprdFullNameLoad(char *fileName);
/* Load hprdFullName from XML file where it is root element. */

struct hprdFullName *hprdFullNameLoadNext(struct xap *xap);
/* Load next hprdFullName element.  Use xapOpen to get xap. */

struct hprdAlias
    {
    struct hprdAlias *next;
    char *text;
    char *type;	/* Required */
    char *typeAc;	/* Required */
    };

void hprdAliasFree(struct hprdAlias **pObj);
/* Free up hprdAlias. */

void hprdAliasFreeList(struct hprdAlias **pList);
/* Free up list of hprdAlias. */

void hprdAliasSave(struct hprdAlias *obj, int indent, FILE *f);
/* Save hprdAlias to file. */

struct hprdAlias *hprdAliasLoad(char *fileName);
/* Load hprdAlias from XML file where it is root element. */

struct hprdAlias *hprdAliasLoadNext(struct xap *xap);
/* Load next hprdAlias element.  Use xapOpen to get xap. */

struct hprdBibref
    {
    struct hprdBibref *next;
    struct hprdXref *hprdXref;	/** Single instance required. **/
    };

void hprdBibrefFree(struct hprdBibref **pObj);
/* Free up hprdBibref. */

void hprdBibrefFreeList(struct hprdBibref **pList);
/* Free up list of hprdBibref. */

void hprdBibrefSave(struct hprdBibref *obj, int indent, FILE *f);
/* Save hprdBibref to file. */

struct hprdBibref *hprdBibrefLoad(char *fileName);
/* Load hprdBibref from XML file where it is root element. */

struct hprdBibref *hprdBibrefLoadNext(struct xap *xap);
/* Load next hprdBibref element.  Use xapOpen to get xap. */

struct hprdXref
    {
    struct hprdXref *next;
    struct hprdPrimaryRef *hprdPrimaryRef;	/** Single instance required. **/
    struct hprdSecondaryRef *hprdSecondaryRef;	/** Possibly empty list. **/
    };

void hprdXrefFree(struct hprdXref **pObj);
/* Free up hprdXref. */

void hprdXrefFreeList(struct hprdXref **pList);
/* Free up list of hprdXref. */

void hprdXrefSave(struct hprdXref *obj, int indent, FILE *f);
/* Save hprdXref to file. */

struct hprdXref *hprdXrefLoad(char *fileName);
/* Load hprdXref from XML file where it is root element. */

struct hprdXref *hprdXrefLoadNext(struct xap *xap);
/* Load next hprdXref element.  Use xapOpen to get xap. */

struct hprdPrimaryRef
    {
    struct hprdPrimaryRef *next;
    char *db;	/* Required */
    char *id;	/* Required */
    char *dbAc;	/* Optional */
    char *refType;	/* Optional */
    char *refTypeAc;	/* Optional */
    };

void hprdPrimaryRefFree(struct hprdPrimaryRef **pObj);
/* Free up hprdPrimaryRef. */

void hprdPrimaryRefFreeList(struct hprdPrimaryRef **pList);
/* Free up list of hprdPrimaryRef. */

void hprdPrimaryRefSave(struct hprdPrimaryRef *obj, int indent, FILE *f);
/* Save hprdPrimaryRef to file. */

struct hprdPrimaryRef *hprdPrimaryRefLoad(char *fileName);
/* Load hprdPrimaryRef from XML file where it is root element. */

struct hprdPrimaryRef *hprdPrimaryRefLoadNext(struct xap *xap);
/* Load next hprdPrimaryRef element.  Use xapOpen to get xap. */

struct hprdSecondaryRef
    {
    struct hprdSecondaryRef *next;
    char *db;	/* Required */
    char *id;	/* Required */
    char *dbAc;	/* Optional */
    char *refType;	/* Optional */
    char *refTypeAc;	/* Optional */
    };

void hprdSecondaryRefFree(struct hprdSecondaryRef **pObj);
/* Free up hprdSecondaryRef. */

void hprdSecondaryRefFreeList(struct hprdSecondaryRef **pList);
/* Free up list of hprdSecondaryRef. */

void hprdSecondaryRefSave(struct hprdSecondaryRef *obj, int indent, FILE *f);
/* Save hprdSecondaryRef to file. */

struct hprdSecondaryRef *hprdSecondaryRefLoad(char *fileName);
/* Load hprdSecondaryRef from XML file where it is root element. */

struct hprdSecondaryRef *hprdSecondaryRefLoadNext(struct xap *xap);
/* Load next hprdSecondaryRef element.  Use xapOpen to get xap. */

struct hprdAvailabilityList
    {
    struct hprdAvailabilityList *next;
    struct hprdAvailability *hprdAvailability;	/** Single instance required. **/
    };

void hprdAvailabilityListFree(struct hprdAvailabilityList **pObj);
/* Free up hprdAvailabilityList. */

void hprdAvailabilityListFreeList(struct hprdAvailabilityList **pList);
/* Free up list of hprdAvailabilityList. */

void hprdAvailabilityListSave(struct hprdAvailabilityList *obj, int indent, FILE *f);
/* Save hprdAvailabilityList to file. */

struct hprdAvailabilityList *hprdAvailabilityListLoad(char *fileName);
/* Load hprdAvailabilityList from XML file where it is root element. */

struct hprdAvailabilityList *hprdAvailabilityListLoadNext(struct xap *xap);
/* Load next hprdAvailabilityList element.  Use xapOpen to get xap. */

struct hprdAvailability
    {
    struct hprdAvailability *next;
    char *text;
    char *id;	/* Required */
    };

void hprdAvailabilityFree(struct hprdAvailability **pObj);
/* Free up hprdAvailability. */

void hprdAvailabilityFreeList(struct hprdAvailability **pList);
/* Free up list of hprdAvailability. */

void hprdAvailabilitySave(struct hprdAvailability *obj, int indent, FILE *f);
/* Save hprdAvailability to file. */

struct hprdAvailability *hprdAvailabilityLoad(char *fileName);
/* Load hprdAvailability from XML file where it is root element. */

struct hprdAvailability *hprdAvailabilityLoadNext(struct xap *xap);
/* Load next hprdAvailability element.  Use xapOpen to get xap. */

struct hprdExperimentList
    {
    struct hprdExperimentList *next;
    struct hprdExperimentDescription *hprdExperimentDescription;	/** Possibly empty list. **/
    struct hprdExperimentRef *hprdExperimentRef;	/** Possibly empty list. **/
    };

void hprdExperimentListFree(struct hprdExperimentList **pObj);
/* Free up hprdExperimentList. */

void hprdExperimentListFreeList(struct hprdExperimentList **pList);
/* Free up list of hprdExperimentList. */

void hprdExperimentListSave(struct hprdExperimentList *obj, int indent, FILE *f);
/* Save hprdExperimentList to file. */

struct hprdExperimentList *hprdExperimentListLoad(char *fileName);
/* Load hprdExperimentList from XML file where it is root element. */

struct hprdExperimentList *hprdExperimentListLoadNext(struct xap *xap);
/* Load next hprdExperimentList element.  Use xapOpen to get xap. */

struct hprdExperimentDescription
    {
    struct hprdExperimentDescription *next;
    char *id;	/* Required */
    struct hprdBibref *hprdBibref;	/** Single instance required. **/
    struct hprdInteractionDetectionMethod *hprdInteractionDetectionMethod;	/** Single instance required. **/
    };

void hprdExperimentDescriptionFree(struct hprdExperimentDescription **pObj);
/* Free up hprdExperimentDescription. */

void hprdExperimentDescriptionFreeList(struct hprdExperimentDescription **pList);
/* Free up list of hprdExperimentDescription. */

void hprdExperimentDescriptionSave(struct hprdExperimentDescription *obj, int indent, FILE *f);
/* Save hprdExperimentDescription to file. */

struct hprdExperimentDescription *hprdExperimentDescriptionLoad(char *fileName);
/* Load hprdExperimentDescription from XML file where it is root element. */

struct hprdExperimentDescription *hprdExperimentDescriptionLoadNext(struct xap *xap);
/* Load next hprdExperimentDescription element.  Use xapOpen to get xap. */

struct hprdInteractionDetectionMethod
    {
    struct hprdInteractionDetectionMethod *next;
    struct hprdNames *hprdNames;	/** Single instance required. **/
    struct hprdXref *hprdXref;	/** Single instance required. **/
    };

void hprdInteractionDetectionMethodFree(struct hprdInteractionDetectionMethod **pObj);
/* Free up hprdInteractionDetectionMethod. */

void hprdInteractionDetectionMethodFreeList(struct hprdInteractionDetectionMethod **pList);
/* Free up list of hprdInteractionDetectionMethod. */

void hprdInteractionDetectionMethodSave(struct hprdInteractionDetectionMethod *obj, int indent, FILE *f);
/* Save hprdInteractionDetectionMethod to file. */

struct hprdInteractionDetectionMethod *hprdInteractionDetectionMethodLoad(char *fileName);
/* Load hprdInteractionDetectionMethod from XML file where it is root element. */

struct hprdInteractionDetectionMethod *hprdInteractionDetectionMethodLoadNext(struct xap *xap);
/* Load next hprdInteractionDetectionMethod element.  Use xapOpen to get xap. */

struct hprdExperimentRef
    {
    struct hprdExperimentRef *next;
    char *text;
    };

void hprdExperimentRefFree(struct hprdExperimentRef **pObj);
/* Free up hprdExperimentRef. */

void hprdExperimentRefFreeList(struct hprdExperimentRef **pList);
/* Free up list of hprdExperimentRef. */

void hprdExperimentRefSave(struct hprdExperimentRef *obj, int indent, FILE *f);
/* Save hprdExperimentRef to file. */

struct hprdExperimentRef *hprdExperimentRefLoad(char *fileName);
/* Load hprdExperimentRef from XML file where it is root element. */

struct hprdExperimentRef *hprdExperimentRefLoadNext(struct xap *xap);
/* Load next hprdExperimentRef element.  Use xapOpen to get xap. */

struct hprdInteractorList
    {
    struct hprdInteractorList *next;
    struct hprdInteractor *hprdInteractor;	/** Non-empty list required. **/
    };

void hprdInteractorListFree(struct hprdInteractorList **pObj);
/* Free up hprdInteractorList. */

void hprdInteractorListFreeList(struct hprdInteractorList **pList);
/* Free up list of hprdInteractorList. */

void hprdInteractorListSave(struct hprdInteractorList *obj, int indent, FILE *f);
/* Save hprdInteractorList to file. */

struct hprdInteractorList *hprdInteractorListLoad(char *fileName);
/* Load hprdInteractorList from XML file where it is root element. */

struct hprdInteractorList *hprdInteractorListLoadNext(struct xap *xap);
/* Load next hprdInteractorList element.  Use xapOpen to get xap. */

struct hprdInteractor
    {
    struct hprdInteractor *next;
    char *id;	/* Required */
    struct hprdNames *hprdNames;	/** Single instance required. **/
    struct hprdXref *hprdXref;	/** Single instance required. **/
    struct hprdInteractorType *hprdInteractorType;	/** Single instance required. **/
    struct hprdOrganism *hprdOrganism;	/** Single instance required. **/
    struct hprdSequence *hprdSequence;	/** Single instance required. **/
    };

void hprdInteractorFree(struct hprdInteractor **pObj);
/* Free up hprdInteractor. */

void hprdInteractorFreeList(struct hprdInteractor **pList);
/* Free up list of hprdInteractor. */

void hprdInteractorSave(struct hprdInteractor *obj, int indent, FILE *f);
/* Save hprdInteractor to file. */

struct hprdInteractor *hprdInteractorLoad(char *fileName);
/* Load hprdInteractor from XML file where it is root element. */

struct hprdInteractor *hprdInteractorLoadNext(struct xap *xap);
/* Load next hprdInteractor element.  Use xapOpen to get xap. */

struct hprdInteractorType
    {
    struct hprdInteractorType *next;
    struct hprdNames *hprdNames;	/** Single instance required. **/
    struct hprdXref *hprdXref;	/** Single instance required. **/
    };

void hprdInteractorTypeFree(struct hprdInteractorType **pObj);
/* Free up hprdInteractorType. */

void hprdInteractorTypeFreeList(struct hprdInteractorType **pList);
/* Free up list of hprdInteractorType. */

void hprdInteractorTypeSave(struct hprdInteractorType *obj, int indent, FILE *f);
/* Save hprdInteractorType to file. */

struct hprdInteractorType *hprdInteractorTypeLoad(char *fileName);
/* Load hprdInteractorType from XML file where it is root element. */

struct hprdInteractorType *hprdInteractorTypeLoadNext(struct xap *xap);
/* Load next hprdInteractorType element.  Use xapOpen to get xap. */

struct hprdOrganism
    {
    struct hprdOrganism *next;
    char *ncbiTaxId;	/* Required */
    struct hprdNames *hprdNames;	/** Single instance required. **/
    };

void hprdOrganismFree(struct hprdOrganism **pObj);
/* Free up hprdOrganism. */

void hprdOrganismFreeList(struct hprdOrganism **pList);
/* Free up list of hprdOrganism. */

void hprdOrganismSave(struct hprdOrganism *obj, int indent, FILE *f);
/* Save hprdOrganism to file. */

struct hprdOrganism *hprdOrganismLoad(char *fileName);
/* Load hprdOrganism from XML file where it is root element. */

struct hprdOrganism *hprdOrganismLoadNext(struct xap *xap);
/* Load next hprdOrganism element.  Use xapOpen to get xap. */

struct hprdSequence
    {
    struct hprdSequence *next;
    char *text;
    };

void hprdSequenceFree(struct hprdSequence **pObj);
/* Free up hprdSequence. */

void hprdSequenceFreeList(struct hprdSequence **pList);
/* Free up list of hprdSequence. */

void hprdSequenceSave(struct hprdSequence *obj, int indent, FILE *f);
/* Save hprdSequence to file. */

struct hprdSequence *hprdSequenceLoad(char *fileName);
/* Load hprdSequence from XML file where it is root element. */

struct hprdSequence *hprdSequenceLoadNext(struct xap *xap);
/* Load next hprdSequence element.  Use xapOpen to get xap. */

struct hprdInteractionList
    {
    struct hprdInteractionList *next;
    struct hprdInteraction *hprdInteraction;	/** Non-empty list required. **/
    };

void hprdInteractionListFree(struct hprdInteractionList **pObj);
/* Free up hprdInteractionList. */

void hprdInteractionListFreeList(struct hprdInteractionList **pList);
/* Free up list of hprdInteractionList. */

void hprdInteractionListSave(struct hprdInteractionList *obj, int indent, FILE *f);
/* Save hprdInteractionList to file. */

struct hprdInteractionList *hprdInteractionListLoad(char *fileName);
/* Load hprdInteractionList from XML file where it is root element. */

struct hprdInteractionList *hprdInteractionListLoadNext(struct xap *xap);
/* Load next hprdInteractionList element.  Use xapOpen to get xap. */

struct hprdInteraction
    {
    struct hprdInteraction *next;
    char *id;	/* Required */
    struct hprdExperimentList *hprdExperimentList;	/** Single instance required. **/
    struct hprdParticipantList *hprdParticipantList;	/** Single instance required. **/
    };

void hprdInteractionFree(struct hprdInteraction **pObj);
/* Free up hprdInteraction. */

void hprdInteractionFreeList(struct hprdInteraction **pList);
/* Free up list of hprdInteraction. */

void hprdInteractionSave(struct hprdInteraction *obj, int indent, FILE *f);
/* Save hprdInteraction to file. */

struct hprdInteraction *hprdInteractionLoad(char *fileName);
/* Load hprdInteraction from XML file where it is root element. */

struct hprdInteraction *hprdInteractionLoadNext(struct xap *xap);
/* Load next hprdInteraction element.  Use xapOpen to get xap. */

struct hprdParticipantList
    {
    struct hprdParticipantList *next;
    struct hprdParticipant *hprdParticipant;	/** Non-empty list required. **/
    };

void hprdParticipantListFree(struct hprdParticipantList **pObj);
/* Free up hprdParticipantList. */

void hprdParticipantListFreeList(struct hprdParticipantList **pList);
/* Free up list of hprdParticipantList. */

void hprdParticipantListSave(struct hprdParticipantList *obj, int indent, FILE *f);
/* Save hprdParticipantList to file. */

struct hprdParticipantList *hprdParticipantListLoad(char *fileName);
/* Load hprdParticipantList from XML file where it is root element. */

struct hprdParticipantList *hprdParticipantListLoadNext(struct xap *xap);
/* Load next hprdParticipantList element.  Use xapOpen to get xap. */

struct hprdParticipant
    {
    struct hprdParticipant *next;
    char *id;	/* Required */
    struct hprdInteractorRef *hprdInteractorRef;	/** Single instance required. **/
    };

void hprdParticipantFree(struct hprdParticipant **pObj);
/* Free up hprdParticipant. */

void hprdParticipantFreeList(struct hprdParticipant **pList);
/* Free up list of hprdParticipant. */

void hprdParticipantSave(struct hprdParticipant *obj, int indent, FILE *f);
/* Save hprdParticipant to file. */

struct hprdParticipant *hprdParticipantLoad(char *fileName);
/* Load hprdParticipant from XML file where it is root element. */

struct hprdParticipant *hprdParticipantLoadNext(struct xap *xap);
/* Load next hprdParticipant element.  Use xapOpen to get xap. */

struct hprdInteractorRef
    {
    struct hprdInteractorRef *next;
    char *text;
    };

void hprdInteractorRefFree(struct hprdInteractorRef **pObj);
/* Free up hprdInteractorRef. */

void hprdInteractorRefFreeList(struct hprdInteractorRef **pList);
/* Free up list of hprdInteractorRef. */

void hprdInteractorRefSave(struct hprdInteractorRef *obj, int indent, FILE *f);
/* Save hprdInteractorRef to file. */

struct hprdInteractorRef *hprdInteractorRefLoad(char *fileName);
/* Load hprdInteractorRef from XML file where it is root element. */

struct hprdInteractorRef *hprdInteractorRefLoadNext(struct xap *xap);
/* Load next hprdInteractorRef element.  Use xapOpen to get xap. */

#endif /* HPRD_H */


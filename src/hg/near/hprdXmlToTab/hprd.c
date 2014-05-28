/* hprd.c autoXml generated file */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "xap.h"
#include "hprd.h"


void hprdEntrySetFree(struct hprdEntrySet **pObj)
/* Free up hprdEntrySet. */
{
struct hprdEntrySet *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->level);
freeMem(obj->version);
freeMem(obj->xmlns);
freeMem(obj->xmlnsxsi);
freeMem(obj->xsischemaLocation);
hprdEntryFree(&obj->hprdEntry);
freez(pObj);
}

void hprdEntrySetFreeList(struct hprdEntrySet **pList)
/* Free up list of hprdEntrySet. */
{
struct hprdEntrySet *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdEntrySetFree(&el);
    el = next;
    }
}

void hprdEntrySetSave(struct hprdEntrySet *obj, int indent, FILE *f)
/* Save hprdEntrySet to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<entrySet");
fprintf(f, " level=\"%s\"", obj->level);
fprintf(f, " version=\"%s\"", obj->version);
fprintf(f, " xmlns=\"%s\"", obj->xmlns);
fprintf(f, " xmlnsxsi=\"%s\"", obj->xmlnsxsi);
fprintf(f, " xsischemaLocation=\"%s\"", obj->xsischemaLocation);
fprintf(f, ">");
fprintf(f, "\n");
hprdEntrySave(obj->hprdEntry, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</entrySet>\n");
}

struct hprdEntrySet *hprdEntrySetLoad(char *fileName)
/* Load hprdEntrySet from XML file where it is root element. */
{
struct hprdEntrySet *obj;
xapParseAny(fileName, "entrySet", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdEntrySet *hprdEntrySetLoadNext(struct xap *xap)
/* Load next hprdEntrySet element.  Use xapOpen to get xap. */
{
return xapNext(xap, "entrySet");
}

void hprdEntryFree(struct hprdEntry **pObj)
/* Free up hprdEntry. */
{
struct hprdEntry *obj = *pObj;
if (obj == NULL) return;
hprdSourceFree(&obj->hprdSource);
hprdAvailabilityListFree(&obj->hprdAvailabilityList);
hprdExperimentListFree(&obj->hprdExperimentList);
hprdInteractorListFree(&obj->hprdInteractorList);
hprdInteractionListFree(&obj->hprdInteractionList);
freez(pObj);
}

void hprdEntryFreeList(struct hprdEntry **pList)
/* Free up list of hprdEntry. */
{
struct hprdEntry *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdEntryFree(&el);
    el = next;
    }
}

void hprdEntrySave(struct hprdEntry *obj, int indent, FILE *f)
/* Save hprdEntry to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<entry>");
fprintf(f, "\n");
hprdSourceSave(obj->hprdSource, indent+2, f);
hprdAvailabilityListSave(obj->hprdAvailabilityList, indent+2, f);
hprdExperimentListSave(obj->hprdExperimentList, indent+2, f);
hprdInteractorListSave(obj->hprdInteractorList, indent+2, f);
hprdInteractionListSave(obj->hprdInteractionList, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</entry>\n");
}

struct hprdEntry *hprdEntryLoad(char *fileName)
/* Load hprdEntry from XML file where it is root element. */
{
struct hprdEntry *obj;
xapParseAny(fileName, "entry", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdEntry *hprdEntryLoadNext(struct xap *xap)
/* Load next hprdEntry element.  Use xapOpen to get xap. */
{
return xapNext(xap, "entry");
}

void hprdSourceFree(struct hprdSource **pObj)
/* Free up hprdSource. */
{
struct hprdSource *obj = *pObj;
if (obj == NULL) return;
hprdNamesFree(&obj->hprdNames);
hprdBibrefFree(&obj->hprdBibref);
freez(pObj);
}

void hprdSourceFreeList(struct hprdSource **pList)
/* Free up list of hprdSource. */
{
struct hprdSource *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdSourceFree(&el);
    el = next;
    }
}

void hprdSourceSave(struct hprdSource *obj, int indent, FILE *f)
/* Save hprdSource to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<source>");
fprintf(f, "\n");
hprdNamesSave(obj->hprdNames, indent+2, f);
hprdBibrefSave(obj->hprdBibref, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</source>\n");
}

struct hprdSource *hprdSourceLoad(char *fileName)
/* Load hprdSource from XML file where it is root element. */
{
struct hprdSource *obj;
xapParseAny(fileName, "source", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdSource *hprdSourceLoadNext(struct xap *xap)
/* Load next hprdSource element.  Use xapOpen to get xap. */
{
return xapNext(xap, "source");
}

void hprdNamesFree(struct hprdNames **pObj)
/* Free up hprdNames. */
{
struct hprdNames *obj = *pObj;
if (obj == NULL) return;
hprdShortLabelFree(&obj->hprdShortLabel);
hprdFullNameFree(&obj->hprdFullName);
hprdAliasFreeList(&obj->hprdAlias);
freez(pObj);
}

void hprdNamesFreeList(struct hprdNames **pList)
/* Free up list of hprdNames. */
{
struct hprdNames *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdNamesFree(&el);
    el = next;
    }
}

void hprdNamesSave(struct hprdNames *obj, int indent, FILE *f)
/* Save hprdNames to file. */
{
struct hprdAlias *hprdAlias;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<names>");
fprintf(f, "\n");
hprdShortLabelSave(obj->hprdShortLabel, indent+2, f);
hprdFullNameSave(obj->hprdFullName, indent+2, f);
for (hprdAlias = obj->hprdAlias; hprdAlias != NULL; hprdAlias = hprdAlias->next)
   {
   hprdAliasSave(hprdAlias, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</names>\n");
}

struct hprdNames *hprdNamesLoad(char *fileName)
/* Load hprdNames from XML file where it is root element. */
{
struct hprdNames *obj;
xapParseAny(fileName, "names", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdNames *hprdNamesLoadNext(struct xap *xap)
/* Load next hprdNames element.  Use xapOpen to get xap. */
{
return xapNext(xap, "names");
}

void hprdShortLabelFree(struct hprdShortLabel **pObj)
/* Free up hprdShortLabel. */
{
struct hprdShortLabel *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void hprdShortLabelFreeList(struct hprdShortLabel **pList)
/* Free up list of hprdShortLabel. */
{
struct hprdShortLabel *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdShortLabelFree(&el);
    el = next;
    }
}

void hprdShortLabelSave(struct hprdShortLabel *obj, int indent, FILE *f)
/* Save hprdShortLabel to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<shortLabel>");
fprintf(f, "%s", obj->text);
fprintf(f, "</shortLabel>\n");
}

struct hprdShortLabel *hprdShortLabelLoad(char *fileName)
/* Load hprdShortLabel from XML file where it is root element. */
{
struct hprdShortLabel *obj;
xapParseAny(fileName, "shortLabel", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdShortLabel *hprdShortLabelLoadNext(struct xap *xap)
/* Load next hprdShortLabel element.  Use xapOpen to get xap. */
{
return xapNext(xap, "shortLabel");
}

void hprdFullNameFree(struct hprdFullName **pObj)
/* Free up hprdFullName. */
{
struct hprdFullName *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void hprdFullNameFreeList(struct hprdFullName **pList)
/* Free up list of hprdFullName. */
{
struct hprdFullName *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdFullNameFree(&el);
    el = next;
    }
}

void hprdFullNameSave(struct hprdFullName *obj, int indent, FILE *f)
/* Save hprdFullName to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<fullName>");
fprintf(f, "%s", obj->text);
fprintf(f, "</fullName>\n");
}

struct hprdFullName *hprdFullNameLoad(char *fileName)
/* Load hprdFullName from XML file where it is root element. */
{
struct hprdFullName *obj;
xapParseAny(fileName, "fullName", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdFullName *hprdFullNameLoadNext(struct xap *xap)
/* Load next hprdFullName element.  Use xapOpen to get xap. */
{
return xapNext(xap, "fullName");
}

void hprdAliasFree(struct hprdAlias **pObj)
/* Free up hprdAlias. */
{
struct hprdAlias *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->type);
freeMem(obj->typeAc);
freeMem(obj->text);
freez(pObj);
}

void hprdAliasFreeList(struct hprdAlias **pList)
/* Free up list of hprdAlias. */
{
struct hprdAlias *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdAliasFree(&el);
    el = next;
    }
}

void hprdAliasSave(struct hprdAlias *obj, int indent, FILE *f)
/* Save hprdAlias to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<alias");
fprintf(f, " type=\"%s\"", obj->type);
fprintf(f, " typeAc=\"%s\"", obj->typeAc);
fprintf(f, ">");
fprintf(f, "%s", obj->text);
fprintf(f, "</alias>\n");
}

struct hprdAlias *hprdAliasLoad(char *fileName)
/* Load hprdAlias from XML file where it is root element. */
{
struct hprdAlias *obj;
xapParseAny(fileName, "alias", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdAlias *hprdAliasLoadNext(struct xap *xap)
/* Load next hprdAlias element.  Use xapOpen to get xap. */
{
return xapNext(xap, "alias");
}

void hprdBibrefFree(struct hprdBibref **pObj)
/* Free up hprdBibref. */
{
struct hprdBibref *obj = *pObj;
if (obj == NULL) return;
hprdXrefFree(&obj->hprdXref);
freez(pObj);
}

void hprdBibrefFreeList(struct hprdBibref **pList)
/* Free up list of hprdBibref. */
{
struct hprdBibref *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdBibrefFree(&el);
    el = next;
    }
}

void hprdBibrefSave(struct hprdBibref *obj, int indent, FILE *f)
/* Save hprdBibref to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<bibref>");
fprintf(f, "\n");
hprdXrefSave(obj->hprdXref, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</bibref>\n");
}

struct hprdBibref *hprdBibrefLoad(char *fileName)
/* Load hprdBibref from XML file where it is root element. */
{
struct hprdBibref *obj;
xapParseAny(fileName, "bibref", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdBibref *hprdBibrefLoadNext(struct xap *xap)
/* Load next hprdBibref element.  Use xapOpen to get xap. */
{
return xapNext(xap, "bibref");
}

void hprdXrefFree(struct hprdXref **pObj)
/* Free up hprdXref. */
{
struct hprdXref *obj = *pObj;
if (obj == NULL) return;
hprdPrimaryRefFree(&obj->hprdPrimaryRef);
hprdSecondaryRefFreeList(&obj->hprdSecondaryRef);
freez(pObj);
}

void hprdXrefFreeList(struct hprdXref **pList)
/* Free up list of hprdXref. */
{
struct hprdXref *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdXrefFree(&el);
    el = next;
    }
}

void hprdXrefSave(struct hprdXref *obj, int indent, FILE *f)
/* Save hprdXref to file. */
{
struct hprdSecondaryRef *hprdSecondaryRef;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<xref>");
fprintf(f, "\n");
hprdPrimaryRefSave(obj->hprdPrimaryRef, indent+2, f);
for (hprdSecondaryRef = obj->hprdSecondaryRef; hprdSecondaryRef != NULL; hprdSecondaryRef = hprdSecondaryRef->next)
   {
   hprdSecondaryRefSave(hprdSecondaryRef, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</xref>\n");
}

struct hprdXref *hprdXrefLoad(char *fileName)
/* Load hprdXref from XML file where it is root element. */
{
struct hprdXref *obj;
xapParseAny(fileName, "xref", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdXref *hprdXrefLoadNext(struct xap *xap)
/* Load next hprdXref element.  Use xapOpen to get xap. */
{
return xapNext(xap, "xref");
}

void hprdPrimaryRefFree(struct hprdPrimaryRef **pObj)
/* Free up hprdPrimaryRef. */
{
struct hprdPrimaryRef *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->db);
freeMem(obj->id);
freeMem(obj->dbAc);
freeMem(obj->refType);
freeMem(obj->refTypeAc);
freez(pObj);
}

void hprdPrimaryRefFreeList(struct hprdPrimaryRef **pList)
/* Free up list of hprdPrimaryRef. */
{
struct hprdPrimaryRef *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdPrimaryRefFree(&el);
    el = next;
    }
}

void hprdPrimaryRefSave(struct hprdPrimaryRef *obj, int indent, FILE *f)
/* Save hprdPrimaryRef to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<primaryRef");
fprintf(f, " db=\"%s\"", obj->db);
fprintf(f, " id=\"%s\"", obj->id);
if (obj->dbAc != NULL)
    fprintf(f, " dbAc=\"%s\"", obj->dbAc);
if (obj->refType != NULL)
    fprintf(f, " refType=\"%s\"", obj->refType);
if (obj->refTypeAc != NULL)
    fprintf(f, " refTypeAc=\"%s\"", obj->refTypeAc);
fprintf(f, "/>\n");
}

struct hprdPrimaryRef *hprdPrimaryRefLoad(char *fileName)
/* Load hprdPrimaryRef from XML file where it is root element. */
{
struct hprdPrimaryRef *obj;
xapParseAny(fileName, "primaryRef", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdPrimaryRef *hprdPrimaryRefLoadNext(struct xap *xap)
/* Load next hprdPrimaryRef element.  Use xapOpen to get xap. */
{
return xapNext(xap, "primaryRef");
}

void hprdSecondaryRefFree(struct hprdSecondaryRef **pObj)
/* Free up hprdSecondaryRef. */
{
struct hprdSecondaryRef *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->db);
freeMem(obj->id);
freeMem(obj->dbAc);
freeMem(obj->refType);
freeMem(obj->refTypeAc);
freez(pObj);
}

void hprdSecondaryRefFreeList(struct hprdSecondaryRef **pList)
/* Free up list of hprdSecondaryRef. */
{
struct hprdSecondaryRef *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdSecondaryRefFree(&el);
    el = next;
    }
}

void hprdSecondaryRefSave(struct hprdSecondaryRef *obj, int indent, FILE *f)
/* Save hprdSecondaryRef to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<secondaryRef");
fprintf(f, " db=\"%s\"", obj->db);
fprintf(f, " id=\"%s\"", obj->id);
if (obj->dbAc != NULL)
    fprintf(f, " dbAc=\"%s\"", obj->dbAc);
if (obj->refType != NULL)
    fprintf(f, " refType=\"%s\"", obj->refType);
if (obj->refTypeAc != NULL)
    fprintf(f, " refTypeAc=\"%s\"", obj->refTypeAc);
fprintf(f, "/>\n");
}

struct hprdSecondaryRef *hprdSecondaryRefLoad(char *fileName)
/* Load hprdSecondaryRef from XML file where it is root element. */
{
struct hprdSecondaryRef *obj;
xapParseAny(fileName, "secondaryRef", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdSecondaryRef *hprdSecondaryRefLoadNext(struct xap *xap)
/* Load next hprdSecondaryRef element.  Use xapOpen to get xap. */
{
return xapNext(xap, "secondaryRef");
}

void hprdAvailabilityListFree(struct hprdAvailabilityList **pObj)
/* Free up hprdAvailabilityList. */
{
struct hprdAvailabilityList *obj = *pObj;
if (obj == NULL) return;
hprdAvailabilityFree(&obj->hprdAvailability);
freez(pObj);
}

void hprdAvailabilityListFreeList(struct hprdAvailabilityList **pList)
/* Free up list of hprdAvailabilityList. */
{
struct hprdAvailabilityList *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdAvailabilityListFree(&el);
    el = next;
    }
}

void hprdAvailabilityListSave(struct hprdAvailabilityList *obj, int indent, FILE *f)
/* Save hprdAvailabilityList to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<availabilityList>");
fprintf(f, "\n");
hprdAvailabilitySave(obj->hprdAvailability, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</availabilityList>\n");
}

struct hprdAvailabilityList *hprdAvailabilityListLoad(char *fileName)
/* Load hprdAvailabilityList from XML file where it is root element. */
{
struct hprdAvailabilityList *obj;
xapParseAny(fileName, "availabilityList", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdAvailabilityList *hprdAvailabilityListLoadNext(struct xap *xap)
/* Load next hprdAvailabilityList element.  Use xapOpen to get xap. */
{
return xapNext(xap, "availabilityList");
}

void hprdAvailabilityFree(struct hprdAvailability **pObj)
/* Free up hprdAvailability. */
{
struct hprdAvailability *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
freeMem(obj->text);
freez(pObj);
}

void hprdAvailabilityFreeList(struct hprdAvailability **pList)
/* Free up list of hprdAvailability. */
{
struct hprdAvailability *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdAvailabilityFree(&el);
    el = next;
    }
}

void hprdAvailabilitySave(struct hprdAvailability *obj, int indent, FILE *f)
/* Save hprdAvailability to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<availability");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
fprintf(f, "%s", obj->text);
fprintf(f, "</availability>\n");
}

struct hprdAvailability *hprdAvailabilityLoad(char *fileName)
/* Load hprdAvailability from XML file where it is root element. */
{
struct hprdAvailability *obj;
xapParseAny(fileName, "availability", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdAvailability *hprdAvailabilityLoadNext(struct xap *xap)
/* Load next hprdAvailability element.  Use xapOpen to get xap. */
{
return xapNext(xap, "availability");
}

void hprdExperimentListFree(struct hprdExperimentList **pObj)
/* Free up hprdExperimentList. */
{
struct hprdExperimentList *obj = *pObj;
if (obj == NULL) return;
hprdExperimentDescriptionFreeList(&obj->hprdExperimentDescription);
hprdExperimentRefFreeList(&obj->hprdExperimentRef);
freez(pObj);
}

void hprdExperimentListFreeList(struct hprdExperimentList **pList)
/* Free up list of hprdExperimentList. */
{
struct hprdExperimentList *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdExperimentListFree(&el);
    el = next;
    }
}

void hprdExperimentListSave(struct hprdExperimentList *obj, int indent, FILE *f)
/* Save hprdExperimentList to file. */
{
struct hprdExperimentDescription *hprdExperimentDescription;
struct hprdExperimentRef *hprdExperimentRef;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<experimentList>");
for (hprdExperimentDescription = obj->hprdExperimentDescription; hprdExperimentDescription != NULL; hprdExperimentDescription = hprdExperimentDescription->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   hprdExperimentDescriptionSave(hprdExperimentDescription, indent+2, f);
   }
for (hprdExperimentRef = obj->hprdExperimentRef; hprdExperimentRef != NULL; hprdExperimentRef = hprdExperimentRef->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   hprdExperimentRefSave(hprdExperimentRef, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</experimentList>\n");
}

struct hprdExperimentList *hprdExperimentListLoad(char *fileName)
/* Load hprdExperimentList from XML file where it is root element. */
{
struct hprdExperimentList *obj;
xapParseAny(fileName, "experimentList", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdExperimentList *hprdExperimentListLoadNext(struct xap *xap)
/* Load next hprdExperimentList element.  Use xapOpen to get xap. */
{
return xapNext(xap, "experimentList");
}

void hprdExperimentDescriptionFree(struct hprdExperimentDescription **pObj)
/* Free up hprdExperimentDescription. */
{
struct hprdExperimentDescription *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
hprdBibrefFree(&obj->hprdBibref);
hprdInteractionDetectionMethodFree(&obj->hprdInteractionDetectionMethod);
freez(pObj);
}

void hprdExperimentDescriptionFreeList(struct hprdExperimentDescription **pList)
/* Free up list of hprdExperimentDescription. */
{
struct hprdExperimentDescription *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdExperimentDescriptionFree(&el);
    el = next;
    }
}

void hprdExperimentDescriptionSave(struct hprdExperimentDescription *obj, int indent, FILE *f)
/* Save hprdExperimentDescription to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<experimentDescription");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
fprintf(f, "\n");
hprdBibrefSave(obj->hprdBibref, indent+2, f);
hprdInteractionDetectionMethodSave(obj->hprdInteractionDetectionMethod, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</experimentDescription>\n");
}

struct hprdExperimentDescription *hprdExperimentDescriptionLoad(char *fileName)
/* Load hprdExperimentDescription from XML file where it is root element. */
{
struct hprdExperimentDescription *obj;
xapParseAny(fileName, "experimentDescription", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdExperimentDescription *hprdExperimentDescriptionLoadNext(struct xap *xap)
/* Load next hprdExperimentDescription element.  Use xapOpen to get xap. */
{
return xapNext(xap, "experimentDescription");
}

void hprdInteractionDetectionMethodFree(struct hprdInteractionDetectionMethod **pObj)
/* Free up hprdInteractionDetectionMethod. */
{
struct hprdInteractionDetectionMethod *obj = *pObj;
if (obj == NULL) return;
hprdNamesFree(&obj->hprdNames);
hprdXrefFree(&obj->hprdXref);
freez(pObj);
}

void hprdInteractionDetectionMethodFreeList(struct hprdInteractionDetectionMethod **pList)
/* Free up list of hprdInteractionDetectionMethod. */
{
struct hprdInteractionDetectionMethod *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdInteractionDetectionMethodFree(&el);
    el = next;
    }
}

void hprdInteractionDetectionMethodSave(struct hprdInteractionDetectionMethod *obj, int indent, FILE *f)
/* Save hprdInteractionDetectionMethod to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<interactionDetectionMethod>");
fprintf(f, "\n");
hprdNamesSave(obj->hprdNames, indent+2, f);
hprdXrefSave(obj->hprdXref, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</interactionDetectionMethod>\n");
}

struct hprdInteractionDetectionMethod *hprdInteractionDetectionMethodLoad(char *fileName)
/* Load hprdInteractionDetectionMethod from XML file where it is root element. */
{
struct hprdInteractionDetectionMethod *obj;
xapParseAny(fileName, "interactionDetectionMethod", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdInteractionDetectionMethod *hprdInteractionDetectionMethodLoadNext(struct xap *xap)
/* Load next hprdInteractionDetectionMethod element.  Use xapOpen to get xap. */
{
return xapNext(xap, "interactionDetectionMethod");
}

void hprdExperimentRefFree(struct hprdExperimentRef **pObj)
/* Free up hprdExperimentRef. */
{
struct hprdExperimentRef *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void hprdExperimentRefFreeList(struct hprdExperimentRef **pList)
/* Free up list of hprdExperimentRef. */
{
struct hprdExperimentRef *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdExperimentRefFree(&el);
    el = next;
    }
}

void hprdExperimentRefSave(struct hprdExperimentRef *obj, int indent, FILE *f)
/* Save hprdExperimentRef to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<experimentRef>");
fprintf(f, "%s", obj->text);
fprintf(f, "</experimentRef>\n");
}

struct hprdExperimentRef *hprdExperimentRefLoad(char *fileName)
/* Load hprdExperimentRef from XML file where it is root element. */
{
struct hprdExperimentRef *obj;
xapParseAny(fileName, "experimentRef", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdExperimentRef *hprdExperimentRefLoadNext(struct xap *xap)
/* Load next hprdExperimentRef element.  Use xapOpen to get xap. */
{
return xapNext(xap, "experimentRef");
}

void hprdInteractorListFree(struct hprdInteractorList **pObj)
/* Free up hprdInteractorList. */
{
struct hprdInteractorList *obj = *pObj;
if (obj == NULL) return;
hprdInteractorFreeList(&obj->hprdInteractor);
freez(pObj);
}

void hprdInteractorListFreeList(struct hprdInteractorList **pList)
/* Free up list of hprdInteractorList. */
{
struct hprdInteractorList *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdInteractorListFree(&el);
    el = next;
    }
}

void hprdInteractorListSave(struct hprdInteractorList *obj, int indent, FILE *f)
/* Save hprdInteractorList to file. */
{
struct hprdInteractor *hprdInteractor;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<interactorList>");
fprintf(f, "\n");
for (hprdInteractor = obj->hprdInteractor; hprdInteractor != NULL; hprdInteractor = hprdInteractor->next)
   {
   hprdInteractorSave(hprdInteractor, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</interactorList>\n");
}

struct hprdInteractorList *hprdInteractorListLoad(char *fileName)
/* Load hprdInteractorList from XML file where it is root element. */
{
struct hprdInteractorList *obj;
xapParseAny(fileName, "interactorList", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdInteractorList *hprdInteractorListLoadNext(struct xap *xap)
/* Load next hprdInteractorList element.  Use xapOpen to get xap. */
{
return xapNext(xap, "interactorList");
}

void hprdInteractorFree(struct hprdInteractor **pObj)
/* Free up hprdInteractor. */
{
struct hprdInteractor *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
hprdNamesFree(&obj->hprdNames);
hprdXrefFree(&obj->hprdXref);
hprdInteractorTypeFree(&obj->hprdInteractorType);
hprdOrganismFree(&obj->hprdOrganism);
hprdSequenceFree(&obj->hprdSequence);
freez(pObj);
}

void hprdInteractorFreeList(struct hprdInteractor **pList)
/* Free up list of hprdInteractor. */
{
struct hprdInteractor *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdInteractorFree(&el);
    el = next;
    }
}

void hprdInteractorSave(struct hprdInteractor *obj, int indent, FILE *f)
/* Save hprdInteractor to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<interactor");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
fprintf(f, "\n");
hprdNamesSave(obj->hprdNames, indent+2, f);
hprdXrefSave(obj->hprdXref, indent+2, f);
hprdInteractorTypeSave(obj->hprdInteractorType, indent+2, f);
hprdOrganismSave(obj->hprdOrganism, indent+2, f);
hprdSequenceSave(obj->hprdSequence, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</interactor>\n");
}

struct hprdInteractor *hprdInteractorLoad(char *fileName)
/* Load hprdInteractor from XML file where it is root element. */
{
struct hprdInteractor *obj;
xapParseAny(fileName, "interactor", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdInteractor *hprdInteractorLoadNext(struct xap *xap)
/* Load next hprdInteractor element.  Use xapOpen to get xap. */
{
return xapNext(xap, "interactor");
}

void hprdInteractorTypeFree(struct hprdInteractorType **pObj)
/* Free up hprdInteractorType. */
{
struct hprdInteractorType *obj = *pObj;
if (obj == NULL) return;
hprdNamesFree(&obj->hprdNames);
hprdXrefFree(&obj->hprdXref);
freez(pObj);
}

void hprdInteractorTypeFreeList(struct hprdInteractorType **pList)
/* Free up list of hprdInteractorType. */
{
struct hprdInteractorType *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdInteractorTypeFree(&el);
    el = next;
    }
}

void hprdInteractorTypeSave(struct hprdInteractorType *obj, int indent, FILE *f)
/* Save hprdInteractorType to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<interactorType>");
fprintf(f, "\n");
hprdNamesSave(obj->hprdNames, indent+2, f);
hprdXrefSave(obj->hprdXref, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</interactorType>\n");
}

struct hprdInteractorType *hprdInteractorTypeLoad(char *fileName)
/* Load hprdInteractorType from XML file where it is root element. */
{
struct hprdInteractorType *obj;
xapParseAny(fileName, "interactorType", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdInteractorType *hprdInteractorTypeLoadNext(struct xap *xap)
/* Load next hprdInteractorType element.  Use xapOpen to get xap. */
{
return xapNext(xap, "interactorType");
}

void hprdOrganismFree(struct hprdOrganism **pObj)
/* Free up hprdOrganism. */
{
struct hprdOrganism *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->ncbiTaxId);
hprdNamesFree(&obj->hprdNames);
freez(pObj);
}

void hprdOrganismFreeList(struct hprdOrganism **pList)
/* Free up list of hprdOrganism. */
{
struct hprdOrganism *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdOrganismFree(&el);
    el = next;
    }
}

void hprdOrganismSave(struct hprdOrganism *obj, int indent, FILE *f)
/* Save hprdOrganism to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<organism");
fprintf(f, " ncbiTaxId=\"%s\"", obj->ncbiTaxId);
fprintf(f, ">");
fprintf(f, "\n");
hprdNamesSave(obj->hprdNames, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</organism>\n");
}

struct hprdOrganism *hprdOrganismLoad(char *fileName)
/* Load hprdOrganism from XML file where it is root element. */
{
struct hprdOrganism *obj;
xapParseAny(fileName, "organism", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdOrganism *hprdOrganismLoadNext(struct xap *xap)
/* Load next hprdOrganism element.  Use xapOpen to get xap. */
{
return xapNext(xap, "organism");
}

void hprdSequenceFree(struct hprdSequence **pObj)
/* Free up hprdSequence. */
{
struct hprdSequence *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void hprdSequenceFreeList(struct hprdSequence **pList)
/* Free up list of hprdSequence. */
{
struct hprdSequence *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdSequenceFree(&el);
    el = next;
    }
}

void hprdSequenceSave(struct hprdSequence *obj, int indent, FILE *f)
/* Save hprdSequence to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<sequence>");
fprintf(f, "%s", obj->text);
fprintf(f, "</sequence>\n");
}

struct hprdSequence *hprdSequenceLoad(char *fileName)
/* Load hprdSequence from XML file where it is root element. */
{
struct hprdSequence *obj;
xapParseAny(fileName, "sequence", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdSequence *hprdSequenceLoadNext(struct xap *xap)
/* Load next hprdSequence element.  Use xapOpen to get xap. */
{
return xapNext(xap, "sequence");
}

void hprdInteractionListFree(struct hprdInteractionList **pObj)
/* Free up hprdInteractionList. */
{
struct hprdInteractionList *obj = *pObj;
if (obj == NULL) return;
hprdInteractionFreeList(&obj->hprdInteraction);
freez(pObj);
}

void hprdInteractionListFreeList(struct hprdInteractionList **pList)
/* Free up list of hprdInteractionList. */
{
struct hprdInteractionList *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdInteractionListFree(&el);
    el = next;
    }
}

void hprdInteractionListSave(struct hprdInteractionList *obj, int indent, FILE *f)
/* Save hprdInteractionList to file. */
{
struct hprdInteraction *hprdInteraction;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<interactionList>");
fprintf(f, "\n");
for (hprdInteraction = obj->hprdInteraction; hprdInteraction != NULL; hprdInteraction = hprdInteraction->next)
   {
   hprdInteractionSave(hprdInteraction, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</interactionList>\n");
}

struct hprdInteractionList *hprdInteractionListLoad(char *fileName)
/* Load hprdInteractionList from XML file where it is root element. */
{
struct hprdInteractionList *obj;
xapParseAny(fileName, "interactionList", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdInteractionList *hprdInteractionListLoadNext(struct xap *xap)
/* Load next hprdInteractionList element.  Use xapOpen to get xap. */
{
return xapNext(xap, "interactionList");
}

void hprdInteractionFree(struct hprdInteraction **pObj)
/* Free up hprdInteraction. */
{
struct hprdInteraction *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
hprdExperimentListFree(&obj->hprdExperimentList);
hprdParticipantListFree(&obj->hprdParticipantList);
freez(pObj);
}

void hprdInteractionFreeList(struct hprdInteraction **pList)
/* Free up list of hprdInteraction. */
{
struct hprdInteraction *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdInteractionFree(&el);
    el = next;
    }
}

void hprdInteractionSave(struct hprdInteraction *obj, int indent, FILE *f)
/* Save hprdInteraction to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<interaction");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
fprintf(f, "\n");
hprdExperimentListSave(obj->hprdExperimentList, indent+2, f);
hprdParticipantListSave(obj->hprdParticipantList, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</interaction>\n");
}

struct hprdInteraction *hprdInteractionLoad(char *fileName)
/* Load hprdInteraction from XML file where it is root element. */
{
struct hprdInteraction *obj;
xapParseAny(fileName, "interaction", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdInteraction *hprdInteractionLoadNext(struct xap *xap)
/* Load next hprdInteraction element.  Use xapOpen to get xap. */
{
return xapNext(xap, "interaction");
}

void hprdParticipantListFree(struct hprdParticipantList **pObj)
/* Free up hprdParticipantList. */
{
struct hprdParticipantList *obj = *pObj;
if (obj == NULL) return;
hprdParticipantFreeList(&obj->hprdParticipant);
freez(pObj);
}

void hprdParticipantListFreeList(struct hprdParticipantList **pList)
/* Free up list of hprdParticipantList. */
{
struct hprdParticipantList *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdParticipantListFree(&el);
    el = next;
    }
}

void hprdParticipantListSave(struct hprdParticipantList *obj, int indent, FILE *f)
/* Save hprdParticipantList to file. */
{
struct hprdParticipant *hprdParticipant;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<participantList>");
fprintf(f, "\n");
for (hprdParticipant = obj->hprdParticipant; hprdParticipant != NULL; hprdParticipant = hprdParticipant->next)
   {
   hprdParticipantSave(hprdParticipant, indent+2, f);
   }
xapIndent(indent, f);
fprintf(f, "</participantList>\n");
}

struct hprdParticipantList *hprdParticipantListLoad(char *fileName)
/* Load hprdParticipantList from XML file where it is root element. */
{
struct hprdParticipantList *obj;
xapParseAny(fileName, "participantList", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdParticipantList *hprdParticipantListLoadNext(struct xap *xap)
/* Load next hprdParticipantList element.  Use xapOpen to get xap. */
{
return xapNext(xap, "participantList");
}

void hprdParticipantFree(struct hprdParticipant **pObj)
/* Free up hprdParticipant. */
{
struct hprdParticipant *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->id);
hprdInteractorRefFree(&obj->hprdInteractorRef);
freez(pObj);
}

void hprdParticipantFreeList(struct hprdParticipant **pList)
/* Free up list of hprdParticipant. */
{
struct hprdParticipant *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdParticipantFree(&el);
    el = next;
    }
}

void hprdParticipantSave(struct hprdParticipant *obj, int indent, FILE *f)
/* Save hprdParticipant to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<participant");
fprintf(f, " id=\"%s\"", obj->id);
fprintf(f, ">");
fprintf(f, "\n");
hprdInteractorRefSave(obj->hprdInteractorRef, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</participant>\n");
}

struct hprdParticipant *hprdParticipantLoad(char *fileName)
/* Load hprdParticipant from XML file where it is root element. */
{
struct hprdParticipant *obj;
xapParseAny(fileName, "participant", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdParticipant *hprdParticipantLoadNext(struct xap *xap)
/* Load next hprdParticipant element.  Use xapOpen to get xap. */
{
return xapNext(xap, "participant");
}

void hprdInteractorRefFree(struct hprdInteractorRef **pObj)
/* Free up hprdInteractorRef. */
{
struct hprdInteractorRef *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void hprdInteractorRefFreeList(struct hprdInteractorRef **pList)
/* Free up list of hprdInteractorRef. */
{
struct hprdInteractorRef *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hprdInteractorRefFree(&el);
    el = next;
    }
}

void hprdInteractorRefSave(struct hprdInteractorRef *obj, int indent, FILE *f)
/* Save hprdInteractorRef to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<interactorRef>");
fprintf(f, "%s", obj->text);
fprintf(f, "</interactorRef>\n");
}

struct hprdInteractorRef *hprdInteractorRefLoad(char *fileName)
/* Load hprdInteractorRef from XML file where it is root element. */
{
struct hprdInteractorRef *obj;
xapParseAny(fileName, "interactorRef", hprdStartHandler, hprdEndHandler, NULL, &obj);
return obj;
}

struct hprdInteractorRef *hprdInteractorRefLoadNext(struct xap *xap)
/* Load next hprdInteractorRef element.  Use xapOpen to get xap. */
{
return xapNext(xap, "interactorRef");
}

void *hprdStartHandler(struct xap *xp, char *name, char **atts)
/* Called by xap with start tag.  Does most of the parsing work. */
{
struct xapStack *st = xp->stack+1;
int depth = xp->stackDepth;
int i;

if (sameString(name, "entrySet"))
    {
    struct hprdEntrySet *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "level"))
            obj->level = cloneString(val);
        else if (sameString(name, "version"))
            obj->version = cloneString(val);
        else if (sameString(name, "xmlns"))
            obj->xmlns = cloneString(val);
        else if (sameString(name, "xmlns:xsi"))
            obj->xmlnsxsi = cloneString(val);
        else if (sameString(name, "xsi:schemaLocation"))
            obj->xsischemaLocation = cloneString(val);
        }
    if (obj->level == NULL)
        xapError(xp, "missing level");
    if (obj->version == NULL)
        xapError(xp, "missing version");
    if (obj->xmlns == NULL)
        xapError(xp, "missing xmlns");
    if (obj->xmlnsxsi == NULL)
        xapError(xp, "missing xmlns:xsi");
    if (obj->xsischemaLocation == NULL)
        xapError(xp, "missing xsi:schemaLocation");
    return obj;
    }
else if (sameString(name, "entry"))
    {
    struct hprdEntry *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "entrySet"))
            {
            struct hprdEntrySet *parent = st->object;
            slAddHead(&parent->hprdEntry, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "source"))
    {
    struct hprdSource *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "entry"))
            {
            struct hprdEntry *parent = st->object;
            slAddHead(&parent->hprdSource, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "names"))
    {
    struct hprdNames *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "source"))
            {
            struct hprdSource *parent = st->object;
            slAddHead(&parent->hprdNames, obj);
            }
        else if (sameString(st->elName, "interactionDetectionMethod"))
            {
            struct hprdInteractionDetectionMethod *parent = st->object;
            slAddHead(&parent->hprdNames, obj);
            }
        else if (sameString(st->elName, "interactor"))
            {
            struct hprdInteractor *parent = st->object;
            slAddHead(&parent->hprdNames, obj);
            }
        else if (sameString(st->elName, "interactorType"))
            {
            struct hprdInteractorType *parent = st->object;
            slAddHead(&parent->hprdNames, obj);
            }
        else if (sameString(st->elName, "organism"))
            {
            struct hprdOrganism *parent = st->object;
            slAddHead(&parent->hprdNames, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "shortLabel"))
    {
    struct hprdShortLabel *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "names"))
            {
            struct hprdNames *parent = st->object;
            slAddHead(&parent->hprdShortLabel, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "fullName"))
    {
    struct hprdFullName *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "names"))
            {
            struct hprdNames *parent = st->object;
            slAddHead(&parent->hprdFullName, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "alias"))
    {
    struct hprdAlias *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "type"))
            obj->type = cloneString(val);
        else if (sameString(name, "typeAc"))
            obj->typeAc = cloneString(val);
        }
    if (obj->type == NULL)
        xapError(xp, "missing type");
    if (obj->typeAc == NULL)
        xapError(xp, "missing typeAc");
    if (depth > 1)
        {
        if  (sameString(st->elName, "names"))
            {
            struct hprdNames *parent = st->object;
            slAddHead(&parent->hprdAlias, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "bibref"))
    {
    struct hprdBibref *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "source"))
            {
            struct hprdSource *parent = st->object;
            slAddHead(&parent->hprdBibref, obj);
            }
        else if (sameString(st->elName, "experimentDescription"))
            {
            struct hprdExperimentDescription *parent = st->object;
            slAddHead(&parent->hprdBibref, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "xref"))
    {
    struct hprdXref *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "bibref"))
            {
            struct hprdBibref *parent = st->object;
            slAddHead(&parent->hprdXref, obj);
            }
        else if (sameString(st->elName, "interactionDetectionMethod"))
            {
            struct hprdInteractionDetectionMethod *parent = st->object;
            slAddHead(&parent->hprdXref, obj);
            }
        else if (sameString(st->elName, "interactor"))
            {
            struct hprdInteractor *parent = st->object;
            slAddHead(&parent->hprdXref, obj);
            }
        else if (sameString(st->elName, "interactorType"))
            {
            struct hprdInteractorType *parent = st->object;
            slAddHead(&parent->hprdXref, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "primaryRef"))
    {
    struct hprdPrimaryRef *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "db"))
            obj->db = cloneString(val);
        else if (sameString(name, "id"))
            obj->id = cloneString(val);
        else if (sameString(name, "dbAc"))
            obj->dbAc = cloneString(val);
        else if (sameString(name, "refType"))
            obj->refType = cloneString(val);
        else if (sameString(name, "refTypeAc"))
            obj->refTypeAc = cloneString(val);
        }
    if (obj->db == NULL)
        xapError(xp, "missing db");
    if (obj->id == NULL)
        xapError(xp, "missing id");
    if (depth > 1)
        {
        if  (sameString(st->elName, "xref"))
            {
            struct hprdXref *parent = st->object;
            slAddHead(&parent->hprdPrimaryRef, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "secondaryRef"))
    {
    struct hprdSecondaryRef *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "db"))
            obj->db = cloneString(val);
        else if (sameString(name, "id"))
            obj->id = cloneString(val);
        else if (sameString(name, "dbAc"))
            obj->dbAc = cloneString(val);
        else if (sameString(name, "refType"))
            obj->refType = cloneString(val);
        else if (sameString(name, "refTypeAc"))
            obj->refTypeAc = cloneString(val);
        }
    if (obj->db == NULL)
        xapError(xp, "missing db");
    if (obj->id == NULL)
        xapError(xp, "missing id");
    if (depth > 1)
        {
        if  (sameString(st->elName, "xref"))
            {
            struct hprdXref *parent = st->object;
            slAddHead(&parent->hprdSecondaryRef, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "availabilityList"))
    {
    struct hprdAvailabilityList *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "entry"))
            {
            struct hprdEntry *parent = st->object;
            slAddHead(&parent->hprdAvailabilityList, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "availability"))
    {
    struct hprdAvailability *obj;
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
        if  (sameString(st->elName, "availabilityList"))
            {
            struct hprdAvailabilityList *parent = st->object;
            slAddHead(&parent->hprdAvailability, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "experimentList"))
    {
    struct hprdExperimentList *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "entry"))
            {
            struct hprdEntry *parent = st->object;
            slAddHead(&parent->hprdExperimentList, obj);
            }
        else if (sameString(st->elName, "interaction"))
            {
            struct hprdInteraction *parent = st->object;
            slAddHead(&parent->hprdExperimentList, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "experimentDescription"))
    {
    struct hprdExperimentDescription *obj;
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
        if  (sameString(st->elName, "experimentList"))
            {
            struct hprdExperimentList *parent = st->object;
            slAddHead(&parent->hprdExperimentDescription, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "interactionDetectionMethod"))
    {
    struct hprdInteractionDetectionMethod *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "experimentDescription"))
            {
            struct hprdExperimentDescription *parent = st->object;
            slAddHead(&parent->hprdInteractionDetectionMethod, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "experimentRef"))
    {
    struct hprdExperimentRef *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "experimentList"))
            {
            struct hprdExperimentList *parent = st->object;
            slAddHead(&parent->hprdExperimentRef, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "interactorList"))
    {
    struct hprdInteractorList *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "entry"))
            {
            struct hprdEntry *parent = st->object;
            slAddHead(&parent->hprdInteractorList, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "interactor"))
    {
    struct hprdInteractor *obj;
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
        if  (sameString(st->elName, "interactorList"))
            {
            struct hprdInteractorList *parent = st->object;
            slAddHead(&parent->hprdInteractor, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "interactorType"))
    {
    struct hprdInteractorType *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "interactor"))
            {
            struct hprdInteractor *parent = st->object;
            slAddHead(&parent->hprdInteractorType, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "organism"))
    {
    struct hprdOrganism *obj;
    AllocVar(obj);
    for (i=0; atts[i] != NULL; i += 2)
        {
        char *name = atts[i], *val = atts[i+1];
        if  (sameString(name, "ncbiTaxId"))
            obj->ncbiTaxId = cloneString(val);
        }
    if (obj->ncbiTaxId == NULL)
        xapError(xp, "missing ncbiTaxId");
    if (depth > 1)
        {
        if  (sameString(st->elName, "interactor"))
            {
            struct hprdInteractor *parent = st->object;
            slAddHead(&parent->hprdOrganism, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "sequence"))
    {
    struct hprdSequence *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "interactor"))
            {
            struct hprdInteractor *parent = st->object;
            slAddHead(&parent->hprdSequence, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "interactionList"))
    {
    struct hprdInteractionList *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "entry"))
            {
            struct hprdEntry *parent = st->object;
            slAddHead(&parent->hprdInteractionList, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "interaction"))
    {
    struct hprdInteraction *obj;
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
        if  (sameString(st->elName, "interactionList"))
            {
            struct hprdInteractionList *parent = st->object;
            slAddHead(&parent->hprdInteraction, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "participantList"))
    {
    struct hprdParticipantList *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "interaction"))
            {
            struct hprdInteraction *parent = st->object;
            slAddHead(&parent->hprdParticipantList, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "participant"))
    {
    struct hprdParticipant *obj;
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
        if  (sameString(st->elName, "participantList"))
            {
            struct hprdParticipantList *parent = st->object;
            slAddHead(&parent->hprdParticipant, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "interactorRef"))
    {
    struct hprdInteractorRef *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "participant"))
            {
            struct hprdParticipant *parent = st->object;
            slAddHead(&parent->hprdInteractorRef, obj);
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

void hprdEndHandler(struct xap *xp, char *name)
/* Called by xap with end tag.  Checks all required children are loaded. */
{
struct xapStack *stack = xp->stack;
if (sameString(name, "entrySet"))
    {
    struct hprdEntrySet *obj = stack->object;
    if (obj->hprdEntry == NULL)
        xapError(xp, "Missing entry");
    if (obj->hprdEntry->next != NULL)
        xapError(xp, "Multiple entry");
    }
else if (sameString(name, "entry"))
    {
    struct hprdEntry *obj = stack->object;
    if (obj->hprdSource == NULL)
        xapError(xp, "Missing source");
    if (obj->hprdSource->next != NULL)
        xapError(xp, "Multiple source");
    if (obj->hprdAvailabilityList == NULL)
        xapError(xp, "Missing availabilityList");
    if (obj->hprdAvailabilityList->next != NULL)
        xapError(xp, "Multiple availabilityList");
    if (obj->hprdExperimentList == NULL)
        xapError(xp, "Missing experimentList");
    if (obj->hprdExperimentList->next != NULL)
        xapError(xp, "Multiple experimentList");
    if (obj->hprdInteractorList == NULL)
        xapError(xp, "Missing interactorList");
    if (obj->hprdInteractorList->next != NULL)
        xapError(xp, "Multiple interactorList");
    if (obj->hprdInteractionList == NULL)
        xapError(xp, "Missing interactionList");
    if (obj->hprdInteractionList->next != NULL)
        xapError(xp, "Multiple interactionList");
    }
else if (sameString(name, "source"))
    {
    struct hprdSource *obj = stack->object;
    if (obj->hprdNames == NULL)
        xapError(xp, "Missing names");
    if (obj->hprdNames->next != NULL)
        xapError(xp, "Multiple names");
    if (obj->hprdBibref == NULL)
        xapError(xp, "Missing bibref");
    if (obj->hprdBibref->next != NULL)
        xapError(xp, "Multiple bibref");
    }
else if (sameString(name, "names"))
    {
    struct hprdNames *obj = stack->object;
    if (obj->hprdShortLabel == NULL)
        xapError(xp, "Missing shortLabel");
    if (obj->hprdShortLabel->next != NULL)
        xapError(xp, "Multiple shortLabel");
    if (obj->hprdFullName != NULL && obj->hprdFullName->next != NULL)
        xapError(xp, "Multiple fullName");
    slReverse(&obj->hprdAlias);
    }
else if (sameString(name, "shortLabel"))
    {
    struct hprdShortLabel *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "fullName"))
    {
    struct hprdFullName *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "alias"))
    {
    struct hprdAlias *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "bibref"))
    {
    struct hprdBibref *obj = stack->object;
    if (obj->hprdXref == NULL)
        xapError(xp, "Missing xref");
    if (obj->hprdXref->next != NULL)
        xapError(xp, "Multiple xref");
    }
else if (sameString(name, "xref"))
    {
    struct hprdXref *obj = stack->object;
    if (obj->hprdPrimaryRef == NULL)
        xapError(xp, "Missing primaryRef");
    if (obj->hprdPrimaryRef->next != NULL)
        xapError(xp, "Multiple primaryRef");
    slReverse(&obj->hprdSecondaryRef);
    }
else if (sameString(name, "availabilityList"))
    {
    struct hprdAvailabilityList *obj = stack->object;
    if (obj->hprdAvailability == NULL)
        xapError(xp, "Missing availability");
    if (obj->hprdAvailability->next != NULL)
        xapError(xp, "Multiple availability");
    }
else if (sameString(name, "availability"))
    {
    struct hprdAvailability *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "experimentList"))
    {
    struct hprdExperimentList *obj = stack->object;
    slReverse(&obj->hprdExperimentDescription);
    slReverse(&obj->hprdExperimentRef);
    }
else if (sameString(name, "experimentDescription"))
    {
    struct hprdExperimentDescription *obj = stack->object;
    if (obj->hprdBibref == NULL)
        xapError(xp, "Missing bibref");
    if (obj->hprdBibref->next != NULL)
        xapError(xp, "Multiple bibref");
    if (obj->hprdInteractionDetectionMethod == NULL)
        xapError(xp, "Missing interactionDetectionMethod");
    if (obj->hprdInteractionDetectionMethod->next != NULL)
        xapError(xp, "Multiple interactionDetectionMethod");
    }
else if (sameString(name, "interactionDetectionMethod"))
    {
    struct hprdInteractionDetectionMethod *obj = stack->object;
    if (obj->hprdNames == NULL)
        xapError(xp, "Missing names");
    if (obj->hprdNames->next != NULL)
        xapError(xp, "Multiple names");
    if (obj->hprdXref == NULL)
        xapError(xp, "Missing xref");
    if (obj->hprdXref->next != NULL)
        xapError(xp, "Multiple xref");
    }
else if (sameString(name, "experimentRef"))
    {
    struct hprdExperimentRef *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "interactorList"))
    {
    struct hprdInteractorList *obj = stack->object;
    if (obj->hprdInteractor == NULL)
        xapError(xp, "Missing interactor");
    slReverse(&obj->hprdInteractor);
    }
else if (sameString(name, "interactor"))
    {
    struct hprdInteractor *obj = stack->object;
    if (obj->hprdNames == NULL)
        xapError(xp, "Missing names");
    if (obj->hprdNames->next != NULL)
        xapError(xp, "Multiple names");
    if (obj->hprdXref == NULL)
        xapError(xp, "Missing xref");
    if (obj->hprdXref->next != NULL)
        xapError(xp, "Multiple xref");
    if (obj->hprdInteractorType == NULL)
        xapError(xp, "Missing interactorType");
    if (obj->hprdInteractorType->next != NULL)
        xapError(xp, "Multiple interactorType");
    if (obj->hprdOrganism == NULL)
        xapError(xp, "Missing organism");
    if (obj->hprdOrganism->next != NULL)
        xapError(xp, "Multiple organism");
    if (obj->hprdSequence == NULL)
        xapError(xp, "Missing sequence");
    if (obj->hprdSequence->next != NULL)
        xapError(xp, "Multiple sequence");
    }
else if (sameString(name, "interactorType"))
    {
    struct hprdInteractorType *obj = stack->object;
    if (obj->hprdNames == NULL)
        xapError(xp, "Missing names");
    if (obj->hprdNames->next != NULL)
        xapError(xp, "Multiple names");
    if (obj->hprdXref == NULL)
        xapError(xp, "Missing xref");
    if (obj->hprdXref->next != NULL)
        xapError(xp, "Multiple xref");
    }
else if (sameString(name, "organism"))
    {
    struct hprdOrganism *obj = stack->object;
    if (obj->hprdNames == NULL)
        xapError(xp, "Missing names");
    if (obj->hprdNames->next != NULL)
        xapError(xp, "Multiple names");
    }
else if (sameString(name, "sequence"))
    {
    struct hprdSequence *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "interactionList"))
    {
    struct hprdInteractionList *obj = stack->object;
    if (obj->hprdInteraction == NULL)
        xapError(xp, "Missing interaction");
    slReverse(&obj->hprdInteraction);
    }
else if (sameString(name, "interaction"))
    {
    struct hprdInteraction *obj = stack->object;
    if (obj->hprdExperimentList == NULL)
        xapError(xp, "Missing experimentList");
    if (obj->hprdExperimentList->next != NULL)
        xapError(xp, "Multiple experimentList");
    if (obj->hprdParticipantList == NULL)
        xapError(xp, "Missing participantList");
    if (obj->hprdParticipantList->next != NULL)
        xapError(xp, "Multiple participantList");
    }
else if (sameString(name, "participantList"))
    {
    struct hprdParticipantList *obj = stack->object;
    if (obj->hprdParticipant == NULL)
        xapError(xp, "Missing participant");
    slReverse(&obj->hprdParticipant);
    }
else if (sameString(name, "participant"))
    {
    struct hprdParticipant *obj = stack->object;
    if (obj->hprdInteractorRef == NULL)
        xapError(xp, "Missing interactorRef");
    if (obj->hprdInteractorRef->next != NULL)
        xapError(xp, "Multiple interactorRef");
    }
else if (sameString(name, "interactorRef"))
    {
    struct hprdInteractorRef *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
}


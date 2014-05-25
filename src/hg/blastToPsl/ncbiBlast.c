/* ncbiBlast.c autoXml generated file */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/**** 
 **** N.B. hand modified to use doubles rather than floats and to convert with sqlNum.
 ****/

#include "common.h"
#include "xap.h"
#include "ncbiBlast.h"
#include "sqlNum.h"


void ncbiBlastBlastOutputFree(struct ncbiBlastBlastOutput **pObj)
/* Free up ncbiBlastBlastOutput. */
{
struct ncbiBlastBlastOutput *obj = *pObj;
if (obj == NULL) return;
ncbiBlastBlastOutputProgramFree(&obj->ncbiBlastBlastOutputProgram);
ncbiBlastBlastOutputVersionFree(&obj->ncbiBlastBlastOutputVersion);
ncbiBlastBlastOutputReferenceFree(&obj->ncbiBlastBlastOutputReference);
ncbiBlastBlastOutputDbFree(&obj->ncbiBlastBlastOutputDb);
ncbiBlastBlastOutputQueryIDFree(&obj->ncbiBlastBlastOutputQueryID);
ncbiBlastBlastOutputQueryDefFree(&obj->ncbiBlastBlastOutputQueryDef);
ncbiBlastBlastOutputQueryLenFree(&obj->ncbiBlastBlastOutputQueryLen);
ncbiBlastBlastOutputQuerySeqFree(&obj->ncbiBlastBlastOutputQuerySeq);
ncbiBlastBlastOutputParamFree(&obj->ncbiBlastBlastOutputParam);
ncbiBlastBlastOutputIterationsFree(&obj->ncbiBlastBlastOutputIterations);
ncbiBlastBlastOutputMbstatFree(&obj->ncbiBlastBlastOutputMbstat);
freez(pObj);
}

void ncbiBlastBlastOutputFreeList(struct ncbiBlastBlastOutput **pList)
/* Free up list of ncbiBlastBlastOutput. */
{
struct ncbiBlastBlastOutput *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputSave(struct ncbiBlastBlastOutput *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutput to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput>");
fprintf(f, "\n");
ncbiBlastBlastOutputProgramSave(obj->ncbiBlastBlastOutputProgram, indent+2, f);
ncbiBlastBlastOutputVersionSave(obj->ncbiBlastBlastOutputVersion, indent+2, f);
ncbiBlastBlastOutputReferenceSave(obj->ncbiBlastBlastOutputReference, indent+2, f);
ncbiBlastBlastOutputDbSave(obj->ncbiBlastBlastOutputDb, indent+2, f);
ncbiBlastBlastOutputQueryIDSave(obj->ncbiBlastBlastOutputQueryID, indent+2, f);
ncbiBlastBlastOutputQueryDefSave(obj->ncbiBlastBlastOutputQueryDef, indent+2, f);
ncbiBlastBlastOutputQueryLenSave(obj->ncbiBlastBlastOutputQueryLen, indent+2, f);
ncbiBlastBlastOutputQuerySeqSave(obj->ncbiBlastBlastOutputQuerySeq, indent+2, f);
ncbiBlastBlastOutputParamSave(obj->ncbiBlastBlastOutputParam, indent+2, f);
ncbiBlastBlastOutputIterationsSave(obj->ncbiBlastBlastOutputIterations, indent+2, f);
ncbiBlastBlastOutputMbstatSave(obj->ncbiBlastBlastOutputMbstat, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</BlastOutput>\n");
}

struct ncbiBlastBlastOutput *ncbiBlastBlastOutputLoad(char *fileName)
/* Load ncbiBlastBlastOutput from XML file where it is root element. */
{
struct ncbiBlastBlastOutput *obj;
xapParseAny(fileName, "BlastOutput", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutput *ncbiBlastBlastOutputLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutput element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput");
}

void ncbiBlastBlastOutputProgramFree(struct ncbiBlastBlastOutputProgram **pObj)
/* Free up ncbiBlastBlastOutputProgram. */
{
struct ncbiBlastBlastOutputProgram *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastBlastOutputProgramFreeList(struct ncbiBlastBlastOutputProgram **pList)
/* Free up list of ncbiBlastBlastOutputProgram. */
{
struct ncbiBlastBlastOutputProgram *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputProgramFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputProgramSave(struct ncbiBlastBlastOutputProgram *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputProgram to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_program>");
fprintf(f, "%s", obj->text);
fprintf(f, "</BlastOutput_program>\n");
}

struct ncbiBlastBlastOutputProgram *ncbiBlastBlastOutputProgramLoad(char *fileName)
/* Load ncbiBlastBlastOutputProgram from XML file where it is root element. */
{
struct ncbiBlastBlastOutputProgram *obj;
xapParseAny(fileName, "BlastOutput_program", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputProgram *ncbiBlastBlastOutputProgramLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputProgram element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_program");
}

void ncbiBlastBlastOutputVersionFree(struct ncbiBlastBlastOutputVersion **pObj)
/* Free up ncbiBlastBlastOutputVersion. */
{
struct ncbiBlastBlastOutputVersion *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastBlastOutputVersionFreeList(struct ncbiBlastBlastOutputVersion **pList)
/* Free up list of ncbiBlastBlastOutputVersion. */
{
struct ncbiBlastBlastOutputVersion *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputVersionFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputVersionSave(struct ncbiBlastBlastOutputVersion *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputVersion to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_version>");
fprintf(f, "%s", obj->text);
fprintf(f, "</BlastOutput_version>\n");
}

struct ncbiBlastBlastOutputVersion *ncbiBlastBlastOutputVersionLoad(char *fileName)
/* Load ncbiBlastBlastOutputVersion from XML file where it is root element. */
{
struct ncbiBlastBlastOutputVersion *obj;
xapParseAny(fileName, "BlastOutput_version", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputVersion *ncbiBlastBlastOutputVersionLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputVersion element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_version");
}

void ncbiBlastBlastOutputReferenceFree(struct ncbiBlastBlastOutputReference **pObj)
/* Free up ncbiBlastBlastOutputReference. */
{
struct ncbiBlastBlastOutputReference *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastBlastOutputReferenceFreeList(struct ncbiBlastBlastOutputReference **pList)
/* Free up list of ncbiBlastBlastOutputReference. */
{
struct ncbiBlastBlastOutputReference *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputReferenceFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputReferenceSave(struct ncbiBlastBlastOutputReference *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputReference to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_reference>");
fprintf(f, "%s", obj->text);
fprintf(f, "</BlastOutput_reference>\n");
}

struct ncbiBlastBlastOutputReference *ncbiBlastBlastOutputReferenceLoad(char *fileName)
/* Load ncbiBlastBlastOutputReference from XML file where it is root element. */
{
struct ncbiBlastBlastOutputReference *obj;
xapParseAny(fileName, "BlastOutput_reference", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputReference *ncbiBlastBlastOutputReferenceLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputReference element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_reference");
}

void ncbiBlastBlastOutputDbFree(struct ncbiBlastBlastOutputDb **pObj)
/* Free up ncbiBlastBlastOutputDb. */
{
struct ncbiBlastBlastOutputDb *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastBlastOutputDbFreeList(struct ncbiBlastBlastOutputDb **pList)
/* Free up list of ncbiBlastBlastOutputDb. */
{
struct ncbiBlastBlastOutputDb *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputDbFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputDbSave(struct ncbiBlastBlastOutputDb *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputDb to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_db>");
fprintf(f, "%s", obj->text);
fprintf(f, "</BlastOutput_db>\n");
}

struct ncbiBlastBlastOutputDb *ncbiBlastBlastOutputDbLoad(char *fileName)
/* Load ncbiBlastBlastOutputDb from XML file where it is root element. */
{
struct ncbiBlastBlastOutputDb *obj;
xapParseAny(fileName, "BlastOutput_db", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputDb *ncbiBlastBlastOutputDbLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputDb element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_db");
}

void ncbiBlastBlastOutputQueryIDFree(struct ncbiBlastBlastOutputQueryID **pObj)
/* Free up ncbiBlastBlastOutputQueryID. */
{
struct ncbiBlastBlastOutputQueryID *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastBlastOutputQueryIDFreeList(struct ncbiBlastBlastOutputQueryID **pList)
/* Free up list of ncbiBlastBlastOutputQueryID. */
{
struct ncbiBlastBlastOutputQueryID *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputQueryIDFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputQueryIDSave(struct ncbiBlastBlastOutputQueryID *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputQueryID to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_query-ID>");
fprintf(f, "%s", obj->text);
fprintf(f, "</BlastOutput_query-ID>\n");
}

struct ncbiBlastBlastOutputQueryID *ncbiBlastBlastOutputQueryIDLoad(char *fileName)
/* Load ncbiBlastBlastOutputQueryID from XML file where it is root element. */
{
struct ncbiBlastBlastOutputQueryID *obj;
xapParseAny(fileName, "BlastOutput_query-ID", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputQueryID *ncbiBlastBlastOutputQueryIDLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputQueryID element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_query-ID");
}

void ncbiBlastBlastOutputQueryDefFree(struct ncbiBlastBlastOutputQueryDef **pObj)
/* Free up ncbiBlastBlastOutputQueryDef. */
{
struct ncbiBlastBlastOutputQueryDef *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastBlastOutputQueryDefFreeList(struct ncbiBlastBlastOutputQueryDef **pList)
/* Free up list of ncbiBlastBlastOutputQueryDef. */
{
struct ncbiBlastBlastOutputQueryDef *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputQueryDefFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputQueryDefSave(struct ncbiBlastBlastOutputQueryDef *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputQueryDef to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_query-def>");
fprintf(f, "%s", obj->text);
fprintf(f, "</BlastOutput_query-def>\n");
}

struct ncbiBlastBlastOutputQueryDef *ncbiBlastBlastOutputQueryDefLoad(char *fileName)
/* Load ncbiBlastBlastOutputQueryDef from XML file where it is root element. */
{
struct ncbiBlastBlastOutputQueryDef *obj;
xapParseAny(fileName, "BlastOutput_query-def", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputQueryDef *ncbiBlastBlastOutputQueryDefLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputQueryDef element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_query-def");
}

void ncbiBlastBlastOutputQueryLenFree(struct ncbiBlastBlastOutputQueryLen **pObj)
/* Free up ncbiBlastBlastOutputQueryLen. */
{
struct ncbiBlastBlastOutputQueryLen *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastBlastOutputQueryLenFreeList(struct ncbiBlastBlastOutputQueryLen **pList)
/* Free up list of ncbiBlastBlastOutputQueryLen. */
{
struct ncbiBlastBlastOutputQueryLen *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputQueryLenFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputQueryLenSave(struct ncbiBlastBlastOutputQueryLen *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputQueryLen to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_query-len>");
fprintf(f, "%d", obj->text);
fprintf(f, "</BlastOutput_query-len>\n");
}

struct ncbiBlastBlastOutputQueryLen *ncbiBlastBlastOutputQueryLenLoad(char *fileName)
/* Load ncbiBlastBlastOutputQueryLen from XML file where it is root element. */
{
struct ncbiBlastBlastOutputQueryLen *obj;
xapParseAny(fileName, "BlastOutput_query-len", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputQueryLen *ncbiBlastBlastOutputQueryLenLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputQueryLen element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_query-len");
}

void ncbiBlastBlastOutputQuerySeqFree(struct ncbiBlastBlastOutputQuerySeq **pObj)
/* Free up ncbiBlastBlastOutputQuerySeq. */
{
struct ncbiBlastBlastOutputQuerySeq *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastBlastOutputQuerySeqFreeList(struct ncbiBlastBlastOutputQuerySeq **pList)
/* Free up list of ncbiBlastBlastOutputQuerySeq. */
{
struct ncbiBlastBlastOutputQuerySeq *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputQuerySeqFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputQuerySeqSave(struct ncbiBlastBlastOutputQuerySeq *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputQuerySeq to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_query-seq>");
fprintf(f, "%s", obj->text);
fprintf(f, "</BlastOutput_query-seq>\n");
}

struct ncbiBlastBlastOutputQuerySeq *ncbiBlastBlastOutputQuerySeqLoad(char *fileName)
/* Load ncbiBlastBlastOutputQuerySeq from XML file where it is root element. */
{
struct ncbiBlastBlastOutputQuerySeq *obj;
xapParseAny(fileName, "BlastOutput_query-seq", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputQuerySeq *ncbiBlastBlastOutputQuerySeqLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputQuerySeq element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_query-seq");
}

void ncbiBlastBlastOutputParamFree(struct ncbiBlastBlastOutputParam **pObj)
/* Free up ncbiBlastBlastOutputParam. */
{
struct ncbiBlastBlastOutputParam *obj = *pObj;
if (obj == NULL) return;
ncbiBlastParametersFree(&obj->ncbiBlastParameters);
freez(pObj);
}

void ncbiBlastBlastOutputParamFreeList(struct ncbiBlastBlastOutputParam **pList)
/* Free up list of ncbiBlastBlastOutputParam. */
{
struct ncbiBlastBlastOutputParam *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputParamFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputParamSave(struct ncbiBlastBlastOutputParam *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputParam to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_param>");
fprintf(f, "\n");
ncbiBlastParametersSave(obj->ncbiBlastParameters, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</BlastOutput_param>\n");
}

struct ncbiBlastBlastOutputParam *ncbiBlastBlastOutputParamLoad(char *fileName)
/* Load ncbiBlastBlastOutputParam from XML file where it is root element. */
{
struct ncbiBlastBlastOutputParam *obj;
xapParseAny(fileName, "BlastOutput_param", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputParam *ncbiBlastBlastOutputParamLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputParam element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_param");
}

void ncbiBlastBlastOutputIterationsFree(struct ncbiBlastBlastOutputIterations **pObj)
/* Free up ncbiBlastBlastOutputIterations. */
{
struct ncbiBlastBlastOutputIterations *obj = *pObj;
if (obj == NULL) return;
ncbiBlastIterationFreeList(&obj->ncbiBlastIteration);
freez(pObj);
}

void ncbiBlastBlastOutputIterationsFreeList(struct ncbiBlastBlastOutputIterations **pList)
/* Free up list of ncbiBlastBlastOutputIterations. */
{
struct ncbiBlastBlastOutputIterations *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputIterationsFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputIterationsSave(struct ncbiBlastBlastOutputIterations *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputIterations to file. */
{
struct ncbiBlastIteration *ncbiBlastIteration;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_iterations>");
for (ncbiBlastIteration = obj->ncbiBlastIteration; ncbiBlastIteration != NULL; ncbiBlastIteration = ncbiBlastIteration->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   ncbiBlastIterationSave(ncbiBlastIteration, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</BlastOutput_iterations>\n");
}

struct ncbiBlastBlastOutputIterations *ncbiBlastBlastOutputIterationsLoad(char *fileName)
/* Load ncbiBlastBlastOutputIterations from XML file where it is root element. */
{
struct ncbiBlastBlastOutputIterations *obj;
xapParseAny(fileName, "BlastOutput_iterations", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputIterations *ncbiBlastBlastOutputIterationsLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputIterations element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_iterations");
}

void ncbiBlastBlastOutputMbstatFree(struct ncbiBlastBlastOutputMbstat **pObj)
/* Free up ncbiBlastBlastOutputMbstat. */
{
struct ncbiBlastBlastOutputMbstat *obj = *pObj;
if (obj == NULL) return;
ncbiBlastStatisticsFree(&obj->ncbiBlastStatistics);
freez(pObj);
}

void ncbiBlastBlastOutputMbstatFreeList(struct ncbiBlastBlastOutputMbstat **pList)
/* Free up list of ncbiBlastBlastOutputMbstat. */
{
struct ncbiBlastBlastOutputMbstat *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastBlastOutputMbstatFree(&el);
    el = next;
    }
}

void ncbiBlastBlastOutputMbstatSave(struct ncbiBlastBlastOutputMbstat *obj, int indent, FILE *f)
/* Save ncbiBlastBlastOutputMbstat to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<BlastOutput_mbstat>");
fprintf(f, "\n");
ncbiBlastStatisticsSave(obj->ncbiBlastStatistics, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</BlastOutput_mbstat>\n");
}

struct ncbiBlastBlastOutputMbstat *ncbiBlastBlastOutputMbstatLoad(char *fileName)
/* Load ncbiBlastBlastOutputMbstat from XML file where it is root element. */
{
struct ncbiBlastBlastOutputMbstat *obj;
xapParseAny(fileName, "BlastOutput_mbstat", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastBlastOutputMbstat *ncbiBlastBlastOutputMbstatLoadNext(struct xap *xap)
/* Load next ncbiBlastBlastOutputMbstat element.  Use xapOpen to get xap. */
{
return xapNext(xap, "BlastOutput_mbstat");
}

void ncbiBlastIterationFree(struct ncbiBlastIteration **pObj)
/* Free up ncbiBlastIteration. */
{
struct ncbiBlastIteration *obj = *pObj;
if (obj == NULL) return;
ncbiBlastIterationIterNumFree(&obj->ncbiBlastIterationIterNum);
ncbiBlastIterationQueryIDFree(&obj->ncbiBlastIterationQueryID);
ncbiBlastIterationQueryDefFree(&obj->ncbiBlastIterationQueryDef);
ncbiBlastIterationQueryLenFree(&obj->ncbiBlastIterationQueryLen);
ncbiBlastIterationHitsFree(&obj->ncbiBlastIterationHits);
ncbiBlastIterationStatFree(&obj->ncbiBlastIterationStat);
ncbiBlastIterationMessageFree(&obj->ncbiBlastIterationMessage);
freez(pObj);
}

void ncbiBlastIterationFreeList(struct ncbiBlastIteration **pList)
/* Free up list of ncbiBlastIteration. */
{
struct ncbiBlastIteration *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastIterationFree(&el);
    el = next;
    }
}

void ncbiBlastIterationSave(struct ncbiBlastIteration *obj, int indent, FILE *f)
/* Save ncbiBlastIteration to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Iteration>");
fprintf(f, "\n");
ncbiBlastIterationIterNumSave(obj->ncbiBlastIterationIterNum, indent+2, f);
ncbiBlastIterationQueryIDSave(obj->ncbiBlastIterationQueryID, indent+2, f);
ncbiBlastIterationQueryDefSave(obj->ncbiBlastIterationQueryDef, indent+2, f);
ncbiBlastIterationQueryLenSave(obj->ncbiBlastIterationQueryLen, indent+2, f);
ncbiBlastIterationHitsSave(obj->ncbiBlastIterationHits, indent+2, f);
ncbiBlastIterationStatSave(obj->ncbiBlastIterationStat, indent+2, f);
ncbiBlastIterationMessageSave(obj->ncbiBlastIterationMessage, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</Iteration>\n");
}

struct ncbiBlastIteration *ncbiBlastIterationLoad(char *fileName)
/* Load ncbiBlastIteration from XML file where it is root element. */
{
struct ncbiBlastIteration *obj;
xapParseAny(fileName, "Iteration", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastIteration *ncbiBlastIterationLoadNext(struct xap *xap)
/* Load next ncbiBlastIteration element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Iteration");
}

void ncbiBlastIterationIterNumFree(struct ncbiBlastIterationIterNum **pObj)
/* Free up ncbiBlastIterationIterNum. */
{
struct ncbiBlastIterationIterNum *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastIterationIterNumFreeList(struct ncbiBlastIterationIterNum **pList)
/* Free up list of ncbiBlastIterationIterNum. */
{
struct ncbiBlastIterationIterNum *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastIterationIterNumFree(&el);
    el = next;
    }
}

void ncbiBlastIterationIterNumSave(struct ncbiBlastIterationIterNum *obj, int indent, FILE *f)
/* Save ncbiBlastIterationIterNum to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Iteration_iter-num>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Iteration_iter-num>\n");
}

struct ncbiBlastIterationIterNum *ncbiBlastIterationIterNumLoad(char *fileName)
/* Load ncbiBlastIterationIterNum from XML file where it is root element. */
{
struct ncbiBlastIterationIterNum *obj;
xapParseAny(fileName, "Iteration_iter-num", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastIterationIterNum *ncbiBlastIterationIterNumLoadNext(struct xap *xap)
/* Load next ncbiBlastIterationIterNum element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Iteration_iter-num");
}

void ncbiBlastIterationQueryIDFree(struct ncbiBlastIterationQueryID **pObj)
/* Free up ncbiBlastIterationQueryID. */
{
struct ncbiBlastIterationQueryID *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastIterationQueryIDFreeList(struct ncbiBlastIterationQueryID **pList)
/* Free up list of ncbiBlastIterationQueryID. */
{
struct ncbiBlastIterationQueryID *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastIterationQueryIDFree(&el);
    el = next;
    }
}

void ncbiBlastIterationQueryIDSave(struct ncbiBlastIterationQueryID *obj, int indent, FILE *f)
/* Save ncbiBlastIterationQueryID to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Iteration_query-ID>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Iteration_query-ID>\n");
}

struct ncbiBlastIterationQueryID *ncbiBlastIterationQueryIDLoad(char *fileName)
/* Load ncbiBlastIterationQueryID from XML file where it is root element. */
{
struct ncbiBlastIterationQueryID *obj;
xapParseAny(fileName, "Iteration_query-ID", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastIterationQueryID *ncbiBlastIterationQueryIDLoadNext(struct xap *xap)
/* Load next ncbiBlastIterationQueryID element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Iteration_query-ID");
}

void ncbiBlastIterationQueryDefFree(struct ncbiBlastIterationQueryDef **pObj)
/* Free up ncbiBlastIterationQueryDef. */
{
struct ncbiBlastIterationQueryDef *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastIterationQueryDefFreeList(struct ncbiBlastIterationQueryDef **pList)
/* Free up list of ncbiBlastIterationQueryDef. */
{
struct ncbiBlastIterationQueryDef *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastIterationQueryDefFree(&el);
    el = next;
    }
}

void ncbiBlastIterationQueryDefSave(struct ncbiBlastIterationQueryDef *obj, int indent, FILE *f)
/* Save ncbiBlastIterationQueryDef to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Iteration_query-def>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Iteration_query-def>\n");
}

struct ncbiBlastIterationQueryDef *ncbiBlastIterationQueryDefLoad(char *fileName)
/* Load ncbiBlastIterationQueryDef from XML file where it is root element. */
{
struct ncbiBlastIterationQueryDef *obj;
xapParseAny(fileName, "Iteration_query-def", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastIterationQueryDef *ncbiBlastIterationQueryDefLoadNext(struct xap *xap)
/* Load next ncbiBlastIterationQueryDef element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Iteration_query-def");
}

void ncbiBlastIterationQueryLenFree(struct ncbiBlastIterationQueryLen **pObj)
/* Free up ncbiBlastIterationQueryLen. */
{
struct ncbiBlastIterationQueryLen *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastIterationQueryLenFreeList(struct ncbiBlastIterationQueryLen **pList)
/* Free up list of ncbiBlastIterationQueryLen. */
{
struct ncbiBlastIterationQueryLen *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastIterationQueryLenFree(&el);
    el = next;
    }
}

void ncbiBlastIterationQueryLenSave(struct ncbiBlastIterationQueryLen *obj, int indent, FILE *f)
/* Save ncbiBlastIterationQueryLen to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Iteration_query-len>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Iteration_query-len>\n");
}

struct ncbiBlastIterationQueryLen *ncbiBlastIterationQueryLenLoad(char *fileName)
/* Load ncbiBlastIterationQueryLen from XML file where it is root element. */
{
struct ncbiBlastIterationQueryLen *obj;
xapParseAny(fileName, "Iteration_query-len", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastIterationQueryLen *ncbiBlastIterationQueryLenLoadNext(struct xap *xap)
/* Load next ncbiBlastIterationQueryLen element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Iteration_query-len");
}

void ncbiBlastIterationHitsFree(struct ncbiBlastIterationHits **pObj)
/* Free up ncbiBlastIterationHits. */
{
struct ncbiBlastIterationHits *obj = *pObj;
if (obj == NULL) return;
ncbiBlastHitFreeList(&obj->ncbiBlastHit);
freez(pObj);
}

void ncbiBlastIterationHitsFreeList(struct ncbiBlastIterationHits **pList)
/* Free up list of ncbiBlastIterationHits. */
{
struct ncbiBlastIterationHits *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastIterationHitsFree(&el);
    el = next;
    }
}

void ncbiBlastIterationHitsSave(struct ncbiBlastIterationHits *obj, int indent, FILE *f)
/* Save ncbiBlastIterationHits to file. */
{
struct ncbiBlastHit *ncbiBlastHit;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Iteration_hits>");
for (ncbiBlastHit = obj->ncbiBlastHit; ncbiBlastHit != NULL; ncbiBlastHit = ncbiBlastHit->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   ncbiBlastHitSave(ncbiBlastHit, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</Iteration_hits>\n");
}

struct ncbiBlastIterationHits *ncbiBlastIterationHitsLoad(char *fileName)
/* Load ncbiBlastIterationHits from XML file where it is root element. */
{
struct ncbiBlastIterationHits *obj;
xapParseAny(fileName, "Iteration_hits", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastIterationHits *ncbiBlastIterationHitsLoadNext(struct xap *xap)
/* Load next ncbiBlastIterationHits element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Iteration_hits");
}

void ncbiBlastIterationStatFree(struct ncbiBlastIterationStat **pObj)
/* Free up ncbiBlastIterationStat. */
{
struct ncbiBlastIterationStat *obj = *pObj;
if (obj == NULL) return;
ncbiBlastStatisticsFree(&obj->ncbiBlastStatistics);
freez(pObj);
}

void ncbiBlastIterationStatFreeList(struct ncbiBlastIterationStat **pList)
/* Free up list of ncbiBlastIterationStat. */
{
struct ncbiBlastIterationStat *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastIterationStatFree(&el);
    el = next;
    }
}

void ncbiBlastIterationStatSave(struct ncbiBlastIterationStat *obj, int indent, FILE *f)
/* Save ncbiBlastIterationStat to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Iteration_stat>");
fprintf(f, "\n");
ncbiBlastStatisticsSave(obj->ncbiBlastStatistics, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</Iteration_stat>\n");
}

struct ncbiBlastIterationStat *ncbiBlastIterationStatLoad(char *fileName)
/* Load ncbiBlastIterationStat from XML file where it is root element. */
{
struct ncbiBlastIterationStat *obj;
xapParseAny(fileName, "Iteration_stat", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastIterationStat *ncbiBlastIterationStatLoadNext(struct xap *xap)
/* Load next ncbiBlastIterationStat element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Iteration_stat");
}

void ncbiBlastIterationMessageFree(struct ncbiBlastIterationMessage **pObj)
/* Free up ncbiBlastIterationMessage. */
{
struct ncbiBlastIterationMessage *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastIterationMessageFreeList(struct ncbiBlastIterationMessage **pList)
/* Free up list of ncbiBlastIterationMessage. */
{
struct ncbiBlastIterationMessage *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastIterationMessageFree(&el);
    el = next;
    }
}

void ncbiBlastIterationMessageSave(struct ncbiBlastIterationMessage *obj, int indent, FILE *f)
/* Save ncbiBlastIterationMessage to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Iteration_message>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Iteration_message>\n");
}

struct ncbiBlastIterationMessage *ncbiBlastIterationMessageLoad(char *fileName)
/* Load ncbiBlastIterationMessage from XML file where it is root element. */
{
struct ncbiBlastIterationMessage *obj;
xapParseAny(fileName, "Iteration_message", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastIterationMessage *ncbiBlastIterationMessageLoadNext(struct xap *xap)
/* Load next ncbiBlastIterationMessage element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Iteration_message");
}

void ncbiBlastParametersFree(struct ncbiBlastParameters **pObj)
/* Free up ncbiBlastParameters. */
{
struct ncbiBlastParameters *obj = *pObj;
if (obj == NULL) return;
ncbiBlastParametersMatrixFree(&obj->ncbiBlastParametersMatrix);
ncbiBlastParametersExpectFree(&obj->ncbiBlastParametersExpect);
ncbiBlastParametersIncludeFree(&obj->ncbiBlastParametersInclude);
ncbiBlastParametersScMatchFree(&obj->ncbiBlastParametersScMatch);
ncbiBlastParametersScMismatchFree(&obj->ncbiBlastParametersScMismatch);
ncbiBlastParametersGapOpenFree(&obj->ncbiBlastParametersGapOpen);
ncbiBlastParametersGapExtendFree(&obj->ncbiBlastParametersGapExtend);
ncbiBlastParametersFilterFree(&obj->ncbiBlastParametersFilter);
ncbiBlastParametersPatternFree(&obj->ncbiBlastParametersPattern);
ncbiBlastParametersEntrezQueryFree(&obj->ncbiBlastParametersEntrezQuery);
freez(pObj);
}

void ncbiBlastParametersFreeList(struct ncbiBlastParameters **pList)
/* Free up list of ncbiBlastParameters. */
{
struct ncbiBlastParameters *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersFree(&el);
    el = next;
    }
}

void ncbiBlastParametersSave(struct ncbiBlastParameters *obj, int indent, FILE *f)
/* Save ncbiBlastParameters to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters>");
fprintf(f, "\n");
ncbiBlastParametersMatrixSave(obj->ncbiBlastParametersMatrix, indent+2, f);
ncbiBlastParametersExpectSave(obj->ncbiBlastParametersExpect, indent+2, f);
ncbiBlastParametersIncludeSave(obj->ncbiBlastParametersInclude, indent+2, f);
ncbiBlastParametersScMatchSave(obj->ncbiBlastParametersScMatch, indent+2, f);
ncbiBlastParametersScMismatchSave(obj->ncbiBlastParametersScMismatch, indent+2, f);
ncbiBlastParametersGapOpenSave(obj->ncbiBlastParametersGapOpen, indent+2, f);
ncbiBlastParametersGapExtendSave(obj->ncbiBlastParametersGapExtend, indent+2, f);
ncbiBlastParametersFilterSave(obj->ncbiBlastParametersFilter, indent+2, f);
ncbiBlastParametersPatternSave(obj->ncbiBlastParametersPattern, indent+2, f);
ncbiBlastParametersEntrezQuerySave(obj->ncbiBlastParametersEntrezQuery, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</Parameters>\n");
}

struct ncbiBlastParameters *ncbiBlastParametersLoad(char *fileName)
/* Load ncbiBlastParameters from XML file where it is root element. */
{
struct ncbiBlastParameters *obj;
xapParseAny(fileName, "Parameters", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParameters *ncbiBlastParametersLoadNext(struct xap *xap)
/* Load next ncbiBlastParameters element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters");
}

void ncbiBlastParametersMatrixFree(struct ncbiBlastParametersMatrix **pObj)
/* Free up ncbiBlastParametersMatrix. */
{
struct ncbiBlastParametersMatrix *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastParametersMatrixFreeList(struct ncbiBlastParametersMatrix **pList)
/* Free up list of ncbiBlastParametersMatrix. */
{
struct ncbiBlastParametersMatrix *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersMatrixFree(&el);
    el = next;
    }
}

void ncbiBlastParametersMatrixSave(struct ncbiBlastParametersMatrix *obj, int indent, FILE *f)
/* Save ncbiBlastParametersMatrix to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_matrix>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Parameters_matrix>\n");
}

struct ncbiBlastParametersMatrix *ncbiBlastParametersMatrixLoad(char *fileName)
/* Load ncbiBlastParametersMatrix from XML file where it is root element. */
{
struct ncbiBlastParametersMatrix *obj;
xapParseAny(fileName, "Parameters_matrix", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersMatrix *ncbiBlastParametersMatrixLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersMatrix element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_matrix");
}

void ncbiBlastParametersExpectFree(struct ncbiBlastParametersExpect **pObj)
/* Free up ncbiBlastParametersExpect. */
{
struct ncbiBlastParametersExpect *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastParametersExpectFreeList(struct ncbiBlastParametersExpect **pList)
/* Free up list of ncbiBlastParametersExpect. */
{
struct ncbiBlastParametersExpect *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersExpectFree(&el);
    el = next;
    }
}

void ncbiBlastParametersExpectSave(struct ncbiBlastParametersExpect *obj, int indent, FILE *f)
/* Save ncbiBlastParametersExpect to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_expect>");
fprintf(f, "%f", obj->text);
fprintf(f, "</Parameters_expect>\n");
}

struct ncbiBlastParametersExpect *ncbiBlastParametersExpectLoad(char *fileName)
/* Load ncbiBlastParametersExpect from XML file where it is root element. */
{
struct ncbiBlastParametersExpect *obj;
xapParseAny(fileName, "Parameters_expect", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersExpect *ncbiBlastParametersExpectLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersExpect element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_expect");
}

void ncbiBlastParametersIncludeFree(struct ncbiBlastParametersInclude **pObj)
/* Free up ncbiBlastParametersInclude. */
{
struct ncbiBlastParametersInclude *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastParametersIncludeFreeList(struct ncbiBlastParametersInclude **pList)
/* Free up list of ncbiBlastParametersInclude. */
{
struct ncbiBlastParametersInclude *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersIncludeFree(&el);
    el = next;
    }
}

void ncbiBlastParametersIncludeSave(struct ncbiBlastParametersInclude *obj, int indent, FILE *f)
/* Save ncbiBlastParametersInclude to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_include>");
fprintf(f, "%f", obj->text);
fprintf(f, "</Parameters_include>\n");
}

struct ncbiBlastParametersInclude *ncbiBlastParametersIncludeLoad(char *fileName)
/* Load ncbiBlastParametersInclude from XML file where it is root element. */
{
struct ncbiBlastParametersInclude *obj;
xapParseAny(fileName, "Parameters_include", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersInclude *ncbiBlastParametersIncludeLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersInclude element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_include");
}

void ncbiBlastParametersScMatchFree(struct ncbiBlastParametersScMatch **pObj)
/* Free up ncbiBlastParametersScMatch. */
{
struct ncbiBlastParametersScMatch *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastParametersScMatchFreeList(struct ncbiBlastParametersScMatch **pList)
/* Free up list of ncbiBlastParametersScMatch. */
{
struct ncbiBlastParametersScMatch *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersScMatchFree(&el);
    el = next;
    }
}

void ncbiBlastParametersScMatchSave(struct ncbiBlastParametersScMatch *obj, int indent, FILE *f)
/* Save ncbiBlastParametersScMatch to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_sc-match>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Parameters_sc-match>\n");
}

struct ncbiBlastParametersScMatch *ncbiBlastParametersScMatchLoad(char *fileName)
/* Load ncbiBlastParametersScMatch from XML file where it is root element. */
{
struct ncbiBlastParametersScMatch *obj;
xapParseAny(fileName, "Parameters_sc-match", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersScMatch *ncbiBlastParametersScMatchLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersScMatch element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_sc-match");
}

void ncbiBlastParametersScMismatchFree(struct ncbiBlastParametersScMismatch **pObj)
/* Free up ncbiBlastParametersScMismatch. */
{
struct ncbiBlastParametersScMismatch *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastParametersScMismatchFreeList(struct ncbiBlastParametersScMismatch **pList)
/* Free up list of ncbiBlastParametersScMismatch. */
{
struct ncbiBlastParametersScMismatch *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersScMismatchFree(&el);
    el = next;
    }
}

void ncbiBlastParametersScMismatchSave(struct ncbiBlastParametersScMismatch *obj, int indent, FILE *f)
/* Save ncbiBlastParametersScMismatch to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_sc-mismatch>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Parameters_sc-mismatch>\n");
}

struct ncbiBlastParametersScMismatch *ncbiBlastParametersScMismatchLoad(char *fileName)
/* Load ncbiBlastParametersScMismatch from XML file where it is root element. */
{
struct ncbiBlastParametersScMismatch *obj;
xapParseAny(fileName, "Parameters_sc-mismatch", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersScMismatch *ncbiBlastParametersScMismatchLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersScMismatch element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_sc-mismatch");
}

void ncbiBlastParametersGapOpenFree(struct ncbiBlastParametersGapOpen **pObj)
/* Free up ncbiBlastParametersGapOpen. */
{
struct ncbiBlastParametersGapOpen *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastParametersGapOpenFreeList(struct ncbiBlastParametersGapOpen **pList)
/* Free up list of ncbiBlastParametersGapOpen. */
{
struct ncbiBlastParametersGapOpen *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersGapOpenFree(&el);
    el = next;
    }
}

void ncbiBlastParametersGapOpenSave(struct ncbiBlastParametersGapOpen *obj, int indent, FILE *f)
/* Save ncbiBlastParametersGapOpen to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_gap-open>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Parameters_gap-open>\n");
}

struct ncbiBlastParametersGapOpen *ncbiBlastParametersGapOpenLoad(char *fileName)
/* Load ncbiBlastParametersGapOpen from XML file where it is root element. */
{
struct ncbiBlastParametersGapOpen *obj;
xapParseAny(fileName, "Parameters_gap-open", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersGapOpen *ncbiBlastParametersGapOpenLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersGapOpen element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_gap-open");
}

void ncbiBlastParametersGapExtendFree(struct ncbiBlastParametersGapExtend **pObj)
/* Free up ncbiBlastParametersGapExtend. */
{
struct ncbiBlastParametersGapExtend *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastParametersGapExtendFreeList(struct ncbiBlastParametersGapExtend **pList)
/* Free up list of ncbiBlastParametersGapExtend. */
{
struct ncbiBlastParametersGapExtend *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersGapExtendFree(&el);
    el = next;
    }
}

void ncbiBlastParametersGapExtendSave(struct ncbiBlastParametersGapExtend *obj, int indent, FILE *f)
/* Save ncbiBlastParametersGapExtend to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_gap-extend>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Parameters_gap-extend>\n");
}

struct ncbiBlastParametersGapExtend *ncbiBlastParametersGapExtendLoad(char *fileName)
/* Load ncbiBlastParametersGapExtend from XML file where it is root element. */
{
struct ncbiBlastParametersGapExtend *obj;
xapParseAny(fileName, "Parameters_gap-extend", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersGapExtend *ncbiBlastParametersGapExtendLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersGapExtend element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_gap-extend");
}

void ncbiBlastParametersFilterFree(struct ncbiBlastParametersFilter **pObj)
/* Free up ncbiBlastParametersFilter. */
{
struct ncbiBlastParametersFilter *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastParametersFilterFreeList(struct ncbiBlastParametersFilter **pList)
/* Free up list of ncbiBlastParametersFilter. */
{
struct ncbiBlastParametersFilter *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersFilterFree(&el);
    el = next;
    }
}

void ncbiBlastParametersFilterSave(struct ncbiBlastParametersFilter *obj, int indent, FILE *f)
/* Save ncbiBlastParametersFilter to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_filter>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Parameters_filter>\n");
}

struct ncbiBlastParametersFilter *ncbiBlastParametersFilterLoad(char *fileName)
/* Load ncbiBlastParametersFilter from XML file where it is root element. */
{
struct ncbiBlastParametersFilter *obj;
xapParseAny(fileName, "Parameters_filter", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersFilter *ncbiBlastParametersFilterLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersFilter element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_filter");
}

void ncbiBlastParametersPatternFree(struct ncbiBlastParametersPattern **pObj)
/* Free up ncbiBlastParametersPattern. */
{
struct ncbiBlastParametersPattern *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastParametersPatternFreeList(struct ncbiBlastParametersPattern **pList)
/* Free up list of ncbiBlastParametersPattern. */
{
struct ncbiBlastParametersPattern *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersPatternFree(&el);
    el = next;
    }
}

void ncbiBlastParametersPatternSave(struct ncbiBlastParametersPattern *obj, int indent, FILE *f)
/* Save ncbiBlastParametersPattern to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_pattern>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Parameters_pattern>\n");
}

struct ncbiBlastParametersPattern *ncbiBlastParametersPatternLoad(char *fileName)
/* Load ncbiBlastParametersPattern from XML file where it is root element. */
{
struct ncbiBlastParametersPattern *obj;
xapParseAny(fileName, "Parameters_pattern", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersPattern *ncbiBlastParametersPatternLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersPattern element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_pattern");
}

void ncbiBlastParametersEntrezQueryFree(struct ncbiBlastParametersEntrezQuery **pObj)
/* Free up ncbiBlastParametersEntrezQuery. */
{
struct ncbiBlastParametersEntrezQuery *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastParametersEntrezQueryFreeList(struct ncbiBlastParametersEntrezQuery **pList)
/* Free up list of ncbiBlastParametersEntrezQuery. */
{
struct ncbiBlastParametersEntrezQuery *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastParametersEntrezQueryFree(&el);
    el = next;
    }
}

void ncbiBlastParametersEntrezQuerySave(struct ncbiBlastParametersEntrezQuery *obj, int indent, FILE *f)
/* Save ncbiBlastParametersEntrezQuery to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Parameters_entrez-query>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Parameters_entrez-query>\n");
}

struct ncbiBlastParametersEntrezQuery *ncbiBlastParametersEntrezQueryLoad(char *fileName)
/* Load ncbiBlastParametersEntrezQuery from XML file where it is root element. */
{
struct ncbiBlastParametersEntrezQuery *obj;
xapParseAny(fileName, "Parameters_entrez-query", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastParametersEntrezQuery *ncbiBlastParametersEntrezQueryLoadNext(struct xap *xap)
/* Load next ncbiBlastParametersEntrezQuery element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Parameters_entrez-query");
}

void ncbiBlastStatisticsFree(struct ncbiBlastStatistics **pObj)
/* Free up ncbiBlastStatistics. */
{
struct ncbiBlastStatistics *obj = *pObj;
if (obj == NULL) return;
ncbiBlastStatisticsDbNumFree(&obj->ncbiBlastStatisticsDbNum);
ncbiBlastStatisticsDbLenFree(&obj->ncbiBlastStatisticsDbLen);
ncbiBlastStatisticsHspLenFree(&obj->ncbiBlastStatisticsHspLen);
ncbiBlastStatisticsEffSpaceFree(&obj->ncbiBlastStatisticsEffSpace);
ncbiBlastStatisticsKappaFree(&obj->ncbiBlastStatisticsKappa);
ncbiBlastStatisticsLambdaFree(&obj->ncbiBlastStatisticsLambda);
ncbiBlastStatisticsEntropyFree(&obj->ncbiBlastStatisticsEntropy);
freez(pObj);
}

void ncbiBlastStatisticsFreeList(struct ncbiBlastStatistics **pList)
/* Free up list of ncbiBlastStatistics. */
{
struct ncbiBlastStatistics *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastStatisticsFree(&el);
    el = next;
    }
}

void ncbiBlastStatisticsSave(struct ncbiBlastStatistics *obj, int indent, FILE *f)
/* Save ncbiBlastStatistics to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Statistics>");
fprintf(f, "\n");
ncbiBlastStatisticsDbNumSave(obj->ncbiBlastStatisticsDbNum, indent+2, f);
ncbiBlastStatisticsDbLenSave(obj->ncbiBlastStatisticsDbLen, indent+2, f);
ncbiBlastStatisticsHspLenSave(obj->ncbiBlastStatisticsHspLen, indent+2, f);
ncbiBlastStatisticsEffSpaceSave(obj->ncbiBlastStatisticsEffSpace, indent+2, f);
ncbiBlastStatisticsKappaSave(obj->ncbiBlastStatisticsKappa, indent+2, f);
ncbiBlastStatisticsLambdaSave(obj->ncbiBlastStatisticsLambda, indent+2, f);
ncbiBlastStatisticsEntropySave(obj->ncbiBlastStatisticsEntropy, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</Statistics>\n");
}

struct ncbiBlastStatistics *ncbiBlastStatisticsLoad(char *fileName)
/* Load ncbiBlastStatistics from XML file where it is root element. */
{
struct ncbiBlastStatistics *obj;
xapParseAny(fileName, "Statistics", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastStatistics *ncbiBlastStatisticsLoadNext(struct xap *xap)
/* Load next ncbiBlastStatistics element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Statistics");
}

void ncbiBlastStatisticsDbNumFree(struct ncbiBlastStatisticsDbNum **pObj)
/* Free up ncbiBlastStatisticsDbNum. */
{
struct ncbiBlastStatisticsDbNum *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastStatisticsDbNumFreeList(struct ncbiBlastStatisticsDbNum **pList)
/* Free up list of ncbiBlastStatisticsDbNum. */
{
struct ncbiBlastStatisticsDbNum *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastStatisticsDbNumFree(&el);
    el = next;
    }
}

void ncbiBlastStatisticsDbNumSave(struct ncbiBlastStatisticsDbNum *obj, int indent, FILE *f)
/* Save ncbiBlastStatisticsDbNum to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Statistics_db-num>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Statistics_db-num>\n");
}

struct ncbiBlastStatisticsDbNum *ncbiBlastStatisticsDbNumLoad(char *fileName)
/* Load ncbiBlastStatisticsDbNum from XML file where it is root element. */
{
struct ncbiBlastStatisticsDbNum *obj;
xapParseAny(fileName, "Statistics_db-num", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastStatisticsDbNum *ncbiBlastStatisticsDbNumLoadNext(struct xap *xap)
/* Load next ncbiBlastStatisticsDbNum element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Statistics_db-num");
}

void ncbiBlastStatisticsDbLenFree(struct ncbiBlastStatisticsDbLen **pObj)
/* Free up ncbiBlastStatisticsDbLen. */
{
struct ncbiBlastStatisticsDbLen *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastStatisticsDbLenFreeList(struct ncbiBlastStatisticsDbLen **pList)
/* Free up list of ncbiBlastStatisticsDbLen. */
{
struct ncbiBlastStatisticsDbLen *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastStatisticsDbLenFree(&el);
    el = next;
    }
}

void ncbiBlastStatisticsDbLenSave(struct ncbiBlastStatisticsDbLen *obj, int indent, FILE *f)
/* Save ncbiBlastStatisticsDbLen to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Statistics_db-len>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Statistics_db-len>\n");
}

struct ncbiBlastStatisticsDbLen *ncbiBlastStatisticsDbLenLoad(char *fileName)
/* Load ncbiBlastStatisticsDbLen from XML file where it is root element. */
{
struct ncbiBlastStatisticsDbLen *obj;
xapParseAny(fileName, "Statistics_db-len", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastStatisticsDbLen *ncbiBlastStatisticsDbLenLoadNext(struct xap *xap)
/* Load next ncbiBlastStatisticsDbLen element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Statistics_db-len");
}

void ncbiBlastStatisticsHspLenFree(struct ncbiBlastStatisticsHspLen **pObj)
/* Free up ncbiBlastStatisticsHspLen. */
{
struct ncbiBlastStatisticsHspLen *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastStatisticsHspLenFreeList(struct ncbiBlastStatisticsHspLen **pList)
/* Free up list of ncbiBlastStatisticsHspLen. */
{
struct ncbiBlastStatisticsHspLen *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastStatisticsHspLenFree(&el);
    el = next;
    }
}

void ncbiBlastStatisticsHspLenSave(struct ncbiBlastStatisticsHspLen *obj, int indent, FILE *f)
/* Save ncbiBlastStatisticsHspLen to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Statistics_hsp-len>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Statistics_hsp-len>\n");
}

struct ncbiBlastStatisticsHspLen *ncbiBlastStatisticsHspLenLoad(char *fileName)
/* Load ncbiBlastStatisticsHspLen from XML file where it is root element. */
{
struct ncbiBlastStatisticsHspLen *obj;
xapParseAny(fileName, "Statistics_hsp-len", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastStatisticsHspLen *ncbiBlastStatisticsHspLenLoadNext(struct xap *xap)
/* Load next ncbiBlastStatisticsHspLen element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Statistics_hsp-len");
}

void ncbiBlastStatisticsEffSpaceFree(struct ncbiBlastStatisticsEffSpace **pObj)
/* Free up ncbiBlastStatisticsEffSpace. */
{
struct ncbiBlastStatisticsEffSpace *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastStatisticsEffSpaceFreeList(struct ncbiBlastStatisticsEffSpace **pList)
/* Free up list of ncbiBlastStatisticsEffSpace. */
{
struct ncbiBlastStatisticsEffSpace *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastStatisticsEffSpaceFree(&el);
    el = next;
    }
}

void ncbiBlastStatisticsEffSpaceSave(struct ncbiBlastStatisticsEffSpace *obj, int indent, FILE *f)
/* Save ncbiBlastStatisticsEffSpace to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Statistics_eff-space>");
fprintf(f, "%f", obj->text);
fprintf(f, "</Statistics_eff-space>\n");
}

struct ncbiBlastStatisticsEffSpace *ncbiBlastStatisticsEffSpaceLoad(char *fileName)
/* Load ncbiBlastStatisticsEffSpace from XML file where it is root element. */
{
struct ncbiBlastStatisticsEffSpace *obj;
xapParseAny(fileName, "Statistics_eff-space", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastStatisticsEffSpace *ncbiBlastStatisticsEffSpaceLoadNext(struct xap *xap)
/* Load next ncbiBlastStatisticsEffSpace element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Statistics_eff-space");
}

void ncbiBlastStatisticsKappaFree(struct ncbiBlastStatisticsKappa **pObj)
/* Free up ncbiBlastStatisticsKappa. */
{
struct ncbiBlastStatisticsKappa *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastStatisticsKappaFreeList(struct ncbiBlastStatisticsKappa **pList)
/* Free up list of ncbiBlastStatisticsKappa. */
{
struct ncbiBlastStatisticsKappa *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastStatisticsKappaFree(&el);
    el = next;
    }
}

void ncbiBlastStatisticsKappaSave(struct ncbiBlastStatisticsKappa *obj, int indent, FILE *f)
/* Save ncbiBlastStatisticsKappa to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Statistics_kappa>");
fprintf(f, "%f", obj->text);
fprintf(f, "</Statistics_kappa>\n");
}

struct ncbiBlastStatisticsKappa *ncbiBlastStatisticsKappaLoad(char *fileName)
/* Load ncbiBlastStatisticsKappa from XML file where it is root element. */
{
struct ncbiBlastStatisticsKappa *obj;
xapParseAny(fileName, "Statistics_kappa", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastStatisticsKappa *ncbiBlastStatisticsKappaLoadNext(struct xap *xap)
/* Load next ncbiBlastStatisticsKappa element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Statistics_kappa");
}

void ncbiBlastStatisticsLambdaFree(struct ncbiBlastStatisticsLambda **pObj)
/* Free up ncbiBlastStatisticsLambda. */
{
struct ncbiBlastStatisticsLambda *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastStatisticsLambdaFreeList(struct ncbiBlastStatisticsLambda **pList)
/* Free up list of ncbiBlastStatisticsLambda. */
{
struct ncbiBlastStatisticsLambda *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastStatisticsLambdaFree(&el);
    el = next;
    }
}

void ncbiBlastStatisticsLambdaSave(struct ncbiBlastStatisticsLambda *obj, int indent, FILE *f)
/* Save ncbiBlastStatisticsLambda to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Statistics_lambda>");
fprintf(f, "%f", obj->text);
fprintf(f, "</Statistics_lambda>\n");
}

struct ncbiBlastStatisticsLambda *ncbiBlastStatisticsLambdaLoad(char *fileName)
/* Load ncbiBlastStatisticsLambda from XML file where it is root element. */
{
struct ncbiBlastStatisticsLambda *obj;
xapParseAny(fileName, "Statistics_lambda", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastStatisticsLambda *ncbiBlastStatisticsLambdaLoadNext(struct xap *xap)
/* Load next ncbiBlastStatisticsLambda element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Statistics_lambda");
}

void ncbiBlastStatisticsEntropyFree(struct ncbiBlastStatisticsEntropy **pObj)
/* Free up ncbiBlastStatisticsEntropy. */
{
struct ncbiBlastStatisticsEntropy *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastStatisticsEntropyFreeList(struct ncbiBlastStatisticsEntropy **pList)
/* Free up list of ncbiBlastStatisticsEntropy. */
{
struct ncbiBlastStatisticsEntropy *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastStatisticsEntropyFree(&el);
    el = next;
    }
}

void ncbiBlastStatisticsEntropySave(struct ncbiBlastStatisticsEntropy *obj, int indent, FILE *f)
/* Save ncbiBlastStatisticsEntropy to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Statistics_entropy>");
fprintf(f, "%f", obj->text);
fprintf(f, "</Statistics_entropy>\n");
}

struct ncbiBlastStatisticsEntropy *ncbiBlastStatisticsEntropyLoad(char *fileName)
/* Load ncbiBlastStatisticsEntropy from XML file where it is root element. */
{
struct ncbiBlastStatisticsEntropy *obj;
xapParseAny(fileName, "Statistics_entropy", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastStatisticsEntropy *ncbiBlastStatisticsEntropyLoadNext(struct xap *xap)
/* Load next ncbiBlastStatisticsEntropy element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Statistics_entropy");
}

void ncbiBlastHitFree(struct ncbiBlastHit **pObj)
/* Free up ncbiBlastHit. */
{
struct ncbiBlastHit *obj = *pObj;
if (obj == NULL) return;
ncbiBlastHitNumFree(&obj->ncbiBlastHitNum);
ncbiBlastHitIdFree(&obj->ncbiBlastHitId);
ncbiBlastHitDefFree(&obj->ncbiBlastHitDef);
ncbiBlastHitAccessionFree(&obj->ncbiBlastHitAccession);
ncbiBlastHitLenFree(&obj->ncbiBlastHitLen);
ncbiBlastHitHspsFree(&obj->ncbiBlastHitHsps);
freez(pObj);
}

void ncbiBlastHitFreeList(struct ncbiBlastHit **pList)
/* Free up list of ncbiBlastHit. */
{
struct ncbiBlastHit *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHitFree(&el);
    el = next;
    }
}

void ncbiBlastHitSave(struct ncbiBlastHit *obj, int indent, FILE *f)
/* Save ncbiBlastHit to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hit>");
fprintf(f, "\n");
ncbiBlastHitNumSave(obj->ncbiBlastHitNum, indent+2, f);
ncbiBlastHitIdSave(obj->ncbiBlastHitId, indent+2, f);
ncbiBlastHitDefSave(obj->ncbiBlastHitDef, indent+2, f);
ncbiBlastHitAccessionSave(obj->ncbiBlastHitAccession, indent+2, f);
ncbiBlastHitLenSave(obj->ncbiBlastHitLen, indent+2, f);
ncbiBlastHitHspsSave(obj->ncbiBlastHitHsps, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</Hit>\n");
}

struct ncbiBlastHit *ncbiBlastHitLoad(char *fileName)
/* Load ncbiBlastHit from XML file where it is root element. */
{
struct ncbiBlastHit *obj;
xapParseAny(fileName, "Hit", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHit *ncbiBlastHitLoadNext(struct xap *xap)
/* Load next ncbiBlastHit element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hit");
}

void ncbiBlastHitNumFree(struct ncbiBlastHitNum **pObj)
/* Free up ncbiBlastHitNum. */
{
struct ncbiBlastHitNum *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHitNumFreeList(struct ncbiBlastHitNum **pList)
/* Free up list of ncbiBlastHitNum. */
{
struct ncbiBlastHitNum *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHitNumFree(&el);
    el = next;
    }
}

void ncbiBlastHitNumSave(struct ncbiBlastHitNum *obj, int indent, FILE *f)
/* Save ncbiBlastHitNum to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hit_num>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hit_num>\n");
}

struct ncbiBlastHitNum *ncbiBlastHitNumLoad(char *fileName)
/* Load ncbiBlastHitNum from XML file where it is root element. */
{
struct ncbiBlastHitNum *obj;
xapParseAny(fileName, "Hit_num", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHitNum *ncbiBlastHitNumLoadNext(struct xap *xap)
/* Load next ncbiBlastHitNum element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hit_num");
}

void ncbiBlastHitIdFree(struct ncbiBlastHitId **pObj)
/* Free up ncbiBlastHitId. */
{
struct ncbiBlastHitId *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastHitIdFreeList(struct ncbiBlastHitId **pList)
/* Free up list of ncbiBlastHitId. */
{
struct ncbiBlastHitId *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHitIdFree(&el);
    el = next;
    }
}

void ncbiBlastHitIdSave(struct ncbiBlastHitId *obj, int indent, FILE *f)
/* Save ncbiBlastHitId to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hit_id>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Hit_id>\n");
}

struct ncbiBlastHitId *ncbiBlastHitIdLoad(char *fileName)
/* Load ncbiBlastHitId from XML file where it is root element. */
{
struct ncbiBlastHitId *obj;
xapParseAny(fileName, "Hit_id", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHitId *ncbiBlastHitIdLoadNext(struct xap *xap)
/* Load next ncbiBlastHitId element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hit_id");
}

void ncbiBlastHitDefFree(struct ncbiBlastHitDef **pObj)
/* Free up ncbiBlastHitDef. */
{
struct ncbiBlastHitDef *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastHitDefFreeList(struct ncbiBlastHitDef **pList)
/* Free up list of ncbiBlastHitDef. */
{
struct ncbiBlastHitDef *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHitDefFree(&el);
    el = next;
    }
}

void ncbiBlastHitDefSave(struct ncbiBlastHitDef *obj, int indent, FILE *f)
/* Save ncbiBlastHitDef to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hit_def>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Hit_def>\n");
}

struct ncbiBlastHitDef *ncbiBlastHitDefLoad(char *fileName)
/* Load ncbiBlastHitDef from XML file where it is root element. */
{
struct ncbiBlastHitDef *obj;
xapParseAny(fileName, "Hit_def", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHitDef *ncbiBlastHitDefLoadNext(struct xap *xap)
/* Load next ncbiBlastHitDef element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hit_def");
}

void ncbiBlastHitAccessionFree(struct ncbiBlastHitAccession **pObj)
/* Free up ncbiBlastHitAccession. */
{
struct ncbiBlastHitAccession *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastHitAccessionFreeList(struct ncbiBlastHitAccession **pList)
/* Free up list of ncbiBlastHitAccession. */
{
struct ncbiBlastHitAccession *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHitAccessionFree(&el);
    el = next;
    }
}

void ncbiBlastHitAccessionSave(struct ncbiBlastHitAccession *obj, int indent, FILE *f)
/* Save ncbiBlastHitAccession to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hit_accession>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Hit_accession>\n");
}

struct ncbiBlastHitAccession *ncbiBlastHitAccessionLoad(char *fileName)
/* Load ncbiBlastHitAccession from XML file where it is root element. */
{
struct ncbiBlastHitAccession *obj;
xapParseAny(fileName, "Hit_accession", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHitAccession *ncbiBlastHitAccessionLoadNext(struct xap *xap)
/* Load next ncbiBlastHitAccession element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hit_accession");
}

void ncbiBlastHitLenFree(struct ncbiBlastHitLen **pObj)
/* Free up ncbiBlastHitLen. */
{
struct ncbiBlastHitLen *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHitLenFreeList(struct ncbiBlastHitLen **pList)
/* Free up list of ncbiBlastHitLen. */
{
struct ncbiBlastHitLen *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHitLenFree(&el);
    el = next;
    }
}

void ncbiBlastHitLenSave(struct ncbiBlastHitLen *obj, int indent, FILE *f)
/* Save ncbiBlastHitLen to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hit_len>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hit_len>\n");
}

struct ncbiBlastHitLen *ncbiBlastHitLenLoad(char *fileName)
/* Load ncbiBlastHitLen from XML file where it is root element. */
{
struct ncbiBlastHitLen *obj;
xapParseAny(fileName, "Hit_len", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHitLen *ncbiBlastHitLenLoadNext(struct xap *xap)
/* Load next ncbiBlastHitLen element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hit_len");
}

void ncbiBlastHitHspsFree(struct ncbiBlastHitHsps **pObj)
/* Free up ncbiBlastHitHsps. */
{
struct ncbiBlastHitHsps *obj = *pObj;
if (obj == NULL) return;
ncbiBlastHspFreeList(&obj->ncbiBlastHsp);
freez(pObj);
}

void ncbiBlastHitHspsFreeList(struct ncbiBlastHitHsps **pList)
/* Free up list of ncbiBlastHitHsps. */
{
struct ncbiBlastHitHsps *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHitHspsFree(&el);
    el = next;
    }
}

void ncbiBlastHitHspsSave(struct ncbiBlastHitHsps *obj, int indent, FILE *f)
/* Save ncbiBlastHitHsps to file. */
{
struct ncbiBlastHsp *ncbiBlastHsp;
boolean isNode = TRUE;
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hit_hsps>");
for (ncbiBlastHsp = obj->ncbiBlastHsp; ncbiBlastHsp != NULL; ncbiBlastHsp = ncbiBlastHsp->next)
   {
   if (isNode)
       {
       fprintf(f, "\n");
       isNode = FALSE;
       }
   ncbiBlastHspSave(ncbiBlastHsp, indent+2, f);
   }
if (!isNode)
    xapIndent(indent, f);
fprintf(f, "</Hit_hsps>\n");
}

struct ncbiBlastHitHsps *ncbiBlastHitHspsLoad(char *fileName)
/* Load ncbiBlastHitHsps from XML file where it is root element. */
{
struct ncbiBlastHitHsps *obj;
xapParseAny(fileName, "Hit_hsps", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHitHsps *ncbiBlastHitHspsLoadNext(struct xap *xap)
/* Load next ncbiBlastHitHsps element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hit_hsps");
}

void ncbiBlastHspFree(struct ncbiBlastHsp **pObj)
/* Free up ncbiBlastHsp. */
{
struct ncbiBlastHsp *obj = *pObj;
if (obj == NULL) return;
ncbiBlastHspNumFree(&obj->ncbiBlastHspNum);
ncbiBlastHspBitScoreFree(&obj->ncbiBlastHspBitScore);
ncbiBlastHspScoreFree(&obj->ncbiBlastHspScore);
ncbiBlastHspEvalueFree(&obj->ncbiBlastHspEvalue);
ncbiBlastHspQueryFromFree(&obj->ncbiBlastHspQueryFrom);
ncbiBlastHspQueryToFree(&obj->ncbiBlastHspQueryTo);
ncbiBlastHspHitFromFree(&obj->ncbiBlastHspHitFrom);
ncbiBlastHspHitToFree(&obj->ncbiBlastHspHitTo);
ncbiBlastHspPatternFromFree(&obj->ncbiBlastHspPatternFrom);
ncbiBlastHspPatternToFree(&obj->ncbiBlastHspPatternTo);
ncbiBlastHspQueryFrameFree(&obj->ncbiBlastHspQueryFrame);
ncbiBlastHspHitFrameFree(&obj->ncbiBlastHspHitFrame);
ncbiBlastHspIdentityFree(&obj->ncbiBlastHspIdentity);
ncbiBlastHspPositiveFree(&obj->ncbiBlastHspPositive);
ncbiBlastHspGapsFree(&obj->ncbiBlastHspGaps);
ncbiBlastHspAlignLenFree(&obj->ncbiBlastHspAlignLen);
ncbiBlastHspDensityFree(&obj->ncbiBlastHspDensity);
ncbiBlastHspQseqFree(&obj->ncbiBlastHspQseq);
ncbiBlastHspHseqFree(&obj->ncbiBlastHspHseq);
ncbiBlastHspMidlineFree(&obj->ncbiBlastHspMidline);
freez(pObj);
}

void ncbiBlastHspFreeList(struct ncbiBlastHsp **pList)
/* Free up list of ncbiBlastHsp. */
{
struct ncbiBlastHsp *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspFree(&el);
    el = next;
    }
}

void ncbiBlastHspSave(struct ncbiBlastHsp *obj, int indent, FILE *f)
/* Save ncbiBlastHsp to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp>");
fprintf(f, "\n");
ncbiBlastHspNumSave(obj->ncbiBlastHspNum, indent+2, f);
ncbiBlastHspBitScoreSave(obj->ncbiBlastHspBitScore, indent+2, f);
ncbiBlastHspScoreSave(obj->ncbiBlastHspScore, indent+2, f);
ncbiBlastHspEvalueSave(obj->ncbiBlastHspEvalue, indent+2, f);
ncbiBlastHspQueryFromSave(obj->ncbiBlastHspQueryFrom, indent+2, f);
ncbiBlastHspQueryToSave(obj->ncbiBlastHspQueryTo, indent+2, f);
ncbiBlastHspHitFromSave(obj->ncbiBlastHspHitFrom, indent+2, f);
ncbiBlastHspHitToSave(obj->ncbiBlastHspHitTo, indent+2, f);
ncbiBlastHspPatternFromSave(obj->ncbiBlastHspPatternFrom, indent+2, f);
ncbiBlastHspPatternToSave(obj->ncbiBlastHspPatternTo, indent+2, f);
ncbiBlastHspQueryFrameSave(obj->ncbiBlastHspQueryFrame, indent+2, f);
ncbiBlastHspHitFrameSave(obj->ncbiBlastHspHitFrame, indent+2, f);
ncbiBlastHspIdentitySave(obj->ncbiBlastHspIdentity, indent+2, f);
ncbiBlastHspPositiveSave(obj->ncbiBlastHspPositive, indent+2, f);
ncbiBlastHspGapsSave(obj->ncbiBlastHspGaps, indent+2, f);
ncbiBlastHspAlignLenSave(obj->ncbiBlastHspAlignLen, indent+2, f);
ncbiBlastHspDensitySave(obj->ncbiBlastHspDensity, indent+2, f);
ncbiBlastHspQseqSave(obj->ncbiBlastHspQseq, indent+2, f);
ncbiBlastHspHseqSave(obj->ncbiBlastHspHseq, indent+2, f);
ncbiBlastHspMidlineSave(obj->ncbiBlastHspMidline, indent+2, f);
xapIndent(indent, f);
fprintf(f, "</Hsp>\n");
}

struct ncbiBlastHsp *ncbiBlastHspLoad(char *fileName)
/* Load ncbiBlastHsp from XML file where it is root element. */
{
struct ncbiBlastHsp *obj;
xapParseAny(fileName, "Hsp", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHsp *ncbiBlastHspLoadNext(struct xap *xap)
/* Load next ncbiBlastHsp element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp");
}

void ncbiBlastHspNumFree(struct ncbiBlastHspNum **pObj)
/* Free up ncbiBlastHspNum. */
{
struct ncbiBlastHspNum *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspNumFreeList(struct ncbiBlastHspNum **pList)
/* Free up list of ncbiBlastHspNum. */
{
struct ncbiBlastHspNum *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspNumFree(&el);
    el = next;
    }
}

void ncbiBlastHspNumSave(struct ncbiBlastHspNum *obj, int indent, FILE *f)
/* Save ncbiBlastHspNum to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_num>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_num>\n");
}

struct ncbiBlastHspNum *ncbiBlastHspNumLoad(char *fileName)
/* Load ncbiBlastHspNum from XML file where it is root element. */
{
struct ncbiBlastHspNum *obj;
xapParseAny(fileName, "Hsp_num", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspNum *ncbiBlastHspNumLoadNext(struct xap *xap)
/* Load next ncbiBlastHspNum element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_num");
}

void ncbiBlastHspBitScoreFree(struct ncbiBlastHspBitScore **pObj)
/* Free up ncbiBlastHspBitScore. */
{
struct ncbiBlastHspBitScore *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspBitScoreFreeList(struct ncbiBlastHspBitScore **pList)
/* Free up list of ncbiBlastHspBitScore. */
{
struct ncbiBlastHspBitScore *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspBitScoreFree(&el);
    el = next;
    }
}

void ncbiBlastHspBitScoreSave(struct ncbiBlastHspBitScore *obj, int indent, FILE *f)
/* Save ncbiBlastHspBitScore to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_bit-score>");
fprintf(f, "%f", obj->text);
fprintf(f, "</Hsp_bit-score>\n");
}

struct ncbiBlastHspBitScore *ncbiBlastHspBitScoreLoad(char *fileName)
/* Load ncbiBlastHspBitScore from XML file where it is root element. */
{
struct ncbiBlastHspBitScore *obj;
xapParseAny(fileName, "Hsp_bit-score", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspBitScore *ncbiBlastHspBitScoreLoadNext(struct xap *xap)
/* Load next ncbiBlastHspBitScore element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_bit-score");
}

void ncbiBlastHspScoreFree(struct ncbiBlastHspScore **pObj)
/* Free up ncbiBlastHspScore. */
{
struct ncbiBlastHspScore *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspScoreFreeList(struct ncbiBlastHspScore **pList)
/* Free up list of ncbiBlastHspScore. */
{
struct ncbiBlastHspScore *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspScoreFree(&el);
    el = next;
    }
}

void ncbiBlastHspScoreSave(struct ncbiBlastHspScore *obj, int indent, FILE *f)
/* Save ncbiBlastHspScore to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_score>");
fprintf(f, "%f", obj->text);
fprintf(f, "</Hsp_score>\n");
}

struct ncbiBlastHspScore *ncbiBlastHspScoreLoad(char *fileName)
/* Load ncbiBlastHspScore from XML file where it is root element. */
{
struct ncbiBlastHspScore *obj;
xapParseAny(fileName, "Hsp_score", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspScore *ncbiBlastHspScoreLoadNext(struct xap *xap)
/* Load next ncbiBlastHspScore element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_score");
}

void ncbiBlastHspEvalueFree(struct ncbiBlastHspEvalue **pObj)
/* Free up ncbiBlastHspEvalue. */
{
struct ncbiBlastHspEvalue *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspEvalueFreeList(struct ncbiBlastHspEvalue **pList)
/* Free up list of ncbiBlastHspEvalue. */
{
struct ncbiBlastHspEvalue *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspEvalueFree(&el);
    el = next;
    }
}

void ncbiBlastHspEvalueSave(struct ncbiBlastHspEvalue *obj, int indent, FILE *f)
/* Save ncbiBlastHspEvalue to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_evalue>");
fprintf(f, "%f", obj->text);
fprintf(f, "</Hsp_evalue>\n");
}

struct ncbiBlastHspEvalue *ncbiBlastHspEvalueLoad(char *fileName)
/* Load ncbiBlastHspEvalue from XML file where it is root element. */
{
struct ncbiBlastHspEvalue *obj;
xapParseAny(fileName, "Hsp_evalue", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspEvalue *ncbiBlastHspEvalueLoadNext(struct xap *xap)
/* Load next ncbiBlastHspEvalue element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_evalue");
}

void ncbiBlastHspQueryFromFree(struct ncbiBlastHspQueryFrom **pObj)
/* Free up ncbiBlastHspQueryFrom. */
{
struct ncbiBlastHspQueryFrom *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspQueryFromFreeList(struct ncbiBlastHspQueryFrom **pList)
/* Free up list of ncbiBlastHspQueryFrom. */
{
struct ncbiBlastHspQueryFrom *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspQueryFromFree(&el);
    el = next;
    }
}

void ncbiBlastHspQueryFromSave(struct ncbiBlastHspQueryFrom *obj, int indent, FILE *f)
/* Save ncbiBlastHspQueryFrom to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_query-from>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_query-from>\n");
}

struct ncbiBlastHspQueryFrom *ncbiBlastHspQueryFromLoad(char *fileName)
/* Load ncbiBlastHspQueryFrom from XML file where it is root element. */
{
struct ncbiBlastHspQueryFrom *obj;
xapParseAny(fileName, "Hsp_query-from", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspQueryFrom *ncbiBlastHspQueryFromLoadNext(struct xap *xap)
/* Load next ncbiBlastHspQueryFrom element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_query-from");
}

void ncbiBlastHspQueryToFree(struct ncbiBlastHspQueryTo **pObj)
/* Free up ncbiBlastHspQueryTo. */
{
struct ncbiBlastHspQueryTo *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspQueryToFreeList(struct ncbiBlastHspQueryTo **pList)
/* Free up list of ncbiBlastHspQueryTo. */
{
struct ncbiBlastHspQueryTo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspQueryToFree(&el);
    el = next;
    }
}

void ncbiBlastHspQueryToSave(struct ncbiBlastHspQueryTo *obj, int indent, FILE *f)
/* Save ncbiBlastHspQueryTo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_query-to>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_query-to>\n");
}

struct ncbiBlastHspQueryTo *ncbiBlastHspQueryToLoad(char *fileName)
/* Load ncbiBlastHspQueryTo from XML file where it is root element. */
{
struct ncbiBlastHspQueryTo *obj;
xapParseAny(fileName, "Hsp_query-to", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspQueryTo *ncbiBlastHspQueryToLoadNext(struct xap *xap)
/* Load next ncbiBlastHspQueryTo element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_query-to");
}

void ncbiBlastHspHitFromFree(struct ncbiBlastHspHitFrom **pObj)
/* Free up ncbiBlastHspHitFrom. */
{
struct ncbiBlastHspHitFrom *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspHitFromFreeList(struct ncbiBlastHspHitFrom **pList)
/* Free up list of ncbiBlastHspHitFrom. */
{
struct ncbiBlastHspHitFrom *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspHitFromFree(&el);
    el = next;
    }
}

void ncbiBlastHspHitFromSave(struct ncbiBlastHspHitFrom *obj, int indent, FILE *f)
/* Save ncbiBlastHspHitFrom to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_hit-from>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_hit-from>\n");
}

struct ncbiBlastHspHitFrom *ncbiBlastHspHitFromLoad(char *fileName)
/* Load ncbiBlastHspHitFrom from XML file where it is root element. */
{
struct ncbiBlastHspHitFrom *obj;
xapParseAny(fileName, "Hsp_hit-from", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspHitFrom *ncbiBlastHspHitFromLoadNext(struct xap *xap)
/* Load next ncbiBlastHspHitFrom element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_hit-from");
}

void ncbiBlastHspHitToFree(struct ncbiBlastHspHitTo **pObj)
/* Free up ncbiBlastHspHitTo. */
{
struct ncbiBlastHspHitTo *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspHitToFreeList(struct ncbiBlastHspHitTo **pList)
/* Free up list of ncbiBlastHspHitTo. */
{
struct ncbiBlastHspHitTo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspHitToFree(&el);
    el = next;
    }
}

void ncbiBlastHspHitToSave(struct ncbiBlastHspHitTo *obj, int indent, FILE *f)
/* Save ncbiBlastHspHitTo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_hit-to>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_hit-to>\n");
}

struct ncbiBlastHspHitTo *ncbiBlastHspHitToLoad(char *fileName)
/* Load ncbiBlastHspHitTo from XML file where it is root element. */
{
struct ncbiBlastHspHitTo *obj;
xapParseAny(fileName, "Hsp_hit-to", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspHitTo *ncbiBlastHspHitToLoadNext(struct xap *xap)
/* Load next ncbiBlastHspHitTo element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_hit-to");
}

void ncbiBlastHspPatternFromFree(struct ncbiBlastHspPatternFrom **pObj)
/* Free up ncbiBlastHspPatternFrom. */
{
struct ncbiBlastHspPatternFrom *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspPatternFromFreeList(struct ncbiBlastHspPatternFrom **pList)
/* Free up list of ncbiBlastHspPatternFrom. */
{
struct ncbiBlastHspPatternFrom *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspPatternFromFree(&el);
    el = next;
    }
}

void ncbiBlastHspPatternFromSave(struct ncbiBlastHspPatternFrom *obj, int indent, FILE *f)
/* Save ncbiBlastHspPatternFrom to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_pattern-from>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_pattern-from>\n");
}

struct ncbiBlastHspPatternFrom *ncbiBlastHspPatternFromLoad(char *fileName)
/* Load ncbiBlastHspPatternFrom from XML file where it is root element. */
{
struct ncbiBlastHspPatternFrom *obj;
xapParseAny(fileName, "Hsp_pattern-from", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspPatternFrom *ncbiBlastHspPatternFromLoadNext(struct xap *xap)
/* Load next ncbiBlastHspPatternFrom element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_pattern-from");
}

void ncbiBlastHspPatternToFree(struct ncbiBlastHspPatternTo **pObj)
/* Free up ncbiBlastHspPatternTo. */
{
struct ncbiBlastHspPatternTo *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspPatternToFreeList(struct ncbiBlastHspPatternTo **pList)
/* Free up list of ncbiBlastHspPatternTo. */
{
struct ncbiBlastHspPatternTo *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspPatternToFree(&el);
    el = next;
    }
}

void ncbiBlastHspPatternToSave(struct ncbiBlastHspPatternTo *obj, int indent, FILE *f)
/* Save ncbiBlastHspPatternTo to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_pattern-to>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_pattern-to>\n");
}

struct ncbiBlastHspPatternTo *ncbiBlastHspPatternToLoad(char *fileName)
/* Load ncbiBlastHspPatternTo from XML file where it is root element. */
{
struct ncbiBlastHspPatternTo *obj;
xapParseAny(fileName, "Hsp_pattern-to", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspPatternTo *ncbiBlastHspPatternToLoadNext(struct xap *xap)
/* Load next ncbiBlastHspPatternTo element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_pattern-to");
}

void ncbiBlastHspQueryFrameFree(struct ncbiBlastHspQueryFrame **pObj)
/* Free up ncbiBlastHspQueryFrame. */
{
struct ncbiBlastHspQueryFrame *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspQueryFrameFreeList(struct ncbiBlastHspQueryFrame **pList)
/* Free up list of ncbiBlastHspQueryFrame. */
{
struct ncbiBlastHspQueryFrame *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspQueryFrameFree(&el);
    el = next;
    }
}

void ncbiBlastHspQueryFrameSave(struct ncbiBlastHspQueryFrame *obj, int indent, FILE *f)
/* Save ncbiBlastHspQueryFrame to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_query-frame>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_query-frame>\n");
}

struct ncbiBlastHspQueryFrame *ncbiBlastHspQueryFrameLoad(char *fileName)
/* Load ncbiBlastHspQueryFrame from XML file where it is root element. */
{
struct ncbiBlastHspQueryFrame *obj;
xapParseAny(fileName, "Hsp_query-frame", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspQueryFrame *ncbiBlastHspQueryFrameLoadNext(struct xap *xap)
/* Load next ncbiBlastHspQueryFrame element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_query-frame");
}

void ncbiBlastHspHitFrameFree(struct ncbiBlastHspHitFrame **pObj)
/* Free up ncbiBlastHspHitFrame. */
{
struct ncbiBlastHspHitFrame *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspHitFrameFreeList(struct ncbiBlastHspHitFrame **pList)
/* Free up list of ncbiBlastHspHitFrame. */
{
struct ncbiBlastHspHitFrame *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspHitFrameFree(&el);
    el = next;
    }
}

void ncbiBlastHspHitFrameSave(struct ncbiBlastHspHitFrame *obj, int indent, FILE *f)
/* Save ncbiBlastHspHitFrame to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_hit-frame>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_hit-frame>\n");
}

struct ncbiBlastHspHitFrame *ncbiBlastHspHitFrameLoad(char *fileName)
/* Load ncbiBlastHspHitFrame from XML file where it is root element. */
{
struct ncbiBlastHspHitFrame *obj;
xapParseAny(fileName, "Hsp_hit-frame", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspHitFrame *ncbiBlastHspHitFrameLoadNext(struct xap *xap)
/* Load next ncbiBlastHspHitFrame element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_hit-frame");
}

void ncbiBlastHspIdentityFree(struct ncbiBlastHspIdentity **pObj)
/* Free up ncbiBlastHspIdentity. */
{
struct ncbiBlastHspIdentity *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspIdentityFreeList(struct ncbiBlastHspIdentity **pList)
/* Free up list of ncbiBlastHspIdentity. */
{
struct ncbiBlastHspIdentity *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspIdentityFree(&el);
    el = next;
    }
}

void ncbiBlastHspIdentitySave(struct ncbiBlastHspIdentity *obj, int indent, FILE *f)
/* Save ncbiBlastHspIdentity to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_identity>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_identity>\n");
}

struct ncbiBlastHspIdentity *ncbiBlastHspIdentityLoad(char *fileName)
/* Load ncbiBlastHspIdentity from XML file where it is root element. */
{
struct ncbiBlastHspIdentity *obj;
xapParseAny(fileName, "Hsp_identity", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspIdentity *ncbiBlastHspIdentityLoadNext(struct xap *xap)
/* Load next ncbiBlastHspIdentity element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_identity");
}

void ncbiBlastHspPositiveFree(struct ncbiBlastHspPositive **pObj)
/* Free up ncbiBlastHspPositive. */
{
struct ncbiBlastHspPositive *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspPositiveFreeList(struct ncbiBlastHspPositive **pList)
/* Free up list of ncbiBlastHspPositive. */
{
struct ncbiBlastHspPositive *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspPositiveFree(&el);
    el = next;
    }
}

void ncbiBlastHspPositiveSave(struct ncbiBlastHspPositive *obj, int indent, FILE *f)
/* Save ncbiBlastHspPositive to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_positive>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_positive>\n");
}

struct ncbiBlastHspPositive *ncbiBlastHspPositiveLoad(char *fileName)
/* Load ncbiBlastHspPositive from XML file where it is root element. */
{
struct ncbiBlastHspPositive *obj;
xapParseAny(fileName, "Hsp_positive", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspPositive *ncbiBlastHspPositiveLoadNext(struct xap *xap)
/* Load next ncbiBlastHspPositive element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_positive");
}

void ncbiBlastHspGapsFree(struct ncbiBlastHspGaps **pObj)
/* Free up ncbiBlastHspGaps. */
{
struct ncbiBlastHspGaps *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspGapsFreeList(struct ncbiBlastHspGaps **pList)
/* Free up list of ncbiBlastHspGaps. */
{
struct ncbiBlastHspGaps *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspGapsFree(&el);
    el = next;
    }
}

void ncbiBlastHspGapsSave(struct ncbiBlastHspGaps *obj, int indent, FILE *f)
/* Save ncbiBlastHspGaps to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_gaps>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_gaps>\n");
}

struct ncbiBlastHspGaps *ncbiBlastHspGapsLoad(char *fileName)
/* Load ncbiBlastHspGaps from XML file where it is root element. */
{
struct ncbiBlastHspGaps *obj;
xapParseAny(fileName, "Hsp_gaps", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspGaps *ncbiBlastHspGapsLoadNext(struct xap *xap)
/* Load next ncbiBlastHspGaps element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_gaps");
}

void ncbiBlastHspAlignLenFree(struct ncbiBlastHspAlignLen **pObj)
/* Free up ncbiBlastHspAlignLen. */
{
struct ncbiBlastHspAlignLen *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspAlignLenFreeList(struct ncbiBlastHspAlignLen **pList)
/* Free up list of ncbiBlastHspAlignLen. */
{
struct ncbiBlastHspAlignLen *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspAlignLenFree(&el);
    el = next;
    }
}

void ncbiBlastHspAlignLenSave(struct ncbiBlastHspAlignLen *obj, int indent, FILE *f)
/* Save ncbiBlastHspAlignLen to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_align-len>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_align-len>\n");
}

struct ncbiBlastHspAlignLen *ncbiBlastHspAlignLenLoad(char *fileName)
/* Load ncbiBlastHspAlignLen from XML file where it is root element. */
{
struct ncbiBlastHspAlignLen *obj;
xapParseAny(fileName, "Hsp_align-len", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspAlignLen *ncbiBlastHspAlignLenLoadNext(struct xap *xap)
/* Load next ncbiBlastHspAlignLen element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_align-len");
}

void ncbiBlastHspDensityFree(struct ncbiBlastHspDensity **pObj)
/* Free up ncbiBlastHspDensity. */
{
struct ncbiBlastHspDensity *obj = *pObj;
if (obj == NULL) return;
freez(pObj);
}

void ncbiBlastHspDensityFreeList(struct ncbiBlastHspDensity **pList)
/* Free up list of ncbiBlastHspDensity. */
{
struct ncbiBlastHspDensity *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspDensityFree(&el);
    el = next;
    }
}

void ncbiBlastHspDensitySave(struct ncbiBlastHspDensity *obj, int indent, FILE *f)
/* Save ncbiBlastHspDensity to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_density>");
fprintf(f, "%d", obj->text);
fprintf(f, "</Hsp_density>\n");
}

struct ncbiBlastHspDensity *ncbiBlastHspDensityLoad(char *fileName)
/* Load ncbiBlastHspDensity from XML file where it is root element. */
{
struct ncbiBlastHspDensity *obj;
xapParseAny(fileName, "Hsp_density", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspDensity *ncbiBlastHspDensityLoadNext(struct xap *xap)
/* Load next ncbiBlastHspDensity element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_density");
}

void ncbiBlastHspQseqFree(struct ncbiBlastHspQseq **pObj)
/* Free up ncbiBlastHspQseq. */
{
struct ncbiBlastHspQseq *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastHspQseqFreeList(struct ncbiBlastHspQseq **pList)
/* Free up list of ncbiBlastHspQseq. */
{
struct ncbiBlastHspQseq *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspQseqFree(&el);
    el = next;
    }
}

void ncbiBlastHspQseqSave(struct ncbiBlastHspQseq *obj, int indent, FILE *f)
/* Save ncbiBlastHspQseq to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_qseq>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Hsp_qseq>\n");
}

struct ncbiBlastHspQseq *ncbiBlastHspQseqLoad(char *fileName)
/* Load ncbiBlastHspQseq from XML file where it is root element. */
{
struct ncbiBlastHspQseq *obj;
xapParseAny(fileName, "Hsp_qseq", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspQseq *ncbiBlastHspQseqLoadNext(struct xap *xap)
/* Load next ncbiBlastHspQseq element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_qseq");
}

void ncbiBlastHspHseqFree(struct ncbiBlastHspHseq **pObj)
/* Free up ncbiBlastHspHseq. */
{
struct ncbiBlastHspHseq *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastHspHseqFreeList(struct ncbiBlastHspHseq **pList)
/* Free up list of ncbiBlastHspHseq. */
{
struct ncbiBlastHspHseq *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspHseqFree(&el);
    el = next;
    }
}

void ncbiBlastHspHseqSave(struct ncbiBlastHspHseq *obj, int indent, FILE *f)
/* Save ncbiBlastHspHseq to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_hseq>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Hsp_hseq>\n");
}

struct ncbiBlastHspHseq *ncbiBlastHspHseqLoad(char *fileName)
/* Load ncbiBlastHspHseq from XML file where it is root element. */
{
struct ncbiBlastHspHseq *obj;
xapParseAny(fileName, "Hsp_hseq", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspHseq *ncbiBlastHspHseqLoadNext(struct xap *xap)
/* Load next ncbiBlastHspHseq element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_hseq");
}

void ncbiBlastHspMidlineFree(struct ncbiBlastHspMidline **pObj)
/* Free up ncbiBlastHspMidline. */
{
struct ncbiBlastHspMidline *obj = *pObj;
if (obj == NULL) return;
freeMem(obj->text);
freez(pObj);
}

void ncbiBlastHspMidlineFreeList(struct ncbiBlastHspMidline **pList)
/* Free up list of ncbiBlastHspMidline. */
{
struct ncbiBlastHspMidline *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    ncbiBlastHspMidlineFree(&el);
    el = next;
    }
}

void ncbiBlastHspMidlineSave(struct ncbiBlastHspMidline *obj, int indent, FILE *f)
/* Save ncbiBlastHspMidline to file. */
{
if (obj == NULL) return;
xapIndent(indent, f);
fprintf(f, "<Hsp_midline>");
fprintf(f, "%s", obj->text);
fprintf(f, "</Hsp_midline>\n");
}

struct ncbiBlastHspMidline *ncbiBlastHspMidlineLoad(char *fileName)
/* Load ncbiBlastHspMidline from XML file where it is root element. */
{
struct ncbiBlastHspMidline *obj;
xapParseAny(fileName, "Hsp_midline", ncbiBlastStartHandler, ncbiBlastEndHandler, NULL, &obj);
return obj;
}

struct ncbiBlastHspMidline *ncbiBlastHspMidlineLoadNext(struct xap *xap)
/* Load next ncbiBlastHspMidline element.  Use xapOpen to get xap. */
{
return xapNext(xap, "Hsp_midline");
}

void *ncbiBlastStartHandler(struct xap *xp, char *name, char **atts)
/* Called by xap with start tag.  Does most of the parsing work. */
{
struct xapStack *st = xp->stack+1;
int depth = xp->stackDepth;

if (sameString(name, "BlastOutput"))
    {
    struct ncbiBlastBlastOutput *obj;
    AllocVar(obj);
    return obj;
    }
else if (sameString(name, "BlastOutput_program"))
    {
    struct ncbiBlastBlastOutputProgram *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputProgram, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_version"))
    {
    struct ncbiBlastBlastOutputVersion *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputVersion, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_reference"))
    {
    struct ncbiBlastBlastOutputReference *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputReference, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_db"))
    {
    struct ncbiBlastBlastOutputDb *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputDb, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_query-ID"))
    {
    struct ncbiBlastBlastOutputQueryID *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputQueryID, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_query-def"))
    {
    struct ncbiBlastBlastOutputQueryDef *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputQueryDef, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_query-len"))
    {
    struct ncbiBlastBlastOutputQueryLen *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputQueryLen, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_query-seq"))
    {
    struct ncbiBlastBlastOutputQuerySeq *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputQuerySeq, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_param"))
    {
    struct ncbiBlastBlastOutputParam *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputParam, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_iterations"))
    {
    struct ncbiBlastBlastOutputIterations *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputIterations, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "BlastOutput_mbstat"))
    {
    struct ncbiBlastBlastOutputMbstat *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput"))
            {
            struct ncbiBlastBlastOutput *parent = st->object;
            slAddHead(&parent->ncbiBlastBlastOutputMbstat, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Iteration"))
    {
    struct ncbiBlastIteration *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput_iterations"))
            {
            struct ncbiBlastBlastOutputIterations *parent = st->object;
            slAddHead(&parent->ncbiBlastIteration, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Iteration_iter-num"))
    {
    struct ncbiBlastIterationIterNum *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Iteration"))
            {
            struct ncbiBlastIteration *parent = st->object;
            slAddHead(&parent->ncbiBlastIterationIterNum, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Iteration_query-ID"))
    {
    struct ncbiBlastIterationQueryID *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Iteration"))
            {
            struct ncbiBlastIteration *parent = st->object;
            slAddHead(&parent->ncbiBlastIterationQueryID, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Iteration_query-def"))
    {
    struct ncbiBlastIterationQueryDef *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Iteration"))
            {
            struct ncbiBlastIteration *parent = st->object;
            slAddHead(&parent->ncbiBlastIterationQueryDef, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Iteration_query-len"))
    {
    struct ncbiBlastIterationQueryLen *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Iteration"))
            {
            struct ncbiBlastIteration *parent = st->object;
            slAddHead(&parent->ncbiBlastIterationQueryLen, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Iteration_hits"))
    {
    struct ncbiBlastIterationHits *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Iteration"))
            {
            struct ncbiBlastIteration *parent = st->object;
            slAddHead(&parent->ncbiBlastIterationHits, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Iteration_stat"))
    {
    struct ncbiBlastIterationStat *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Iteration"))
            {
            struct ncbiBlastIteration *parent = st->object;
            slAddHead(&parent->ncbiBlastIterationStat, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Iteration_message"))
    {
    struct ncbiBlastIterationMessage *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Iteration"))
            {
            struct ncbiBlastIteration *parent = st->object;
            slAddHead(&parent->ncbiBlastIterationMessage, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters"))
    {
    struct ncbiBlastParameters *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput_param"))
            {
            struct ncbiBlastBlastOutputParam *parent = st->object;
            slAddHead(&parent->ncbiBlastParameters, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_matrix"))
    {
    struct ncbiBlastParametersMatrix *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersMatrix, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_expect"))
    {
    struct ncbiBlastParametersExpect *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersExpect, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_include"))
    {
    struct ncbiBlastParametersInclude *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersInclude, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_sc-match"))
    {
    struct ncbiBlastParametersScMatch *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersScMatch, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_sc-mismatch"))
    {
    struct ncbiBlastParametersScMismatch *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersScMismatch, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_gap-open"))
    {
    struct ncbiBlastParametersGapOpen *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersGapOpen, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_gap-extend"))
    {
    struct ncbiBlastParametersGapExtend *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersGapExtend, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_filter"))
    {
    struct ncbiBlastParametersFilter *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersFilter, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_pattern"))
    {
    struct ncbiBlastParametersPattern *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersPattern, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Parameters_entrez-query"))
    {
    struct ncbiBlastParametersEntrezQuery *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Parameters"))
            {
            struct ncbiBlastParameters *parent = st->object;
            slAddHead(&parent->ncbiBlastParametersEntrezQuery, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Statistics"))
    {
    struct ncbiBlastStatistics *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "BlastOutput_mbstat"))
            {
            struct ncbiBlastBlastOutputMbstat *parent = st->object;
            slAddHead(&parent->ncbiBlastStatistics, obj);
            }
        else if (sameString(st->elName, "Iteration_stat"))
            {
            struct ncbiBlastIterationStat *parent = st->object;
            slAddHead(&parent->ncbiBlastStatistics, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Statistics_db-num"))
    {
    struct ncbiBlastStatisticsDbNum *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Statistics"))
            {
            struct ncbiBlastStatistics *parent = st->object;
            slAddHead(&parent->ncbiBlastStatisticsDbNum, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Statistics_db-len"))
    {
    struct ncbiBlastStatisticsDbLen *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Statistics"))
            {
            struct ncbiBlastStatistics *parent = st->object;
            slAddHead(&parent->ncbiBlastStatisticsDbLen, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Statistics_hsp-len"))
    {
    struct ncbiBlastStatisticsHspLen *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Statistics"))
            {
            struct ncbiBlastStatistics *parent = st->object;
            slAddHead(&parent->ncbiBlastStatisticsHspLen, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Statistics_eff-space"))
    {
    struct ncbiBlastStatisticsEffSpace *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Statistics"))
            {
            struct ncbiBlastStatistics *parent = st->object;
            slAddHead(&parent->ncbiBlastStatisticsEffSpace, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Statistics_kappa"))
    {
    struct ncbiBlastStatisticsKappa *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Statistics"))
            {
            struct ncbiBlastStatistics *parent = st->object;
            slAddHead(&parent->ncbiBlastStatisticsKappa, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Statistics_lambda"))
    {
    struct ncbiBlastStatisticsLambda *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Statistics"))
            {
            struct ncbiBlastStatistics *parent = st->object;
            slAddHead(&parent->ncbiBlastStatisticsLambda, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Statistics_entropy"))
    {
    struct ncbiBlastStatisticsEntropy *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Statistics"))
            {
            struct ncbiBlastStatistics *parent = st->object;
            slAddHead(&parent->ncbiBlastStatisticsEntropy, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hit"))
    {
    struct ncbiBlastHit *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Iteration_hits"))
            {
            struct ncbiBlastIterationHits *parent = st->object;
            slAddHead(&parent->ncbiBlastHit, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hit_num"))
    {
    struct ncbiBlastHitNum *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hit"))
            {
            struct ncbiBlastHit *parent = st->object;
            slAddHead(&parent->ncbiBlastHitNum, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hit_id"))
    {
    struct ncbiBlastHitId *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hit"))
            {
            struct ncbiBlastHit *parent = st->object;
            slAddHead(&parent->ncbiBlastHitId, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hit_def"))
    {
    struct ncbiBlastHitDef *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hit"))
            {
            struct ncbiBlastHit *parent = st->object;
            slAddHead(&parent->ncbiBlastHitDef, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hit_accession"))
    {
    struct ncbiBlastHitAccession *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hit"))
            {
            struct ncbiBlastHit *parent = st->object;
            slAddHead(&parent->ncbiBlastHitAccession, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hit_len"))
    {
    struct ncbiBlastHitLen *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hit"))
            {
            struct ncbiBlastHit *parent = st->object;
            slAddHead(&parent->ncbiBlastHitLen, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hit_hsps"))
    {
    struct ncbiBlastHitHsps *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hit"))
            {
            struct ncbiBlastHit *parent = st->object;
            slAddHead(&parent->ncbiBlastHitHsps, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp"))
    {
    struct ncbiBlastHsp *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hit_hsps"))
            {
            struct ncbiBlastHitHsps *parent = st->object;
            slAddHead(&parent->ncbiBlastHsp, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_num"))
    {
    struct ncbiBlastHspNum *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspNum, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_bit-score"))
    {
    struct ncbiBlastHspBitScore *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspBitScore, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_score"))
    {
    struct ncbiBlastHspScore *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspScore, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_evalue"))
    {
    struct ncbiBlastHspEvalue *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspEvalue, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_query-from"))
    {
    struct ncbiBlastHspQueryFrom *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspQueryFrom, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_query-to"))
    {
    struct ncbiBlastHspQueryTo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspQueryTo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_hit-from"))
    {
    struct ncbiBlastHspHitFrom *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspHitFrom, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_hit-to"))
    {
    struct ncbiBlastHspHitTo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspHitTo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_pattern-from"))
    {
    struct ncbiBlastHspPatternFrom *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspPatternFrom, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_pattern-to"))
    {
    struct ncbiBlastHspPatternTo *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspPatternTo, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_query-frame"))
    {
    struct ncbiBlastHspQueryFrame *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspQueryFrame, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_hit-frame"))
    {
    struct ncbiBlastHspHitFrame *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspHitFrame, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_identity"))
    {
    struct ncbiBlastHspIdentity *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspIdentity, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_positive"))
    {
    struct ncbiBlastHspPositive *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspPositive, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_gaps"))
    {
    struct ncbiBlastHspGaps *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspGaps, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_align-len"))
    {
    struct ncbiBlastHspAlignLen *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspAlignLen, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_density"))
    {
    struct ncbiBlastHspDensity *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspDensity, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_qseq"))
    {
    struct ncbiBlastHspQseq *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspQseq, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_hseq"))
    {
    struct ncbiBlastHspHseq *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspHseq, obj);
            }
        }
    return obj;
    }
else if (sameString(name, "Hsp_midline"))
    {
    struct ncbiBlastHspMidline *obj;
    AllocVar(obj);
    if (depth > 1)
        {
        if  (sameString(st->elName, "Hsp"))
            {
            struct ncbiBlastHsp *parent = st->object;
            slAddHead(&parent->ncbiBlastHspMidline, obj);
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

void ncbiBlastEndHandler(struct xap *xp, char *name)
/* Called by xap with end tag.  Checks all required children are loaded. */
{
struct xapStack *stack = xp->stack;
if (sameString(name, "BlastOutput"))
    {
    struct ncbiBlastBlastOutput *obj = stack->object;
    if (obj->ncbiBlastBlastOutputProgram == NULL)
        xapError(xp, "Missing BlastOutput_program");
    if (obj->ncbiBlastBlastOutputProgram->next != NULL)
        xapError(xp, "Multiple BlastOutput_program");
    if (obj->ncbiBlastBlastOutputVersion == NULL)
        xapError(xp, "Missing BlastOutput_version");
    if (obj->ncbiBlastBlastOutputVersion->next != NULL)
        xapError(xp, "Multiple BlastOutput_version");
    if (obj->ncbiBlastBlastOutputReference == NULL)
        xapError(xp, "Missing BlastOutput_reference");
    if (obj->ncbiBlastBlastOutputReference->next != NULL)
        xapError(xp, "Multiple BlastOutput_reference");
    if (obj->ncbiBlastBlastOutputDb == NULL)
        xapError(xp, "Missing BlastOutput_db");
    if (obj->ncbiBlastBlastOutputDb->next != NULL)
        xapError(xp, "Multiple BlastOutput_db");
    if (obj->ncbiBlastBlastOutputQueryID == NULL)
        xapError(xp, "Missing BlastOutput_query-ID");
    if (obj->ncbiBlastBlastOutputQueryID->next != NULL)
        xapError(xp, "Multiple BlastOutput_query-ID");
    if (obj->ncbiBlastBlastOutputQueryDef == NULL)
        xapError(xp, "Missing BlastOutput_query-def");
    if (obj->ncbiBlastBlastOutputQueryDef->next != NULL)
        xapError(xp, "Multiple BlastOutput_query-def");
    if (obj->ncbiBlastBlastOutputQueryLen == NULL)
        xapError(xp, "Missing BlastOutput_query-len");
    if (obj->ncbiBlastBlastOutputQueryLen->next != NULL)
        xapError(xp, "Multiple BlastOutput_query-len");
    if (obj->ncbiBlastBlastOutputQuerySeq != NULL && obj->ncbiBlastBlastOutputQuerySeq->next != NULL)
        xapError(xp, "Multiple BlastOutput_query-seq");
    if (obj->ncbiBlastBlastOutputParam == NULL)
        xapError(xp, "Missing BlastOutput_param");
    if (obj->ncbiBlastBlastOutputParam->next != NULL)
        xapError(xp, "Multiple BlastOutput_param");
    if (obj->ncbiBlastBlastOutputIterations == NULL)
        xapError(xp, "Missing BlastOutput_iterations");
    if (obj->ncbiBlastBlastOutputIterations->next != NULL)
        xapError(xp, "Multiple BlastOutput_iterations");
    if (obj->ncbiBlastBlastOutputMbstat != NULL && obj->ncbiBlastBlastOutputMbstat->next != NULL)
        xapError(xp, "Multiple BlastOutput_mbstat");
    }
else if (sameString(name, "BlastOutput_program"))
    {
    struct ncbiBlastBlastOutputProgram *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "BlastOutput_version"))
    {
    struct ncbiBlastBlastOutputVersion *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "BlastOutput_reference"))
    {
    struct ncbiBlastBlastOutputReference *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "BlastOutput_db"))
    {
    struct ncbiBlastBlastOutputDb *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "BlastOutput_query-ID"))
    {
    struct ncbiBlastBlastOutputQueryID *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "BlastOutput_query-def"))
    {
    struct ncbiBlastBlastOutputQueryDef *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "BlastOutput_query-len"))
    {
    struct ncbiBlastBlastOutputQueryLen *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "BlastOutput_query-seq"))
    {
    struct ncbiBlastBlastOutputQuerySeq *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "BlastOutput_param"))
    {
    struct ncbiBlastBlastOutputParam *obj = stack->object;
    if (obj->ncbiBlastParameters == NULL)
        xapError(xp, "Missing Parameters");
    if (obj->ncbiBlastParameters->next != NULL)
        xapError(xp, "Multiple Parameters");
    }
else if (sameString(name, "BlastOutput_iterations"))
    {
    struct ncbiBlastBlastOutputIterations *obj = stack->object;
    slReverse(&obj->ncbiBlastIteration);
    }
else if (sameString(name, "BlastOutput_mbstat"))
    {
    struct ncbiBlastBlastOutputMbstat *obj = stack->object;
    if (obj->ncbiBlastStatistics == NULL)
        xapError(xp, "Missing Statistics");
    if (obj->ncbiBlastStatistics->next != NULL)
        xapError(xp, "Multiple Statistics");
    }
else if (sameString(name, "Iteration"))
    {
    struct ncbiBlastIteration *obj = stack->object;
    if (obj->ncbiBlastIterationIterNum == NULL)
        xapError(xp, "Missing Iteration_iter-num");
    if (obj->ncbiBlastIterationIterNum->next != NULL)
        xapError(xp, "Multiple Iteration_iter-num");
    if (obj->ncbiBlastIterationQueryID != NULL && obj->ncbiBlastIterationQueryID->next != NULL)
        xapError(xp, "Multiple Iteration_query-ID");
    if (obj->ncbiBlastIterationQueryDef != NULL && obj->ncbiBlastIterationQueryDef->next != NULL)
        xapError(xp, "Multiple Iteration_query-def");
    if (obj->ncbiBlastIterationQueryLen != NULL && obj->ncbiBlastIterationQueryLen->next != NULL)
        xapError(xp, "Multiple Iteration_query-len");
    if (obj->ncbiBlastIterationHits != NULL && obj->ncbiBlastIterationHits->next != NULL)
        xapError(xp, "Multiple Iteration_hits");
    if (obj->ncbiBlastIterationStat != NULL && obj->ncbiBlastIterationStat->next != NULL)
        xapError(xp, "Multiple Iteration_stat");
    if (obj->ncbiBlastIterationMessage != NULL && obj->ncbiBlastIterationMessage->next != NULL)
        xapError(xp, "Multiple Iteration_message");
    }
else if (sameString(name, "Iteration_iter-num"))
    {
    struct ncbiBlastIterationIterNum *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Iteration_query-ID"))
    {
    struct ncbiBlastIterationQueryID *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Iteration_query-def"))
    {
    struct ncbiBlastIterationQueryDef *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Iteration_query-len"))
    {
    struct ncbiBlastIterationQueryLen *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Iteration_hits"))
    {
    struct ncbiBlastIterationHits *obj = stack->object;
    slReverse(&obj->ncbiBlastHit);
    }
else if (sameString(name, "Iteration_stat"))
    {
    struct ncbiBlastIterationStat *obj = stack->object;
    if (obj->ncbiBlastStatistics == NULL)
        xapError(xp, "Missing Statistics");
    if (obj->ncbiBlastStatistics->next != NULL)
        xapError(xp, "Multiple Statistics");
    }
else if (sameString(name, "Iteration_message"))
    {
    struct ncbiBlastIterationMessage *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Parameters"))
    {
    struct ncbiBlastParameters *obj = stack->object;
    if (obj->ncbiBlastParametersMatrix != NULL && obj->ncbiBlastParametersMatrix->next != NULL)
        xapError(xp, "Multiple Parameters_matrix");
    if (obj->ncbiBlastParametersExpect == NULL)
        xapError(xp, "Missing Parameters_expect");
    if (obj->ncbiBlastParametersExpect->next != NULL)
        xapError(xp, "Multiple Parameters_expect");
    if (obj->ncbiBlastParametersInclude != NULL && obj->ncbiBlastParametersInclude->next != NULL)
        xapError(xp, "Multiple Parameters_include");
    if (obj->ncbiBlastParametersScMatch != NULL && obj->ncbiBlastParametersScMatch->next != NULL)
        xapError(xp, "Multiple Parameters_sc-match");
    if (obj->ncbiBlastParametersScMismatch != NULL && obj->ncbiBlastParametersScMismatch->next != NULL)
        xapError(xp, "Multiple Parameters_sc-mismatch");
    if (obj->ncbiBlastParametersGapOpen == NULL)
        xapError(xp, "Missing Parameters_gap-open");
    if (obj->ncbiBlastParametersGapOpen->next != NULL)
        xapError(xp, "Multiple Parameters_gap-open");
    if (obj->ncbiBlastParametersGapExtend == NULL)
        xapError(xp, "Missing Parameters_gap-extend");
    if (obj->ncbiBlastParametersGapExtend->next != NULL)
        xapError(xp, "Multiple Parameters_gap-extend");
    if (obj->ncbiBlastParametersFilter != NULL && obj->ncbiBlastParametersFilter->next != NULL)
        xapError(xp, "Multiple Parameters_filter");
    if (obj->ncbiBlastParametersPattern != NULL && obj->ncbiBlastParametersPattern->next != NULL)
        xapError(xp, "Multiple Parameters_pattern");
    if (obj->ncbiBlastParametersEntrezQuery != NULL && obj->ncbiBlastParametersEntrezQuery->next != NULL)
        xapError(xp, "Multiple Parameters_entrez-query");
    }
else if (sameString(name, "Parameters_matrix"))
    {
    struct ncbiBlastParametersMatrix *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Parameters_expect"))
    {
    struct ncbiBlastParametersExpect *obj = stack->object;
    obj->text = sqlDouble(stack->text->string);
    }
else if (sameString(name, "Parameters_include"))
    {
    struct ncbiBlastParametersInclude *obj = stack->object;
    obj->text = sqlDouble(stack->text->string);
    }
else if (sameString(name, "Parameters_sc-match"))
    {
    struct ncbiBlastParametersScMatch *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Parameters_sc-mismatch"))
    {
    struct ncbiBlastParametersScMismatch *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Parameters_gap-open"))
    {
    struct ncbiBlastParametersGapOpen *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Parameters_gap-extend"))
    {
    struct ncbiBlastParametersGapExtend *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Parameters_filter"))
    {
    struct ncbiBlastParametersFilter *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Parameters_pattern"))
    {
    struct ncbiBlastParametersPattern *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Parameters_entrez-query"))
    {
    struct ncbiBlastParametersEntrezQuery *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Statistics"))
    {
    struct ncbiBlastStatistics *obj = stack->object;
    if (obj->ncbiBlastStatisticsDbNum == NULL)
        xapError(xp, "Missing Statistics_db-num");
    if (obj->ncbiBlastStatisticsDbNum->next != NULL)
        xapError(xp, "Multiple Statistics_db-num");
    if (obj->ncbiBlastStatisticsDbLen == NULL)
        xapError(xp, "Missing Statistics_db-len");
    if (obj->ncbiBlastStatisticsDbLen->next != NULL)
        xapError(xp, "Multiple Statistics_db-len");
    if (obj->ncbiBlastStatisticsHspLen == NULL)
        xapError(xp, "Missing Statistics_hsp-len");
    if (obj->ncbiBlastStatisticsHspLen->next != NULL)
        xapError(xp, "Multiple Statistics_hsp-len");
    if (obj->ncbiBlastStatisticsEffSpace == NULL)
        xapError(xp, "Missing Statistics_eff-space");
    if (obj->ncbiBlastStatisticsEffSpace->next != NULL)
        xapError(xp, "Multiple Statistics_eff-space");
    if (obj->ncbiBlastStatisticsKappa == NULL)
        xapError(xp, "Missing Statistics_kappa");
    if (obj->ncbiBlastStatisticsKappa->next != NULL)
        xapError(xp, "Multiple Statistics_kappa");
    if (obj->ncbiBlastStatisticsLambda == NULL)
        xapError(xp, "Missing Statistics_lambda");
    if (obj->ncbiBlastStatisticsLambda->next != NULL)
        xapError(xp, "Multiple Statistics_lambda");
    if (obj->ncbiBlastStatisticsEntropy == NULL)
        xapError(xp, "Missing Statistics_entropy");
    if (obj->ncbiBlastStatisticsEntropy->next != NULL)
        xapError(xp, "Multiple Statistics_entropy");
    }
else if (sameString(name, "Statistics_db-num"))
    {
    struct ncbiBlastStatisticsDbNum *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Statistics_db-len"))
    {
    struct ncbiBlastStatisticsDbLen *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Statistics_hsp-len"))
    {
    struct ncbiBlastStatisticsHspLen *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Statistics_eff-space"))
    {
    struct ncbiBlastStatisticsEffSpace *obj = stack->object;
    obj->text = sqlDouble(stack->text->string);
    }
else if (sameString(name, "Statistics_kappa"))
    {
    struct ncbiBlastStatisticsKappa *obj = stack->object;
    obj->text = sqlDouble(stack->text->string);
    }
else if (sameString(name, "Statistics_lambda"))
    {
    struct ncbiBlastStatisticsLambda *obj = stack->object;
    obj->text = sqlDouble(stack->text->string);
    }
else if (sameString(name, "Statistics_entropy"))
    {
    struct ncbiBlastStatisticsEntropy *obj = stack->object;
    obj->text = sqlDouble(stack->text->string);
    }
else if (sameString(name, "Hit"))
    {
    struct ncbiBlastHit *obj = stack->object;
    if (obj->ncbiBlastHitNum == NULL)
        xapError(xp, "Missing Hit_num");
    if (obj->ncbiBlastHitNum->next != NULL)
        xapError(xp, "Multiple Hit_num");
    if (obj->ncbiBlastHitId == NULL)
        xapError(xp, "Missing Hit_id");
    if (obj->ncbiBlastHitId->next != NULL)
        xapError(xp, "Multiple Hit_id");
    if (obj->ncbiBlastHitDef == NULL)
        xapError(xp, "Missing Hit_def");
    if (obj->ncbiBlastHitDef->next != NULL)
        xapError(xp, "Multiple Hit_def");
    if (obj->ncbiBlastHitAccession == NULL)
        xapError(xp, "Missing Hit_accession");
    if (obj->ncbiBlastHitAccession->next != NULL)
        xapError(xp, "Multiple Hit_accession");
    if (obj->ncbiBlastHitLen == NULL)
        xapError(xp, "Missing Hit_len");
    if (obj->ncbiBlastHitLen->next != NULL)
        xapError(xp, "Multiple Hit_len");
    if (obj->ncbiBlastHitHsps != NULL && obj->ncbiBlastHitHsps->next != NULL)
        xapError(xp, "Multiple Hit_hsps");
    }
else if (sameString(name, "Hit_num"))
    {
    struct ncbiBlastHitNum *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hit_id"))
    {
    struct ncbiBlastHitId *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Hit_def"))
    {
    struct ncbiBlastHitDef *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Hit_accession"))
    {
    struct ncbiBlastHitAccession *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Hit_len"))
    {
    struct ncbiBlastHitLen *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hit_hsps"))
    {
    struct ncbiBlastHitHsps *obj = stack->object;
    slReverse(&obj->ncbiBlastHsp);
    }
else if (sameString(name, "Hsp"))
    {
    struct ncbiBlastHsp *obj = stack->object;
    if (obj->ncbiBlastHspNum == NULL)
        xapError(xp, "Missing Hsp_num");
    if (obj->ncbiBlastHspNum->next != NULL)
        xapError(xp, "Multiple Hsp_num");
    if (obj->ncbiBlastHspBitScore == NULL)
        xapError(xp, "Missing Hsp_bit-score");
    if (obj->ncbiBlastHspBitScore->next != NULL)
        xapError(xp, "Multiple Hsp_bit-score");
    if (obj->ncbiBlastHspScore == NULL)
        xapError(xp, "Missing Hsp_score");
    if (obj->ncbiBlastHspScore->next != NULL)
        xapError(xp, "Multiple Hsp_score");
    if (obj->ncbiBlastHspEvalue == NULL)
        xapError(xp, "Missing Hsp_evalue");
    if (obj->ncbiBlastHspEvalue->next != NULL)
        xapError(xp, "Multiple Hsp_evalue");
    if (obj->ncbiBlastHspQueryFrom == NULL)
        xapError(xp, "Missing Hsp_query-from");
    if (obj->ncbiBlastHspQueryFrom->next != NULL)
        xapError(xp, "Multiple Hsp_query-from");
    if (obj->ncbiBlastHspQueryTo == NULL)
        xapError(xp, "Missing Hsp_query-to");
    if (obj->ncbiBlastHspQueryTo->next != NULL)
        xapError(xp, "Multiple Hsp_query-to");
    if (obj->ncbiBlastHspHitFrom == NULL)
        xapError(xp, "Missing Hsp_hit-from");
    if (obj->ncbiBlastHspHitFrom->next != NULL)
        xapError(xp, "Multiple Hsp_hit-from");
    if (obj->ncbiBlastHspHitTo == NULL)
        xapError(xp, "Missing Hsp_hit-to");
    if (obj->ncbiBlastHspHitTo->next != NULL)
        xapError(xp, "Multiple Hsp_hit-to");
    if (obj->ncbiBlastHspPatternFrom != NULL && obj->ncbiBlastHspPatternFrom->next != NULL)
        xapError(xp, "Multiple Hsp_pattern-from");
    if (obj->ncbiBlastHspPatternTo != NULL && obj->ncbiBlastHspPatternTo->next != NULL)
        xapError(xp, "Multiple Hsp_pattern-to");
    if (obj->ncbiBlastHspQueryFrame != NULL && obj->ncbiBlastHspQueryFrame->next != NULL)
        xapError(xp, "Multiple Hsp_query-frame");
    if (obj->ncbiBlastHspHitFrame != NULL && obj->ncbiBlastHspHitFrame->next != NULL)
        xapError(xp, "Multiple Hsp_hit-frame");
    if (obj->ncbiBlastHspIdentity != NULL && obj->ncbiBlastHspIdentity->next != NULL)
        xapError(xp, "Multiple Hsp_identity");
    if (obj->ncbiBlastHspPositive != NULL && obj->ncbiBlastHspPositive->next != NULL)
        xapError(xp, "Multiple Hsp_positive");
    if (obj->ncbiBlastHspGaps != NULL && obj->ncbiBlastHspGaps->next != NULL)
        xapError(xp, "Multiple Hsp_gaps");
    if (obj->ncbiBlastHspAlignLen != NULL && obj->ncbiBlastHspAlignLen->next != NULL)
        xapError(xp, "Multiple Hsp_align-len");
    if (obj->ncbiBlastHspDensity != NULL && obj->ncbiBlastHspDensity->next != NULL)
        xapError(xp, "Multiple Hsp_density");
    if (obj->ncbiBlastHspQseq == NULL)
        xapError(xp, "Missing Hsp_qseq");
    if (obj->ncbiBlastHspQseq->next != NULL)
        xapError(xp, "Multiple Hsp_qseq");
    if (obj->ncbiBlastHspHseq == NULL)
        xapError(xp, "Missing Hsp_hseq");
    if (obj->ncbiBlastHspHseq->next != NULL)
        xapError(xp, "Multiple Hsp_hseq");
    if (obj->ncbiBlastHspMidline != NULL && obj->ncbiBlastHspMidline->next != NULL)
        xapError(xp, "Multiple Hsp_midline");
    }
else if (sameString(name, "Hsp_num"))
    {
    struct ncbiBlastHspNum *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_bit-score"))
    {
    struct ncbiBlastHspBitScore *obj = stack->object;
    obj->text = sqlDouble(stack->text->string);
    }
else if (sameString(name, "Hsp_score"))
    {
    struct ncbiBlastHspScore *obj = stack->object;
    obj->text = sqlDouble(stack->text->string);
    }
else if (sameString(name, "Hsp_evalue"))
    {
    struct ncbiBlastHspEvalue *obj = stack->object;
    obj->text = sqlDouble(stack->text->string);
    }
else if (sameString(name, "Hsp_query-from"))
    {
    struct ncbiBlastHspQueryFrom *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_query-to"))
    {
    struct ncbiBlastHspQueryTo *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_hit-from"))
    {
    struct ncbiBlastHspHitFrom *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_hit-to"))
    {
    struct ncbiBlastHspHitTo *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_pattern-from"))
    {
    struct ncbiBlastHspPatternFrom *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_pattern-to"))
    {
    struct ncbiBlastHspPatternTo *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_query-frame"))
    {
    struct ncbiBlastHspQueryFrame *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_hit-frame"))
    {
    struct ncbiBlastHspHitFrame *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_identity"))
    {
    struct ncbiBlastHspIdentity *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_positive"))
    {
    struct ncbiBlastHspPositive *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_gaps"))
    {
    struct ncbiBlastHspGaps *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_align-len"))
    {
    struct ncbiBlastHspAlignLen *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_density"))
    {
    struct ncbiBlastHspDensity *obj = stack->object;
    obj->text = sqlSigned(stack->text->string);
    }
else if (sameString(name, "Hsp_qseq"))
    {
    struct ncbiBlastHspQseq *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Hsp_hseq"))
    {
    struct ncbiBlastHspHseq *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
else if (sameString(name, "Hsp_midline"))
    {
    struct ncbiBlastHspMidline *obj = stack->object;
    obj->text = cloneString(stack->text->string);
    }
}


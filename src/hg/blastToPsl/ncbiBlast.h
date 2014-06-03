/* ncbiBlast.h autoXml generated file */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef NCBIBLAST_H
#define NCBIBLAST_H

#ifndef XAP_H
#include "xap.h"
#endif

/* The start and end handlers here are used with routines defined in xap.h.
 * In particular if you want to read just parts of the XML file into memory
 * call xapOpen() with these, and then xapNext() with the name of the tag
 * you want to load. */

void *ncbiBlastStartHandler(struct xap *xp, char *name, char **atts);
/* Called by xap with start tag.  Does most of the parsing work. */

void ncbiBlastEndHandler(struct xap *xp, char *name);
/* Called by xap with end tag.  Checks all required children are loaded. */


struct ncbiBlastBlastOutput
    {
    struct ncbiBlastBlastOutput *next;
    struct ncbiBlastBlastOutputProgram *ncbiBlastBlastOutputProgram;	/** Single instance required. **/
    struct ncbiBlastBlastOutputVersion *ncbiBlastBlastOutputVersion;	/** Single instance required. **/
    struct ncbiBlastBlastOutputReference *ncbiBlastBlastOutputReference;	/** Single instance required. **/
    struct ncbiBlastBlastOutputDb *ncbiBlastBlastOutputDb;	/** Single instance required. **/
    struct ncbiBlastBlastOutputQueryID *ncbiBlastBlastOutputQueryID;	/** Single instance required. **/
    struct ncbiBlastBlastOutputQueryDef *ncbiBlastBlastOutputQueryDef;	/** Single instance required. **/
    struct ncbiBlastBlastOutputQueryLen *ncbiBlastBlastOutputQueryLen;	/** Single instance required. **/
    struct ncbiBlastBlastOutputQuerySeq *ncbiBlastBlastOutputQuerySeq;	/** Optional (may be NULL). **/
    struct ncbiBlastBlastOutputParam *ncbiBlastBlastOutputParam;	/** Single instance required. **/
    struct ncbiBlastBlastOutputIterations *ncbiBlastBlastOutputIterations;	/** Single instance required. **/
    struct ncbiBlastBlastOutputMbstat *ncbiBlastBlastOutputMbstat;	/** Optional (may be NULL). **/
    };

void ncbiBlastBlastOutputFree(struct ncbiBlastBlastOutput **pObj);
/* Free up ncbiBlastBlastOutput. */

void ncbiBlastBlastOutputFreeList(struct ncbiBlastBlastOutput **pList);
/* Free up list of ncbiBlastBlastOutput. */

void ncbiBlastBlastOutputSave(struct ncbiBlastBlastOutput *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutput to file. */

struct ncbiBlastBlastOutput *ncbiBlastBlastOutputLoad(char *fileName);
/* Load ncbiBlastBlastOutput from XML file where it is root element. */

struct ncbiBlastBlastOutput *ncbiBlastBlastOutputLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutput element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputProgram
    {
    struct ncbiBlastBlastOutputProgram *next;
    char *text;
    };

void ncbiBlastBlastOutputProgramFree(struct ncbiBlastBlastOutputProgram **pObj);
/* Free up ncbiBlastBlastOutputProgram. */

void ncbiBlastBlastOutputProgramFreeList(struct ncbiBlastBlastOutputProgram **pList);
/* Free up list of ncbiBlastBlastOutputProgram. */

void ncbiBlastBlastOutputProgramSave(struct ncbiBlastBlastOutputProgram *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputProgram to file. */

struct ncbiBlastBlastOutputProgram *ncbiBlastBlastOutputProgramLoad(char *fileName);
/* Load ncbiBlastBlastOutputProgram from XML file where it is root element. */

struct ncbiBlastBlastOutputProgram *ncbiBlastBlastOutputProgramLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputProgram element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputVersion
    {
    struct ncbiBlastBlastOutputVersion *next;
    char *text;
    };

void ncbiBlastBlastOutputVersionFree(struct ncbiBlastBlastOutputVersion **pObj);
/* Free up ncbiBlastBlastOutputVersion. */

void ncbiBlastBlastOutputVersionFreeList(struct ncbiBlastBlastOutputVersion **pList);
/* Free up list of ncbiBlastBlastOutputVersion. */

void ncbiBlastBlastOutputVersionSave(struct ncbiBlastBlastOutputVersion *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputVersion to file. */

struct ncbiBlastBlastOutputVersion *ncbiBlastBlastOutputVersionLoad(char *fileName);
/* Load ncbiBlastBlastOutputVersion from XML file where it is root element. */

struct ncbiBlastBlastOutputVersion *ncbiBlastBlastOutputVersionLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputVersion element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputReference
    {
    struct ncbiBlastBlastOutputReference *next;
    char *text;
    };

void ncbiBlastBlastOutputReferenceFree(struct ncbiBlastBlastOutputReference **pObj);
/* Free up ncbiBlastBlastOutputReference. */

void ncbiBlastBlastOutputReferenceFreeList(struct ncbiBlastBlastOutputReference **pList);
/* Free up list of ncbiBlastBlastOutputReference. */

void ncbiBlastBlastOutputReferenceSave(struct ncbiBlastBlastOutputReference *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputReference to file. */

struct ncbiBlastBlastOutputReference *ncbiBlastBlastOutputReferenceLoad(char *fileName);
/* Load ncbiBlastBlastOutputReference from XML file where it is root element. */

struct ncbiBlastBlastOutputReference *ncbiBlastBlastOutputReferenceLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputReference element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputDb
    {
    struct ncbiBlastBlastOutputDb *next;
    char *text;
    };

void ncbiBlastBlastOutputDbFree(struct ncbiBlastBlastOutputDb **pObj);
/* Free up ncbiBlastBlastOutputDb. */

void ncbiBlastBlastOutputDbFreeList(struct ncbiBlastBlastOutputDb **pList);
/* Free up list of ncbiBlastBlastOutputDb. */

void ncbiBlastBlastOutputDbSave(struct ncbiBlastBlastOutputDb *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputDb to file. */

struct ncbiBlastBlastOutputDb *ncbiBlastBlastOutputDbLoad(char *fileName);
/* Load ncbiBlastBlastOutputDb from XML file where it is root element. */

struct ncbiBlastBlastOutputDb *ncbiBlastBlastOutputDbLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputDb element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputQueryID
    {
    struct ncbiBlastBlastOutputQueryID *next;
    char *text;
    };

void ncbiBlastBlastOutputQueryIDFree(struct ncbiBlastBlastOutputQueryID **pObj);
/* Free up ncbiBlastBlastOutputQueryID. */

void ncbiBlastBlastOutputQueryIDFreeList(struct ncbiBlastBlastOutputQueryID **pList);
/* Free up list of ncbiBlastBlastOutputQueryID. */

void ncbiBlastBlastOutputQueryIDSave(struct ncbiBlastBlastOutputQueryID *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputQueryID to file. */

struct ncbiBlastBlastOutputQueryID *ncbiBlastBlastOutputQueryIDLoad(char *fileName);
/* Load ncbiBlastBlastOutputQueryID from XML file where it is root element. */

struct ncbiBlastBlastOutputQueryID *ncbiBlastBlastOutputQueryIDLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputQueryID element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputQueryDef
    {
    struct ncbiBlastBlastOutputQueryDef *next;
    char *text;
    };

void ncbiBlastBlastOutputQueryDefFree(struct ncbiBlastBlastOutputQueryDef **pObj);
/* Free up ncbiBlastBlastOutputQueryDef. */

void ncbiBlastBlastOutputQueryDefFreeList(struct ncbiBlastBlastOutputQueryDef **pList);
/* Free up list of ncbiBlastBlastOutputQueryDef. */

void ncbiBlastBlastOutputQueryDefSave(struct ncbiBlastBlastOutputQueryDef *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputQueryDef to file. */

struct ncbiBlastBlastOutputQueryDef *ncbiBlastBlastOutputQueryDefLoad(char *fileName);
/* Load ncbiBlastBlastOutputQueryDef from XML file where it is root element. */

struct ncbiBlastBlastOutputQueryDef *ncbiBlastBlastOutputQueryDefLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputQueryDef element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputQueryLen
    {
    struct ncbiBlastBlastOutputQueryLen *next;
    int text;
    };

void ncbiBlastBlastOutputQueryLenFree(struct ncbiBlastBlastOutputQueryLen **pObj);
/* Free up ncbiBlastBlastOutputQueryLen. */

void ncbiBlastBlastOutputQueryLenFreeList(struct ncbiBlastBlastOutputQueryLen **pList);
/* Free up list of ncbiBlastBlastOutputQueryLen. */

void ncbiBlastBlastOutputQueryLenSave(struct ncbiBlastBlastOutputQueryLen *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputQueryLen to file. */

struct ncbiBlastBlastOutputQueryLen *ncbiBlastBlastOutputQueryLenLoad(char *fileName);
/* Load ncbiBlastBlastOutputQueryLen from XML file where it is root element. */

struct ncbiBlastBlastOutputQueryLen *ncbiBlastBlastOutputQueryLenLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputQueryLen element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputQuerySeq
    {
    struct ncbiBlastBlastOutputQuerySeq *next;
    char *text;
    };

void ncbiBlastBlastOutputQuerySeqFree(struct ncbiBlastBlastOutputQuerySeq **pObj);
/* Free up ncbiBlastBlastOutputQuerySeq. */

void ncbiBlastBlastOutputQuerySeqFreeList(struct ncbiBlastBlastOutputQuerySeq **pList);
/* Free up list of ncbiBlastBlastOutputQuerySeq. */

void ncbiBlastBlastOutputQuerySeqSave(struct ncbiBlastBlastOutputQuerySeq *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputQuerySeq to file. */

struct ncbiBlastBlastOutputQuerySeq *ncbiBlastBlastOutputQuerySeqLoad(char *fileName);
/* Load ncbiBlastBlastOutputQuerySeq from XML file where it is root element. */

struct ncbiBlastBlastOutputQuerySeq *ncbiBlastBlastOutputQuerySeqLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputQuerySeq element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputParam
    {
    struct ncbiBlastBlastOutputParam *next;
    struct ncbiBlastParameters *ncbiBlastParameters;	/** Single instance required. **/
    };

void ncbiBlastBlastOutputParamFree(struct ncbiBlastBlastOutputParam **pObj);
/* Free up ncbiBlastBlastOutputParam. */

void ncbiBlastBlastOutputParamFreeList(struct ncbiBlastBlastOutputParam **pList);
/* Free up list of ncbiBlastBlastOutputParam. */

void ncbiBlastBlastOutputParamSave(struct ncbiBlastBlastOutputParam *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputParam to file. */

struct ncbiBlastBlastOutputParam *ncbiBlastBlastOutputParamLoad(char *fileName);
/* Load ncbiBlastBlastOutputParam from XML file where it is root element. */

struct ncbiBlastBlastOutputParam *ncbiBlastBlastOutputParamLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputParam element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputIterations
    {
    struct ncbiBlastBlastOutputIterations *next;
    struct ncbiBlastIteration *ncbiBlastIteration;	/** Possibly empty list. **/
    };

void ncbiBlastBlastOutputIterationsFree(struct ncbiBlastBlastOutputIterations **pObj);
/* Free up ncbiBlastBlastOutputIterations. */

void ncbiBlastBlastOutputIterationsFreeList(struct ncbiBlastBlastOutputIterations **pList);
/* Free up list of ncbiBlastBlastOutputIterations. */

void ncbiBlastBlastOutputIterationsSave(struct ncbiBlastBlastOutputIterations *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputIterations to file. */

struct ncbiBlastBlastOutputIterations *ncbiBlastBlastOutputIterationsLoad(char *fileName);
/* Load ncbiBlastBlastOutputIterations from XML file where it is root element. */

struct ncbiBlastBlastOutputIterations *ncbiBlastBlastOutputIterationsLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputIterations element.  Use xapOpen to get xap. */

struct ncbiBlastBlastOutputMbstat
    {
    struct ncbiBlastBlastOutputMbstat *next;
    struct ncbiBlastStatistics *ncbiBlastStatistics;	/** Single instance required. **/
    };

void ncbiBlastBlastOutputMbstatFree(struct ncbiBlastBlastOutputMbstat **pObj);
/* Free up ncbiBlastBlastOutputMbstat. */

void ncbiBlastBlastOutputMbstatFreeList(struct ncbiBlastBlastOutputMbstat **pList);
/* Free up list of ncbiBlastBlastOutputMbstat. */

void ncbiBlastBlastOutputMbstatSave(struct ncbiBlastBlastOutputMbstat *obj, int indent, FILE *f);
/* Save ncbiBlastBlastOutputMbstat to file. */

struct ncbiBlastBlastOutputMbstat *ncbiBlastBlastOutputMbstatLoad(char *fileName);
/* Load ncbiBlastBlastOutputMbstat from XML file where it is root element. */

struct ncbiBlastBlastOutputMbstat *ncbiBlastBlastOutputMbstatLoadNext(struct xap *xap);
/* Load next ncbiBlastBlastOutputMbstat element.  Use xapOpen to get xap. */

struct ncbiBlastIteration
    {
    struct ncbiBlastIteration *next;
    struct ncbiBlastIterationIterNum *ncbiBlastIterationIterNum;	/** Single instance required. **/
    struct ncbiBlastIterationQueryID *ncbiBlastIterationQueryID;	/** Optional (may be NULL). **/
    struct ncbiBlastIterationQueryDef *ncbiBlastIterationQueryDef;	/** Optional (may be NULL). **/
    struct ncbiBlastIterationQueryLen *ncbiBlastIterationQueryLen;	/** Optional (may be NULL). **/
    struct ncbiBlastIterationHits *ncbiBlastIterationHits;	/** Optional (may be NULL). **/
    struct ncbiBlastIterationStat *ncbiBlastIterationStat;	/** Optional (may be NULL). **/
    struct ncbiBlastIterationMessage *ncbiBlastIterationMessage;	/** Optional (may be NULL). **/
    };

void ncbiBlastIterationFree(struct ncbiBlastIteration **pObj);
/* Free up ncbiBlastIteration. */

void ncbiBlastIterationFreeList(struct ncbiBlastIteration **pList);
/* Free up list of ncbiBlastIteration. */

void ncbiBlastIterationSave(struct ncbiBlastIteration *obj, int indent, FILE *f);
/* Save ncbiBlastIteration to file. */

struct ncbiBlastIteration *ncbiBlastIterationLoad(char *fileName);
/* Load ncbiBlastIteration from XML file where it is root element. */

struct ncbiBlastIteration *ncbiBlastIterationLoadNext(struct xap *xap);
/* Load next ncbiBlastIteration element.  Use xapOpen to get xap. */

struct ncbiBlastIterationIterNum
    {
    struct ncbiBlastIterationIterNum *next;
    int text;
    };

void ncbiBlastIterationIterNumFree(struct ncbiBlastIterationIterNum **pObj);
/* Free up ncbiBlastIterationIterNum. */

void ncbiBlastIterationIterNumFreeList(struct ncbiBlastIterationIterNum **pList);
/* Free up list of ncbiBlastIterationIterNum. */

void ncbiBlastIterationIterNumSave(struct ncbiBlastIterationIterNum *obj, int indent, FILE *f);
/* Save ncbiBlastIterationIterNum to file. */

struct ncbiBlastIterationIterNum *ncbiBlastIterationIterNumLoad(char *fileName);
/* Load ncbiBlastIterationIterNum from XML file where it is root element. */

struct ncbiBlastIterationIterNum *ncbiBlastIterationIterNumLoadNext(struct xap *xap);
/* Load next ncbiBlastIterationIterNum element.  Use xapOpen to get xap. */

struct ncbiBlastIterationQueryID
    {
    struct ncbiBlastIterationQueryID *next;
    char *text;
    };

void ncbiBlastIterationQueryIDFree(struct ncbiBlastIterationQueryID **pObj);
/* Free up ncbiBlastIterationQueryID. */

void ncbiBlastIterationQueryIDFreeList(struct ncbiBlastIterationQueryID **pList);
/* Free up list of ncbiBlastIterationQueryID. */

void ncbiBlastIterationQueryIDSave(struct ncbiBlastIterationQueryID *obj, int indent, FILE *f);
/* Save ncbiBlastIterationQueryID to file. */

struct ncbiBlastIterationQueryID *ncbiBlastIterationQueryIDLoad(char *fileName);
/* Load ncbiBlastIterationQueryID from XML file where it is root element. */

struct ncbiBlastIterationQueryID *ncbiBlastIterationQueryIDLoadNext(struct xap *xap);
/* Load next ncbiBlastIterationQueryID element.  Use xapOpen to get xap. */

struct ncbiBlastIterationQueryDef
    {
    struct ncbiBlastIterationQueryDef *next;
    char *text;
    };

void ncbiBlastIterationQueryDefFree(struct ncbiBlastIterationQueryDef **pObj);
/* Free up ncbiBlastIterationQueryDef. */

void ncbiBlastIterationQueryDefFreeList(struct ncbiBlastIterationQueryDef **pList);
/* Free up list of ncbiBlastIterationQueryDef. */

void ncbiBlastIterationQueryDefSave(struct ncbiBlastIterationQueryDef *obj, int indent, FILE *f);
/* Save ncbiBlastIterationQueryDef to file. */

struct ncbiBlastIterationQueryDef *ncbiBlastIterationQueryDefLoad(char *fileName);
/* Load ncbiBlastIterationQueryDef from XML file where it is root element. */

struct ncbiBlastIterationQueryDef *ncbiBlastIterationQueryDefLoadNext(struct xap *xap);
/* Load next ncbiBlastIterationQueryDef element.  Use xapOpen to get xap. */

struct ncbiBlastIterationQueryLen
    {
    struct ncbiBlastIterationQueryLen *next;
    int text;
    };

void ncbiBlastIterationQueryLenFree(struct ncbiBlastIterationQueryLen **pObj);
/* Free up ncbiBlastIterationQueryLen. */

void ncbiBlastIterationQueryLenFreeList(struct ncbiBlastIterationQueryLen **pList);
/* Free up list of ncbiBlastIterationQueryLen. */

void ncbiBlastIterationQueryLenSave(struct ncbiBlastIterationQueryLen *obj, int indent, FILE *f);
/* Save ncbiBlastIterationQueryLen to file. */

struct ncbiBlastIterationQueryLen *ncbiBlastIterationQueryLenLoad(char *fileName);
/* Load ncbiBlastIterationQueryLen from XML file where it is root element. */

struct ncbiBlastIterationQueryLen *ncbiBlastIterationQueryLenLoadNext(struct xap *xap);
/* Load next ncbiBlastIterationQueryLen element.  Use xapOpen to get xap. */

struct ncbiBlastIterationHits
    {
    struct ncbiBlastIterationHits *next;
    struct ncbiBlastHit *ncbiBlastHit;	/** Possibly empty list. **/
    };

void ncbiBlastIterationHitsFree(struct ncbiBlastIterationHits **pObj);
/* Free up ncbiBlastIterationHits. */

void ncbiBlastIterationHitsFreeList(struct ncbiBlastIterationHits **pList);
/* Free up list of ncbiBlastIterationHits. */

void ncbiBlastIterationHitsSave(struct ncbiBlastIterationHits *obj, int indent, FILE *f);
/* Save ncbiBlastIterationHits to file. */

struct ncbiBlastIterationHits *ncbiBlastIterationHitsLoad(char *fileName);
/* Load ncbiBlastIterationHits from XML file where it is root element. */

struct ncbiBlastIterationHits *ncbiBlastIterationHitsLoadNext(struct xap *xap);
/* Load next ncbiBlastIterationHits element.  Use xapOpen to get xap. */

struct ncbiBlastIterationStat
    {
    struct ncbiBlastIterationStat *next;
    struct ncbiBlastStatistics *ncbiBlastStatistics;	/** Single instance required. **/
    };

void ncbiBlastIterationStatFree(struct ncbiBlastIterationStat **pObj);
/* Free up ncbiBlastIterationStat. */

void ncbiBlastIterationStatFreeList(struct ncbiBlastIterationStat **pList);
/* Free up list of ncbiBlastIterationStat. */

void ncbiBlastIterationStatSave(struct ncbiBlastIterationStat *obj, int indent, FILE *f);
/* Save ncbiBlastIterationStat to file. */

struct ncbiBlastIterationStat *ncbiBlastIterationStatLoad(char *fileName);
/* Load ncbiBlastIterationStat from XML file where it is root element. */

struct ncbiBlastIterationStat *ncbiBlastIterationStatLoadNext(struct xap *xap);
/* Load next ncbiBlastIterationStat element.  Use xapOpen to get xap. */

struct ncbiBlastIterationMessage
    {
    struct ncbiBlastIterationMessage *next;
    char *text;
    };

void ncbiBlastIterationMessageFree(struct ncbiBlastIterationMessage **pObj);
/* Free up ncbiBlastIterationMessage. */

void ncbiBlastIterationMessageFreeList(struct ncbiBlastIterationMessage **pList);
/* Free up list of ncbiBlastIterationMessage. */

void ncbiBlastIterationMessageSave(struct ncbiBlastIterationMessage *obj, int indent, FILE *f);
/* Save ncbiBlastIterationMessage to file. */

struct ncbiBlastIterationMessage *ncbiBlastIterationMessageLoad(char *fileName);
/* Load ncbiBlastIterationMessage from XML file where it is root element. */

struct ncbiBlastIterationMessage *ncbiBlastIterationMessageLoadNext(struct xap *xap);
/* Load next ncbiBlastIterationMessage element.  Use xapOpen to get xap. */

struct ncbiBlastParameters
    {
    struct ncbiBlastParameters *next;
    struct ncbiBlastParametersMatrix *ncbiBlastParametersMatrix;	/** Optional (may be NULL). **/
    struct ncbiBlastParametersExpect *ncbiBlastParametersExpect;	/** Single instance required. **/
    struct ncbiBlastParametersInclude *ncbiBlastParametersInclude;	/** Optional (may be NULL). **/
    struct ncbiBlastParametersScMatch *ncbiBlastParametersScMatch;	/** Optional (may be NULL). **/
    struct ncbiBlastParametersScMismatch *ncbiBlastParametersScMismatch;	/** Optional (may be NULL). **/
    struct ncbiBlastParametersGapOpen *ncbiBlastParametersGapOpen;	/** Single instance required. **/
    struct ncbiBlastParametersGapExtend *ncbiBlastParametersGapExtend;	/** Single instance required. **/
    struct ncbiBlastParametersFilter *ncbiBlastParametersFilter;	/** Optional (may be NULL). **/
    struct ncbiBlastParametersPattern *ncbiBlastParametersPattern;	/** Optional (may be NULL). **/
    struct ncbiBlastParametersEntrezQuery *ncbiBlastParametersEntrezQuery;	/** Optional (may be NULL). **/
    };

void ncbiBlastParametersFree(struct ncbiBlastParameters **pObj);
/* Free up ncbiBlastParameters. */

void ncbiBlastParametersFreeList(struct ncbiBlastParameters **pList);
/* Free up list of ncbiBlastParameters. */

void ncbiBlastParametersSave(struct ncbiBlastParameters *obj, int indent, FILE *f);
/* Save ncbiBlastParameters to file. */

struct ncbiBlastParameters *ncbiBlastParametersLoad(char *fileName);
/* Load ncbiBlastParameters from XML file where it is root element. */

struct ncbiBlastParameters *ncbiBlastParametersLoadNext(struct xap *xap);
/* Load next ncbiBlastParameters element.  Use xapOpen to get xap. */

struct ncbiBlastParametersMatrix
    {
    struct ncbiBlastParametersMatrix *next;
    char *text;
    };

void ncbiBlastParametersMatrixFree(struct ncbiBlastParametersMatrix **pObj);
/* Free up ncbiBlastParametersMatrix. */

void ncbiBlastParametersMatrixFreeList(struct ncbiBlastParametersMatrix **pList);
/* Free up list of ncbiBlastParametersMatrix. */

void ncbiBlastParametersMatrixSave(struct ncbiBlastParametersMatrix *obj, int indent, FILE *f);
/* Save ncbiBlastParametersMatrix to file. */

struct ncbiBlastParametersMatrix *ncbiBlastParametersMatrixLoad(char *fileName);
/* Load ncbiBlastParametersMatrix from XML file where it is root element. */

struct ncbiBlastParametersMatrix *ncbiBlastParametersMatrixLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersMatrix element.  Use xapOpen to get xap. */

struct ncbiBlastParametersExpect
    {
    struct ncbiBlastParametersExpect *next;
    double text;
    };

void ncbiBlastParametersExpectFree(struct ncbiBlastParametersExpect **pObj);
/* Free up ncbiBlastParametersExpect. */

void ncbiBlastParametersExpectFreeList(struct ncbiBlastParametersExpect **pList);
/* Free up list of ncbiBlastParametersExpect. */

void ncbiBlastParametersExpectSave(struct ncbiBlastParametersExpect *obj, int indent, FILE *f);
/* Save ncbiBlastParametersExpect to file. */

struct ncbiBlastParametersExpect *ncbiBlastParametersExpectLoad(char *fileName);
/* Load ncbiBlastParametersExpect from XML file where it is root element. */

struct ncbiBlastParametersExpect *ncbiBlastParametersExpectLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersExpect element.  Use xapOpen to get xap. */

struct ncbiBlastParametersInclude
    {
    struct ncbiBlastParametersInclude *next;
    double text;
    };

void ncbiBlastParametersIncludeFree(struct ncbiBlastParametersInclude **pObj);
/* Free up ncbiBlastParametersInclude. */

void ncbiBlastParametersIncludeFreeList(struct ncbiBlastParametersInclude **pList);
/* Free up list of ncbiBlastParametersInclude. */

void ncbiBlastParametersIncludeSave(struct ncbiBlastParametersInclude *obj, int indent, FILE *f);
/* Save ncbiBlastParametersInclude to file. */

struct ncbiBlastParametersInclude *ncbiBlastParametersIncludeLoad(char *fileName);
/* Load ncbiBlastParametersInclude from XML file where it is root element. */

struct ncbiBlastParametersInclude *ncbiBlastParametersIncludeLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersInclude element.  Use xapOpen to get xap. */

struct ncbiBlastParametersScMatch
    {
    struct ncbiBlastParametersScMatch *next;
    int text;
    };

void ncbiBlastParametersScMatchFree(struct ncbiBlastParametersScMatch **pObj);
/* Free up ncbiBlastParametersScMatch. */

void ncbiBlastParametersScMatchFreeList(struct ncbiBlastParametersScMatch **pList);
/* Free up list of ncbiBlastParametersScMatch. */

void ncbiBlastParametersScMatchSave(struct ncbiBlastParametersScMatch *obj, int indent, FILE *f);
/* Save ncbiBlastParametersScMatch to file. */

struct ncbiBlastParametersScMatch *ncbiBlastParametersScMatchLoad(char *fileName);
/* Load ncbiBlastParametersScMatch from XML file where it is root element. */

struct ncbiBlastParametersScMatch *ncbiBlastParametersScMatchLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersScMatch element.  Use xapOpen to get xap. */

struct ncbiBlastParametersScMismatch
    {
    struct ncbiBlastParametersScMismatch *next;
    int text;
    };

void ncbiBlastParametersScMismatchFree(struct ncbiBlastParametersScMismatch **pObj);
/* Free up ncbiBlastParametersScMismatch. */

void ncbiBlastParametersScMismatchFreeList(struct ncbiBlastParametersScMismatch **pList);
/* Free up list of ncbiBlastParametersScMismatch. */

void ncbiBlastParametersScMismatchSave(struct ncbiBlastParametersScMismatch *obj, int indent, FILE *f);
/* Save ncbiBlastParametersScMismatch to file. */

struct ncbiBlastParametersScMismatch *ncbiBlastParametersScMismatchLoad(char *fileName);
/* Load ncbiBlastParametersScMismatch from XML file where it is root element. */

struct ncbiBlastParametersScMismatch *ncbiBlastParametersScMismatchLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersScMismatch element.  Use xapOpen to get xap. */

struct ncbiBlastParametersGapOpen
    {
    struct ncbiBlastParametersGapOpen *next;
    int text;
    };

void ncbiBlastParametersGapOpenFree(struct ncbiBlastParametersGapOpen **pObj);
/* Free up ncbiBlastParametersGapOpen. */

void ncbiBlastParametersGapOpenFreeList(struct ncbiBlastParametersGapOpen **pList);
/* Free up list of ncbiBlastParametersGapOpen. */

void ncbiBlastParametersGapOpenSave(struct ncbiBlastParametersGapOpen *obj, int indent, FILE *f);
/* Save ncbiBlastParametersGapOpen to file. */

struct ncbiBlastParametersGapOpen *ncbiBlastParametersGapOpenLoad(char *fileName);
/* Load ncbiBlastParametersGapOpen from XML file where it is root element. */

struct ncbiBlastParametersGapOpen *ncbiBlastParametersGapOpenLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersGapOpen element.  Use xapOpen to get xap. */

struct ncbiBlastParametersGapExtend
    {
    struct ncbiBlastParametersGapExtend *next;
    int text;
    };

void ncbiBlastParametersGapExtendFree(struct ncbiBlastParametersGapExtend **pObj);
/* Free up ncbiBlastParametersGapExtend. */

void ncbiBlastParametersGapExtendFreeList(struct ncbiBlastParametersGapExtend **pList);
/* Free up list of ncbiBlastParametersGapExtend. */

void ncbiBlastParametersGapExtendSave(struct ncbiBlastParametersGapExtend *obj, int indent, FILE *f);
/* Save ncbiBlastParametersGapExtend to file. */

struct ncbiBlastParametersGapExtend *ncbiBlastParametersGapExtendLoad(char *fileName);
/* Load ncbiBlastParametersGapExtend from XML file where it is root element. */

struct ncbiBlastParametersGapExtend *ncbiBlastParametersGapExtendLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersGapExtend element.  Use xapOpen to get xap. */

struct ncbiBlastParametersFilter
    {
    struct ncbiBlastParametersFilter *next;
    char *text;
    };

void ncbiBlastParametersFilterFree(struct ncbiBlastParametersFilter **pObj);
/* Free up ncbiBlastParametersFilter. */

void ncbiBlastParametersFilterFreeList(struct ncbiBlastParametersFilter **pList);
/* Free up list of ncbiBlastParametersFilter. */

void ncbiBlastParametersFilterSave(struct ncbiBlastParametersFilter *obj, int indent, FILE *f);
/* Save ncbiBlastParametersFilter to file. */

struct ncbiBlastParametersFilter *ncbiBlastParametersFilterLoad(char *fileName);
/* Load ncbiBlastParametersFilter from XML file where it is root element. */

struct ncbiBlastParametersFilter *ncbiBlastParametersFilterLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersFilter element.  Use xapOpen to get xap. */

struct ncbiBlastParametersPattern
    {
    struct ncbiBlastParametersPattern *next;
    char *text;
    };

void ncbiBlastParametersPatternFree(struct ncbiBlastParametersPattern **pObj);
/* Free up ncbiBlastParametersPattern. */

void ncbiBlastParametersPatternFreeList(struct ncbiBlastParametersPattern **pList);
/* Free up list of ncbiBlastParametersPattern. */

void ncbiBlastParametersPatternSave(struct ncbiBlastParametersPattern *obj, int indent, FILE *f);
/* Save ncbiBlastParametersPattern to file. */

struct ncbiBlastParametersPattern *ncbiBlastParametersPatternLoad(char *fileName);
/* Load ncbiBlastParametersPattern from XML file where it is root element. */

struct ncbiBlastParametersPattern *ncbiBlastParametersPatternLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersPattern element.  Use xapOpen to get xap. */

struct ncbiBlastParametersEntrezQuery
    {
    struct ncbiBlastParametersEntrezQuery *next;
    char *text;
    };

void ncbiBlastParametersEntrezQueryFree(struct ncbiBlastParametersEntrezQuery **pObj);
/* Free up ncbiBlastParametersEntrezQuery. */

void ncbiBlastParametersEntrezQueryFreeList(struct ncbiBlastParametersEntrezQuery **pList);
/* Free up list of ncbiBlastParametersEntrezQuery. */

void ncbiBlastParametersEntrezQuerySave(struct ncbiBlastParametersEntrezQuery *obj, int indent, FILE *f);
/* Save ncbiBlastParametersEntrezQuery to file. */

struct ncbiBlastParametersEntrezQuery *ncbiBlastParametersEntrezQueryLoad(char *fileName);
/* Load ncbiBlastParametersEntrezQuery from XML file where it is root element. */

struct ncbiBlastParametersEntrezQuery *ncbiBlastParametersEntrezQueryLoadNext(struct xap *xap);
/* Load next ncbiBlastParametersEntrezQuery element.  Use xapOpen to get xap. */

struct ncbiBlastStatistics
    {
    struct ncbiBlastStatistics *next;
    struct ncbiBlastStatisticsDbNum *ncbiBlastStatisticsDbNum;	/** Single instance required. **/
    struct ncbiBlastStatisticsDbLen *ncbiBlastStatisticsDbLen;	/** Single instance required. **/
    struct ncbiBlastStatisticsHspLen *ncbiBlastStatisticsHspLen;	/** Single instance required. **/
    struct ncbiBlastStatisticsEffSpace *ncbiBlastStatisticsEffSpace;	/** Single instance required. **/
    struct ncbiBlastStatisticsKappa *ncbiBlastStatisticsKappa;	/** Single instance required. **/
    struct ncbiBlastStatisticsLambda *ncbiBlastStatisticsLambda;	/** Single instance required. **/
    struct ncbiBlastStatisticsEntropy *ncbiBlastStatisticsEntropy;	/** Single instance required. **/
    };

void ncbiBlastStatisticsFree(struct ncbiBlastStatistics **pObj);
/* Free up ncbiBlastStatistics. */

void ncbiBlastStatisticsFreeList(struct ncbiBlastStatistics **pList);
/* Free up list of ncbiBlastStatistics. */

void ncbiBlastStatisticsSave(struct ncbiBlastStatistics *obj, int indent, FILE *f);
/* Save ncbiBlastStatistics to file. */

struct ncbiBlastStatistics *ncbiBlastStatisticsLoad(char *fileName);
/* Load ncbiBlastStatistics from XML file where it is root element. */

struct ncbiBlastStatistics *ncbiBlastStatisticsLoadNext(struct xap *xap);
/* Load next ncbiBlastStatistics element.  Use xapOpen to get xap. */

struct ncbiBlastStatisticsDbNum
    {
    struct ncbiBlastStatisticsDbNum *next;
    int text;
    };

void ncbiBlastStatisticsDbNumFree(struct ncbiBlastStatisticsDbNum **pObj);
/* Free up ncbiBlastStatisticsDbNum. */

void ncbiBlastStatisticsDbNumFreeList(struct ncbiBlastStatisticsDbNum **pList);
/* Free up list of ncbiBlastStatisticsDbNum. */

void ncbiBlastStatisticsDbNumSave(struct ncbiBlastStatisticsDbNum *obj, int indent, FILE *f);
/* Save ncbiBlastStatisticsDbNum to file. */

struct ncbiBlastStatisticsDbNum *ncbiBlastStatisticsDbNumLoad(char *fileName);
/* Load ncbiBlastStatisticsDbNum from XML file where it is root element. */

struct ncbiBlastStatisticsDbNum *ncbiBlastStatisticsDbNumLoadNext(struct xap *xap);
/* Load next ncbiBlastStatisticsDbNum element.  Use xapOpen to get xap. */

struct ncbiBlastStatisticsDbLen
    {
    struct ncbiBlastStatisticsDbLen *next;
    int text;
    };

void ncbiBlastStatisticsDbLenFree(struct ncbiBlastStatisticsDbLen **pObj);
/* Free up ncbiBlastStatisticsDbLen. */

void ncbiBlastStatisticsDbLenFreeList(struct ncbiBlastStatisticsDbLen **pList);
/* Free up list of ncbiBlastStatisticsDbLen. */

void ncbiBlastStatisticsDbLenSave(struct ncbiBlastStatisticsDbLen *obj, int indent, FILE *f);
/* Save ncbiBlastStatisticsDbLen to file. */

struct ncbiBlastStatisticsDbLen *ncbiBlastStatisticsDbLenLoad(char *fileName);
/* Load ncbiBlastStatisticsDbLen from XML file where it is root element. */

struct ncbiBlastStatisticsDbLen *ncbiBlastStatisticsDbLenLoadNext(struct xap *xap);
/* Load next ncbiBlastStatisticsDbLen element.  Use xapOpen to get xap. */

struct ncbiBlastStatisticsHspLen
    {
    struct ncbiBlastStatisticsHspLen *next;
    int text;
    };

void ncbiBlastStatisticsHspLenFree(struct ncbiBlastStatisticsHspLen **pObj);
/* Free up ncbiBlastStatisticsHspLen. */

void ncbiBlastStatisticsHspLenFreeList(struct ncbiBlastStatisticsHspLen **pList);
/* Free up list of ncbiBlastStatisticsHspLen. */

void ncbiBlastStatisticsHspLenSave(struct ncbiBlastStatisticsHspLen *obj, int indent, FILE *f);
/* Save ncbiBlastStatisticsHspLen to file. */

struct ncbiBlastStatisticsHspLen *ncbiBlastStatisticsHspLenLoad(char *fileName);
/* Load ncbiBlastStatisticsHspLen from XML file where it is root element. */

struct ncbiBlastStatisticsHspLen *ncbiBlastStatisticsHspLenLoadNext(struct xap *xap);
/* Load next ncbiBlastStatisticsHspLen element.  Use xapOpen to get xap. */

struct ncbiBlastStatisticsEffSpace
    {
    struct ncbiBlastStatisticsEffSpace *next;
    double text;
    };

void ncbiBlastStatisticsEffSpaceFree(struct ncbiBlastStatisticsEffSpace **pObj);
/* Free up ncbiBlastStatisticsEffSpace. */

void ncbiBlastStatisticsEffSpaceFreeList(struct ncbiBlastStatisticsEffSpace **pList);
/* Free up list of ncbiBlastStatisticsEffSpace. */

void ncbiBlastStatisticsEffSpaceSave(struct ncbiBlastStatisticsEffSpace *obj, int indent, FILE *f);
/* Save ncbiBlastStatisticsEffSpace to file. */

struct ncbiBlastStatisticsEffSpace *ncbiBlastStatisticsEffSpaceLoad(char *fileName);
/* Load ncbiBlastStatisticsEffSpace from XML file where it is root element. */

struct ncbiBlastStatisticsEffSpace *ncbiBlastStatisticsEffSpaceLoadNext(struct xap *xap);
/* Load next ncbiBlastStatisticsEffSpace element.  Use xapOpen to get xap. */

struct ncbiBlastStatisticsKappa
    {
    struct ncbiBlastStatisticsKappa *next;
    double text;
    };

void ncbiBlastStatisticsKappaFree(struct ncbiBlastStatisticsKappa **pObj);
/* Free up ncbiBlastStatisticsKappa. */

void ncbiBlastStatisticsKappaFreeList(struct ncbiBlastStatisticsKappa **pList);
/* Free up list of ncbiBlastStatisticsKappa. */

void ncbiBlastStatisticsKappaSave(struct ncbiBlastStatisticsKappa *obj, int indent, FILE *f);
/* Save ncbiBlastStatisticsKappa to file. */

struct ncbiBlastStatisticsKappa *ncbiBlastStatisticsKappaLoad(char *fileName);
/* Load ncbiBlastStatisticsKappa from XML file where it is root element. */

struct ncbiBlastStatisticsKappa *ncbiBlastStatisticsKappaLoadNext(struct xap *xap);
/* Load next ncbiBlastStatisticsKappa element.  Use xapOpen to get xap. */

struct ncbiBlastStatisticsLambda
    {
    struct ncbiBlastStatisticsLambda *next;
    double text;
    };

void ncbiBlastStatisticsLambdaFree(struct ncbiBlastStatisticsLambda **pObj);
/* Free up ncbiBlastStatisticsLambda. */

void ncbiBlastStatisticsLambdaFreeList(struct ncbiBlastStatisticsLambda **pList);
/* Free up list of ncbiBlastStatisticsLambda. */

void ncbiBlastStatisticsLambdaSave(struct ncbiBlastStatisticsLambda *obj, int indent, FILE *f);
/* Save ncbiBlastStatisticsLambda to file. */

struct ncbiBlastStatisticsLambda *ncbiBlastStatisticsLambdaLoad(char *fileName);
/* Load ncbiBlastStatisticsLambda from XML file where it is root element. */

struct ncbiBlastStatisticsLambda *ncbiBlastStatisticsLambdaLoadNext(struct xap *xap);
/* Load next ncbiBlastStatisticsLambda element.  Use xapOpen to get xap. */

struct ncbiBlastStatisticsEntropy
    {
    struct ncbiBlastStatisticsEntropy *next;
    double text;
    };

void ncbiBlastStatisticsEntropyFree(struct ncbiBlastStatisticsEntropy **pObj);
/* Free up ncbiBlastStatisticsEntropy. */

void ncbiBlastStatisticsEntropyFreeList(struct ncbiBlastStatisticsEntropy **pList);
/* Free up list of ncbiBlastStatisticsEntropy. */

void ncbiBlastStatisticsEntropySave(struct ncbiBlastStatisticsEntropy *obj, int indent, FILE *f);
/* Save ncbiBlastStatisticsEntropy to file. */

struct ncbiBlastStatisticsEntropy *ncbiBlastStatisticsEntropyLoad(char *fileName);
/* Load ncbiBlastStatisticsEntropy from XML file where it is root element. */

struct ncbiBlastStatisticsEntropy *ncbiBlastStatisticsEntropyLoadNext(struct xap *xap);
/* Load next ncbiBlastStatisticsEntropy element.  Use xapOpen to get xap. */

struct ncbiBlastHit
    {
    struct ncbiBlastHit *next;
    struct ncbiBlastHitNum *ncbiBlastHitNum;	/** Single instance required. **/
    struct ncbiBlastHitId *ncbiBlastHitId;	/** Single instance required. **/
    struct ncbiBlastHitDef *ncbiBlastHitDef;	/** Single instance required. **/
    struct ncbiBlastHitAccession *ncbiBlastHitAccession;	/** Single instance required. **/
    struct ncbiBlastHitLen *ncbiBlastHitLen;	/** Single instance required. **/
    struct ncbiBlastHitHsps *ncbiBlastHitHsps;	/** Optional (may be NULL). **/
    };

void ncbiBlastHitFree(struct ncbiBlastHit **pObj);
/* Free up ncbiBlastHit. */

void ncbiBlastHitFreeList(struct ncbiBlastHit **pList);
/* Free up list of ncbiBlastHit. */

void ncbiBlastHitSave(struct ncbiBlastHit *obj, int indent, FILE *f);
/* Save ncbiBlastHit to file. */

struct ncbiBlastHit *ncbiBlastHitLoad(char *fileName);
/* Load ncbiBlastHit from XML file where it is root element. */

struct ncbiBlastHit *ncbiBlastHitLoadNext(struct xap *xap);
/* Load next ncbiBlastHit element.  Use xapOpen to get xap. */

struct ncbiBlastHitNum
    {
    struct ncbiBlastHitNum *next;
    int text;
    };

void ncbiBlastHitNumFree(struct ncbiBlastHitNum **pObj);
/* Free up ncbiBlastHitNum. */

void ncbiBlastHitNumFreeList(struct ncbiBlastHitNum **pList);
/* Free up list of ncbiBlastHitNum. */

void ncbiBlastHitNumSave(struct ncbiBlastHitNum *obj, int indent, FILE *f);
/* Save ncbiBlastHitNum to file. */

struct ncbiBlastHitNum *ncbiBlastHitNumLoad(char *fileName);
/* Load ncbiBlastHitNum from XML file where it is root element. */

struct ncbiBlastHitNum *ncbiBlastHitNumLoadNext(struct xap *xap);
/* Load next ncbiBlastHitNum element.  Use xapOpen to get xap. */

struct ncbiBlastHitId
    {
    struct ncbiBlastHitId *next;
    char *text;
    };

void ncbiBlastHitIdFree(struct ncbiBlastHitId **pObj);
/* Free up ncbiBlastHitId. */

void ncbiBlastHitIdFreeList(struct ncbiBlastHitId **pList);
/* Free up list of ncbiBlastHitId. */

void ncbiBlastHitIdSave(struct ncbiBlastHitId *obj, int indent, FILE *f);
/* Save ncbiBlastHitId to file. */

struct ncbiBlastHitId *ncbiBlastHitIdLoad(char *fileName);
/* Load ncbiBlastHitId from XML file where it is root element. */

struct ncbiBlastHitId *ncbiBlastHitIdLoadNext(struct xap *xap);
/* Load next ncbiBlastHitId element.  Use xapOpen to get xap. */

struct ncbiBlastHitDef
    {
    struct ncbiBlastHitDef *next;
    char *text;
    };

void ncbiBlastHitDefFree(struct ncbiBlastHitDef **pObj);
/* Free up ncbiBlastHitDef. */

void ncbiBlastHitDefFreeList(struct ncbiBlastHitDef **pList);
/* Free up list of ncbiBlastHitDef. */

void ncbiBlastHitDefSave(struct ncbiBlastHitDef *obj, int indent, FILE *f);
/* Save ncbiBlastHitDef to file. */

struct ncbiBlastHitDef *ncbiBlastHitDefLoad(char *fileName);
/* Load ncbiBlastHitDef from XML file where it is root element. */

struct ncbiBlastHitDef *ncbiBlastHitDefLoadNext(struct xap *xap);
/* Load next ncbiBlastHitDef element.  Use xapOpen to get xap. */

struct ncbiBlastHitAccession
    {
    struct ncbiBlastHitAccession *next;
    char *text;
    };

void ncbiBlastHitAccessionFree(struct ncbiBlastHitAccession **pObj);
/* Free up ncbiBlastHitAccession. */

void ncbiBlastHitAccessionFreeList(struct ncbiBlastHitAccession **pList);
/* Free up list of ncbiBlastHitAccession. */

void ncbiBlastHitAccessionSave(struct ncbiBlastHitAccession *obj, int indent, FILE *f);
/* Save ncbiBlastHitAccession to file. */

struct ncbiBlastHitAccession *ncbiBlastHitAccessionLoad(char *fileName);
/* Load ncbiBlastHitAccession from XML file where it is root element. */

struct ncbiBlastHitAccession *ncbiBlastHitAccessionLoadNext(struct xap *xap);
/* Load next ncbiBlastHitAccession element.  Use xapOpen to get xap. */

struct ncbiBlastHitLen
    {
    struct ncbiBlastHitLen *next;
    int text;
    };

void ncbiBlastHitLenFree(struct ncbiBlastHitLen **pObj);
/* Free up ncbiBlastHitLen. */

void ncbiBlastHitLenFreeList(struct ncbiBlastHitLen **pList);
/* Free up list of ncbiBlastHitLen. */

void ncbiBlastHitLenSave(struct ncbiBlastHitLen *obj, int indent, FILE *f);
/* Save ncbiBlastHitLen to file. */

struct ncbiBlastHitLen *ncbiBlastHitLenLoad(char *fileName);
/* Load ncbiBlastHitLen from XML file where it is root element. */

struct ncbiBlastHitLen *ncbiBlastHitLenLoadNext(struct xap *xap);
/* Load next ncbiBlastHitLen element.  Use xapOpen to get xap. */

struct ncbiBlastHitHsps
    {
    struct ncbiBlastHitHsps *next;
    struct ncbiBlastHsp *ncbiBlastHsp;	/** Possibly empty list. **/
    };

void ncbiBlastHitHspsFree(struct ncbiBlastHitHsps **pObj);
/* Free up ncbiBlastHitHsps. */

void ncbiBlastHitHspsFreeList(struct ncbiBlastHitHsps **pList);
/* Free up list of ncbiBlastHitHsps. */

void ncbiBlastHitHspsSave(struct ncbiBlastHitHsps *obj, int indent, FILE *f);
/* Save ncbiBlastHitHsps to file. */

struct ncbiBlastHitHsps *ncbiBlastHitHspsLoad(char *fileName);
/* Load ncbiBlastHitHsps from XML file where it is root element. */

struct ncbiBlastHitHsps *ncbiBlastHitHspsLoadNext(struct xap *xap);
/* Load next ncbiBlastHitHsps element.  Use xapOpen to get xap. */

struct ncbiBlastHsp
    {
    struct ncbiBlastHsp *next;
    struct ncbiBlastHspNum *ncbiBlastHspNum;	/** Single instance required. **/
    struct ncbiBlastHspBitScore *ncbiBlastHspBitScore;	/** Single instance required. **/
    struct ncbiBlastHspScore *ncbiBlastHspScore;	/** Single instance required. **/
    struct ncbiBlastHspEvalue *ncbiBlastHspEvalue;	/** Single instance required. **/
    struct ncbiBlastHspQueryFrom *ncbiBlastHspQueryFrom;	/** Single instance required. **/
    struct ncbiBlastHspQueryTo *ncbiBlastHspQueryTo;	/** Single instance required. **/
    struct ncbiBlastHspHitFrom *ncbiBlastHspHitFrom;	/** Single instance required. **/
    struct ncbiBlastHspHitTo *ncbiBlastHspHitTo;	/** Single instance required. **/
    struct ncbiBlastHspPatternFrom *ncbiBlastHspPatternFrom;	/** Optional (may be NULL). **/
    struct ncbiBlastHspPatternTo *ncbiBlastHspPatternTo;	/** Optional (may be NULL). **/
    struct ncbiBlastHspQueryFrame *ncbiBlastHspQueryFrame;	/** Optional (may be NULL). **/
    struct ncbiBlastHspHitFrame *ncbiBlastHspHitFrame;	/** Optional (may be NULL). **/
    struct ncbiBlastHspIdentity *ncbiBlastHspIdentity;	/** Optional (may be NULL). **/
    struct ncbiBlastHspPositive *ncbiBlastHspPositive;	/** Optional (may be NULL). **/
    struct ncbiBlastHspGaps *ncbiBlastHspGaps;	/** Optional (may be NULL). **/
    struct ncbiBlastHspAlignLen *ncbiBlastHspAlignLen;	/** Optional (may be NULL). **/
    struct ncbiBlastHspDensity *ncbiBlastHspDensity;	/** Optional (may be NULL). **/
    struct ncbiBlastHspQseq *ncbiBlastHspQseq;	/** Single instance required. **/
    struct ncbiBlastHspHseq *ncbiBlastHspHseq;	/** Single instance required. **/
    struct ncbiBlastHspMidline *ncbiBlastHspMidline;	/** Optional (may be NULL). **/
    };

void ncbiBlastHspFree(struct ncbiBlastHsp **pObj);
/* Free up ncbiBlastHsp. */

void ncbiBlastHspFreeList(struct ncbiBlastHsp **pList);
/* Free up list of ncbiBlastHsp. */

void ncbiBlastHspSave(struct ncbiBlastHsp *obj, int indent, FILE *f);
/* Save ncbiBlastHsp to file. */

struct ncbiBlastHsp *ncbiBlastHspLoad(char *fileName);
/* Load ncbiBlastHsp from XML file where it is root element. */

struct ncbiBlastHsp *ncbiBlastHspLoadNext(struct xap *xap);
/* Load next ncbiBlastHsp element.  Use xapOpen to get xap. */

struct ncbiBlastHspNum
    {
    struct ncbiBlastHspNum *next;
    int text;
    };

void ncbiBlastHspNumFree(struct ncbiBlastHspNum **pObj);
/* Free up ncbiBlastHspNum. */

void ncbiBlastHspNumFreeList(struct ncbiBlastHspNum **pList);
/* Free up list of ncbiBlastHspNum. */

void ncbiBlastHspNumSave(struct ncbiBlastHspNum *obj, int indent, FILE *f);
/* Save ncbiBlastHspNum to file. */

struct ncbiBlastHspNum *ncbiBlastHspNumLoad(char *fileName);
/* Load ncbiBlastHspNum from XML file where it is root element. */

struct ncbiBlastHspNum *ncbiBlastHspNumLoadNext(struct xap *xap);
/* Load next ncbiBlastHspNum element.  Use xapOpen to get xap. */

struct ncbiBlastHspBitScore
    {
    struct ncbiBlastHspBitScore *next;
    double text;
    };

void ncbiBlastHspBitScoreFree(struct ncbiBlastHspBitScore **pObj);
/* Free up ncbiBlastHspBitScore. */

void ncbiBlastHspBitScoreFreeList(struct ncbiBlastHspBitScore **pList);
/* Free up list of ncbiBlastHspBitScore. */

void ncbiBlastHspBitScoreSave(struct ncbiBlastHspBitScore *obj, int indent, FILE *f);
/* Save ncbiBlastHspBitScore to file. */

struct ncbiBlastHspBitScore *ncbiBlastHspBitScoreLoad(char *fileName);
/* Load ncbiBlastHspBitScore from XML file where it is root element. */

struct ncbiBlastHspBitScore *ncbiBlastHspBitScoreLoadNext(struct xap *xap);
/* Load next ncbiBlastHspBitScore element.  Use xapOpen to get xap. */

struct ncbiBlastHspScore
    {
    struct ncbiBlastHspScore *next;
    double text;
    };

void ncbiBlastHspScoreFree(struct ncbiBlastHspScore **pObj);
/* Free up ncbiBlastHspScore. */

void ncbiBlastHspScoreFreeList(struct ncbiBlastHspScore **pList);
/* Free up list of ncbiBlastHspScore. */

void ncbiBlastHspScoreSave(struct ncbiBlastHspScore *obj, int indent, FILE *f);
/* Save ncbiBlastHspScore to file. */

struct ncbiBlastHspScore *ncbiBlastHspScoreLoad(char *fileName);
/* Load ncbiBlastHspScore from XML file where it is root element. */

struct ncbiBlastHspScore *ncbiBlastHspScoreLoadNext(struct xap *xap);
/* Load next ncbiBlastHspScore element.  Use xapOpen to get xap. */

struct ncbiBlastHspEvalue
    {
    struct ncbiBlastHspEvalue *next;
    double text;
    };

void ncbiBlastHspEvalueFree(struct ncbiBlastHspEvalue **pObj);
/* Free up ncbiBlastHspEvalue. */

void ncbiBlastHspEvalueFreeList(struct ncbiBlastHspEvalue **pList);
/* Free up list of ncbiBlastHspEvalue. */

void ncbiBlastHspEvalueSave(struct ncbiBlastHspEvalue *obj, int indent, FILE *f);
/* Save ncbiBlastHspEvalue to file. */

struct ncbiBlastHspEvalue *ncbiBlastHspEvalueLoad(char *fileName);
/* Load ncbiBlastHspEvalue from XML file where it is root element. */

struct ncbiBlastHspEvalue *ncbiBlastHspEvalueLoadNext(struct xap *xap);
/* Load next ncbiBlastHspEvalue element.  Use xapOpen to get xap. */

struct ncbiBlastHspQueryFrom
    {
    struct ncbiBlastHspQueryFrom *next;
    int text;
    };

void ncbiBlastHspQueryFromFree(struct ncbiBlastHspQueryFrom **pObj);
/* Free up ncbiBlastHspQueryFrom. */

void ncbiBlastHspQueryFromFreeList(struct ncbiBlastHspQueryFrom **pList);
/* Free up list of ncbiBlastHspQueryFrom. */

void ncbiBlastHspQueryFromSave(struct ncbiBlastHspQueryFrom *obj, int indent, FILE *f);
/* Save ncbiBlastHspQueryFrom to file. */

struct ncbiBlastHspQueryFrom *ncbiBlastHspQueryFromLoad(char *fileName);
/* Load ncbiBlastHspQueryFrom from XML file where it is root element. */

struct ncbiBlastHspQueryFrom *ncbiBlastHspQueryFromLoadNext(struct xap *xap);
/* Load next ncbiBlastHspQueryFrom element.  Use xapOpen to get xap. */

struct ncbiBlastHspQueryTo
    {
    struct ncbiBlastHspQueryTo *next;
    int text;
    };

void ncbiBlastHspQueryToFree(struct ncbiBlastHspQueryTo **pObj);
/* Free up ncbiBlastHspQueryTo. */

void ncbiBlastHspQueryToFreeList(struct ncbiBlastHspQueryTo **pList);
/* Free up list of ncbiBlastHspQueryTo. */

void ncbiBlastHspQueryToSave(struct ncbiBlastHspQueryTo *obj, int indent, FILE *f);
/* Save ncbiBlastHspQueryTo to file. */

struct ncbiBlastHspQueryTo *ncbiBlastHspQueryToLoad(char *fileName);
/* Load ncbiBlastHspQueryTo from XML file where it is root element. */

struct ncbiBlastHspQueryTo *ncbiBlastHspQueryToLoadNext(struct xap *xap);
/* Load next ncbiBlastHspQueryTo element.  Use xapOpen to get xap. */

struct ncbiBlastHspHitFrom
    {
    struct ncbiBlastHspHitFrom *next;
    int text;
    };

void ncbiBlastHspHitFromFree(struct ncbiBlastHspHitFrom **pObj);
/* Free up ncbiBlastHspHitFrom. */

void ncbiBlastHspHitFromFreeList(struct ncbiBlastHspHitFrom **pList);
/* Free up list of ncbiBlastHspHitFrom. */

void ncbiBlastHspHitFromSave(struct ncbiBlastHspHitFrom *obj, int indent, FILE *f);
/* Save ncbiBlastHspHitFrom to file. */

struct ncbiBlastHspHitFrom *ncbiBlastHspHitFromLoad(char *fileName);
/* Load ncbiBlastHspHitFrom from XML file where it is root element. */

struct ncbiBlastHspHitFrom *ncbiBlastHspHitFromLoadNext(struct xap *xap);
/* Load next ncbiBlastHspHitFrom element.  Use xapOpen to get xap. */

struct ncbiBlastHspHitTo
    {
    struct ncbiBlastHspHitTo *next;
    int text;
    };

void ncbiBlastHspHitToFree(struct ncbiBlastHspHitTo **pObj);
/* Free up ncbiBlastHspHitTo. */

void ncbiBlastHspHitToFreeList(struct ncbiBlastHspHitTo **pList);
/* Free up list of ncbiBlastHspHitTo. */

void ncbiBlastHspHitToSave(struct ncbiBlastHspHitTo *obj, int indent, FILE *f);
/* Save ncbiBlastHspHitTo to file. */

struct ncbiBlastHspHitTo *ncbiBlastHspHitToLoad(char *fileName);
/* Load ncbiBlastHspHitTo from XML file where it is root element. */

struct ncbiBlastHspHitTo *ncbiBlastHspHitToLoadNext(struct xap *xap);
/* Load next ncbiBlastHspHitTo element.  Use xapOpen to get xap. */

struct ncbiBlastHspPatternFrom
    {
    struct ncbiBlastHspPatternFrom *next;
    int text;
    };

void ncbiBlastHspPatternFromFree(struct ncbiBlastHspPatternFrom **pObj);
/* Free up ncbiBlastHspPatternFrom. */

void ncbiBlastHspPatternFromFreeList(struct ncbiBlastHspPatternFrom **pList);
/* Free up list of ncbiBlastHspPatternFrom. */

void ncbiBlastHspPatternFromSave(struct ncbiBlastHspPatternFrom *obj, int indent, FILE *f);
/* Save ncbiBlastHspPatternFrom to file. */

struct ncbiBlastHspPatternFrom *ncbiBlastHspPatternFromLoad(char *fileName);
/* Load ncbiBlastHspPatternFrom from XML file where it is root element. */

struct ncbiBlastHspPatternFrom *ncbiBlastHspPatternFromLoadNext(struct xap *xap);
/* Load next ncbiBlastHspPatternFrom element.  Use xapOpen to get xap. */

struct ncbiBlastHspPatternTo
    {
    struct ncbiBlastHspPatternTo *next;
    int text;
    };

void ncbiBlastHspPatternToFree(struct ncbiBlastHspPatternTo **pObj);
/* Free up ncbiBlastHspPatternTo. */

void ncbiBlastHspPatternToFreeList(struct ncbiBlastHspPatternTo **pList);
/* Free up list of ncbiBlastHspPatternTo. */

void ncbiBlastHspPatternToSave(struct ncbiBlastHspPatternTo *obj, int indent, FILE *f);
/* Save ncbiBlastHspPatternTo to file. */

struct ncbiBlastHspPatternTo *ncbiBlastHspPatternToLoad(char *fileName);
/* Load ncbiBlastHspPatternTo from XML file where it is root element. */

struct ncbiBlastHspPatternTo *ncbiBlastHspPatternToLoadNext(struct xap *xap);
/* Load next ncbiBlastHspPatternTo element.  Use xapOpen to get xap. */

struct ncbiBlastHspQueryFrame
    {
    struct ncbiBlastHspQueryFrame *next;
    int text;
    };

void ncbiBlastHspQueryFrameFree(struct ncbiBlastHspQueryFrame **pObj);
/* Free up ncbiBlastHspQueryFrame. */

void ncbiBlastHspQueryFrameFreeList(struct ncbiBlastHspQueryFrame **pList);
/* Free up list of ncbiBlastHspQueryFrame. */

void ncbiBlastHspQueryFrameSave(struct ncbiBlastHspQueryFrame *obj, int indent, FILE *f);
/* Save ncbiBlastHspQueryFrame to file. */

struct ncbiBlastHspQueryFrame *ncbiBlastHspQueryFrameLoad(char *fileName);
/* Load ncbiBlastHspQueryFrame from XML file where it is root element. */

struct ncbiBlastHspQueryFrame *ncbiBlastHspQueryFrameLoadNext(struct xap *xap);
/* Load next ncbiBlastHspQueryFrame element.  Use xapOpen to get xap. */

struct ncbiBlastHspHitFrame
    {
    struct ncbiBlastHspHitFrame *next;
    int text;
    };

void ncbiBlastHspHitFrameFree(struct ncbiBlastHspHitFrame **pObj);
/* Free up ncbiBlastHspHitFrame. */

void ncbiBlastHspHitFrameFreeList(struct ncbiBlastHspHitFrame **pList);
/* Free up list of ncbiBlastHspHitFrame. */

void ncbiBlastHspHitFrameSave(struct ncbiBlastHspHitFrame *obj, int indent, FILE *f);
/* Save ncbiBlastHspHitFrame to file. */

struct ncbiBlastHspHitFrame *ncbiBlastHspHitFrameLoad(char *fileName);
/* Load ncbiBlastHspHitFrame from XML file where it is root element. */

struct ncbiBlastHspHitFrame *ncbiBlastHspHitFrameLoadNext(struct xap *xap);
/* Load next ncbiBlastHspHitFrame element.  Use xapOpen to get xap. */

struct ncbiBlastHspIdentity
    {
    struct ncbiBlastHspIdentity *next;
    int text;
    };

void ncbiBlastHspIdentityFree(struct ncbiBlastHspIdentity **pObj);
/* Free up ncbiBlastHspIdentity. */

void ncbiBlastHspIdentityFreeList(struct ncbiBlastHspIdentity **pList);
/* Free up list of ncbiBlastHspIdentity. */

void ncbiBlastHspIdentitySave(struct ncbiBlastHspIdentity *obj, int indent, FILE *f);
/* Save ncbiBlastHspIdentity to file. */

struct ncbiBlastHspIdentity *ncbiBlastHspIdentityLoad(char *fileName);
/* Load ncbiBlastHspIdentity from XML file where it is root element. */

struct ncbiBlastHspIdentity *ncbiBlastHspIdentityLoadNext(struct xap *xap);
/* Load next ncbiBlastHspIdentity element.  Use xapOpen to get xap. */

struct ncbiBlastHspPositive
    {
    struct ncbiBlastHspPositive *next;
    int text;
    };

void ncbiBlastHspPositiveFree(struct ncbiBlastHspPositive **pObj);
/* Free up ncbiBlastHspPositive. */

void ncbiBlastHspPositiveFreeList(struct ncbiBlastHspPositive **pList);
/* Free up list of ncbiBlastHspPositive. */

void ncbiBlastHspPositiveSave(struct ncbiBlastHspPositive *obj, int indent, FILE *f);
/* Save ncbiBlastHspPositive to file. */

struct ncbiBlastHspPositive *ncbiBlastHspPositiveLoad(char *fileName);
/* Load ncbiBlastHspPositive from XML file where it is root element. */

struct ncbiBlastHspPositive *ncbiBlastHspPositiveLoadNext(struct xap *xap);
/* Load next ncbiBlastHspPositive element.  Use xapOpen to get xap. */

struct ncbiBlastHspGaps
    {
    struct ncbiBlastHspGaps *next;
    int text;
    };

void ncbiBlastHspGapsFree(struct ncbiBlastHspGaps **pObj);
/* Free up ncbiBlastHspGaps. */

void ncbiBlastHspGapsFreeList(struct ncbiBlastHspGaps **pList);
/* Free up list of ncbiBlastHspGaps. */

void ncbiBlastHspGapsSave(struct ncbiBlastHspGaps *obj, int indent, FILE *f);
/* Save ncbiBlastHspGaps to file. */

struct ncbiBlastHspGaps *ncbiBlastHspGapsLoad(char *fileName);
/* Load ncbiBlastHspGaps from XML file where it is root element. */

struct ncbiBlastHspGaps *ncbiBlastHspGapsLoadNext(struct xap *xap);
/* Load next ncbiBlastHspGaps element.  Use xapOpen to get xap. */

struct ncbiBlastHspAlignLen
    {
    struct ncbiBlastHspAlignLen *next;
    int text;
    };

void ncbiBlastHspAlignLenFree(struct ncbiBlastHspAlignLen **pObj);
/* Free up ncbiBlastHspAlignLen. */

void ncbiBlastHspAlignLenFreeList(struct ncbiBlastHspAlignLen **pList);
/* Free up list of ncbiBlastHspAlignLen. */

void ncbiBlastHspAlignLenSave(struct ncbiBlastHspAlignLen *obj, int indent, FILE *f);
/* Save ncbiBlastHspAlignLen to file. */

struct ncbiBlastHspAlignLen *ncbiBlastHspAlignLenLoad(char *fileName);
/* Load ncbiBlastHspAlignLen from XML file where it is root element. */

struct ncbiBlastHspAlignLen *ncbiBlastHspAlignLenLoadNext(struct xap *xap);
/* Load next ncbiBlastHspAlignLen element.  Use xapOpen to get xap. */

struct ncbiBlastHspDensity
    {
    struct ncbiBlastHspDensity *next;
    int text;
    };

void ncbiBlastHspDensityFree(struct ncbiBlastHspDensity **pObj);
/* Free up ncbiBlastHspDensity. */

void ncbiBlastHspDensityFreeList(struct ncbiBlastHspDensity **pList);
/* Free up list of ncbiBlastHspDensity. */

void ncbiBlastHspDensitySave(struct ncbiBlastHspDensity *obj, int indent, FILE *f);
/* Save ncbiBlastHspDensity to file. */

struct ncbiBlastHspDensity *ncbiBlastHspDensityLoad(char *fileName);
/* Load ncbiBlastHspDensity from XML file where it is root element. */

struct ncbiBlastHspDensity *ncbiBlastHspDensityLoadNext(struct xap *xap);
/* Load next ncbiBlastHspDensity element.  Use xapOpen to get xap. */

struct ncbiBlastHspQseq
    {
    struct ncbiBlastHspQseq *next;
    char *text;
    };

void ncbiBlastHspQseqFree(struct ncbiBlastHspQseq **pObj);
/* Free up ncbiBlastHspQseq. */

void ncbiBlastHspQseqFreeList(struct ncbiBlastHspQseq **pList);
/* Free up list of ncbiBlastHspQseq. */

void ncbiBlastHspQseqSave(struct ncbiBlastHspQseq *obj, int indent, FILE *f);
/* Save ncbiBlastHspQseq to file. */

struct ncbiBlastHspQseq *ncbiBlastHspQseqLoad(char *fileName);
/* Load ncbiBlastHspQseq from XML file where it is root element. */

struct ncbiBlastHspQseq *ncbiBlastHspQseqLoadNext(struct xap *xap);
/* Load next ncbiBlastHspQseq element.  Use xapOpen to get xap. */

struct ncbiBlastHspHseq
    {
    struct ncbiBlastHspHseq *next;
    char *text;
    };

void ncbiBlastHspHseqFree(struct ncbiBlastHspHseq **pObj);
/* Free up ncbiBlastHspHseq. */

void ncbiBlastHspHseqFreeList(struct ncbiBlastHspHseq **pList);
/* Free up list of ncbiBlastHspHseq. */

void ncbiBlastHspHseqSave(struct ncbiBlastHspHseq *obj, int indent, FILE *f);
/* Save ncbiBlastHspHseq to file. */

struct ncbiBlastHspHseq *ncbiBlastHspHseqLoad(char *fileName);
/* Load ncbiBlastHspHseq from XML file where it is root element. */

struct ncbiBlastHspHseq *ncbiBlastHspHseqLoadNext(struct xap *xap);
/* Load next ncbiBlastHspHseq element.  Use xapOpen to get xap. */

struct ncbiBlastHspMidline
    {
    struct ncbiBlastHspMidline *next;
    char *text;
    };

void ncbiBlastHspMidlineFree(struct ncbiBlastHspMidline **pObj);
/* Free up ncbiBlastHspMidline. */

void ncbiBlastHspMidlineFreeList(struct ncbiBlastHspMidline **pList);
/* Free up list of ncbiBlastHspMidline. */

void ncbiBlastHspMidlineSave(struct ncbiBlastHspMidline *obj, int indent, FILE *f);
/* Save ncbiBlastHspMidline to file. */

struct ncbiBlastHspMidline *ncbiBlastHspMidlineLoad(char *fileName);
/* Load ncbiBlastHspMidline from XML file where it is root element. */

struct ncbiBlastHspMidline *ncbiBlastHspMidlineLoadNext(struct xap *xap);
/* Load next ncbiBlastHspMidline element.  Use xapOpen to get xap. */

#endif /* NCBIBLAST_H */


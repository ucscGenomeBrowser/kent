/* eapGraph - stuff to help traverse the graph defined by the eapRun, eapInput, and eapOutput
 * tables that define what files were used to produce what other files */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "portable.h"
#include "intValTree.h"
#include "longToList.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapDb.h"
#include "eapLib.h"
#include "eapGraph.h"


struct eapGraph *eapGraphNew(struct sqlConnection *conn)
/* Return new eapGraph made by querying database */
{
struct eapGraph *eg;
AllocVar(eg);
char query[256];

/* Load up everything from database, logging time maybe */
sqlSafef(query, sizeof(query), "select * from eapRun");
eg->runList = eapRunLoadByQuery(conn, query);
sqlSafef(query, sizeof(query), "select * from eapInput");
eg->inputList = eapInputLoadByQuery(conn, query);
sqlSafef(query, sizeof(query), "select * from eapOutput");
eg->outputList = eapOutputLoadByQuery(conn, query);
sqlSafef(query, sizeof(query), "select * from edwFile");
eg->fileList = edwFileLoadByQuery(conn, query);
sqlSafef(query, sizeof(query), "select * from edwValidFile");
eg->validList = edwValidFileLoadByQuery(conn, query);

/* Set up runByExperiment */
struct hash *hash;
eg->runByExperiment = hash = hashNew(0);
struct eapRun *run;
for (run = eg->runList; run != NULL; run = run->next)
     hashAdd(hash, run->experiment, run);

/* Set up runById */
struct rbTree *ivt;
eg->runById = ivt = intValTreeNew();
for (run = eg->runList; run != NULL; run = run->next)
     intValTreeAdd(ivt, run->id, run);

/* Set up inputByFile */
struct longToList *lToL;
eg->inputByFile = lToL = longToListNew();
struct eapInput *in;
for (in = eg->inputList; in != NULL; in = in->next)
    longToListAdd(lToL, in->fileId, in);

/* Set up inputByRun */
eg->inputByRun = lToL = longToListNew();
for (in = eg->inputList; in != NULL; in = in->next)
    longToListAdd(lToL, in->runId, in);

/* Set up outputByFile - this one is singly-valued so easier */
eg->outputByFile = ivt = intValTreeNew();
struct eapOutput *out;
for (out = eg->outputList; out != NULL; out = out->next)
    intValTreeAdd(ivt, out->fileId, out);

/* Set up outputByRun */
eg->outputByRun = lToL = longToListNew();
for (out = eg->outputList; out != NULL; out = out->next)
    longToListAdd(lToL, out->runId, out);

/* Set up fileById */
eg->fileById = ivt = intValTreeNew();
struct edwFile *file;
for (file = eg->fileList; file != NULL; file = file->next)
    intValTreeAdd(ivt, file->id, file);

/* Set up validByFileId */
eg->validByFileId = ivt = intValTreeNew();
struct edwValidFile *valid;
for (valid = eg->validList; valid != NULL; valid = valid->next)
    intValTreeAdd(ivt, valid->fileId, valid);
    
/* Set up validByExperiment */
eg->validByExperiment = hash = hashNew(0);
for (valid = eg->validList; valid != NULL; valid = valid->next)
    hashAdd(hash, valid->experiment, valid);

return eg;
}


void eapGraphFree(struct eapGraph **pEg)
/* Free up resources associated with graph */
{
struct eapGraph *eg = *pEg;
if (eg == NULL)
    return;

/* Free up the 5 lists we own */
eapRunFreeList(&eg->runList);
eapInputFreeList(&eg->inputList);
eapOutputFreeList(&eg->outputList);
edwFileFreeList(&eg->fileList);
edwValidFileFreeList(&eg->validList);

/* Free up all 9 subcontainers, in same order as the corresponding fields are declared. */
hashFree(&eg->runByExperiment);
rbTreeFree(&eg->runById);
longToListFree(&eg->inputByFile);
longToListFree(&eg->inputByRun);
rbTreeFree(&eg->outputByFile);
longToListFree(&eg->outputByRun);
rbTreeFree(&eg->fileById);
rbTreeFree(&eg->validByFileId);
hashFree(&eg->validByExperiment);

/* Finally free up self and set to NULL */
freez(pEg);
}

/* Routines to fetch out files given fileIds */

struct edwValidFile *eapGraphValidFromId(struct eapGraph *eg, unsigned fileId)
/* Return edwValidFile given  fileId */
{
return intValTreeMustFind(eg->validByFileId, fileId);
}

struct edwFile *eapGraphFileFromId(struct eapGraph *eg, unsigned fileId)
/* Return edwFile given ID */
{
return intValTreeMustFind(eg->fileById, fileId);
}

struct slRef *eapGraphRunInputs(struct eapGraph *eg, unsigned runId)
/* Fetch all inputs to this run.  Vals on slRef are eapInputs. 
 * Do not free this list, it is owned by graph. */
{
return longToListFindVal(eg->inputByRun, runId);
}

struct slRef *eapGraphRunOutputs(struct eapGraph *eg, unsigned runId)
/* Fetch all outputs to this run.  Vals on slRef are eapOutputs.   
 * Do not free this list, it is owned by graph. */
{
return longToListFindVal(eg->outputByRun, runId);
}

struct slRef *eapGraphParentList(struct eapGraph *eg, unsigned fileId)
/* Return list of all parents, possibly NULL.  List is in form of slRefs with eapInput vals. 
 * Do not free this list, it is owned by graph. */
{
struct eapOutput *out = intValTreeFind(eg->outputByFile, fileId);
if (out == NULL)
    return NULL;
return longToListFindVal(eg->inputByRun, out->runId);
}


unsigned eapGraphAnyParent(struct eapGraph *eg, unsigned fileId)
/* Return fileId of a parent chosen at convenience, zero if none */
{
struct slRef *ref = eapGraphParentList(eg, fileId);
if (ref == NULL)
    return 0;
struct eapInput *in = ref->val;
return in->fileId;
}

unsigned eapGraphSingleParent(struct eapGraph *eg, unsigned fileId)
/* Return fileId of a parent, and make sure no more than one.  Returns zero if none. */
{
struct slRef *ref = eapGraphParentList(eg, fileId);
if (ref == NULL)
    return 0;
if (ref->next != NULL)
    errAbort("Multiple parents for %u in eapGraphSingleParent", fileId);
struct eapInput *in = ref->val;
return in->fileId;
}

unsigned eapGraphOneSingleParent(struct eapGraph *eg, unsigned fileId)
/* Get file ID of parent gauranteeing that it exists and there is just one or aborting. */
{
unsigned id = eapGraphSingleParent(eg, fileId);
if (id == 0)
    errAbort("%u doesn't exist in eapGraphOneSingleParent", fileId);
return id;
}

unsigned eapGraphAnyAncestorOfFormat(struct eapGraph *eg, unsigned fileId, char *format)
/* Return fileId of first convenient ancestor of given format */
{
struct slRef *ref, *refList = eapGraphParentList(eg, fileId);

/* We do breadth first searching */
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct eapInput *in = ref->val;
    struct edwValidFile *valid = eapGraphValidFromId(eg, in->fileId);
    if (sameString(valid->format, format))
        return valid->fileId;
    }

/* If we got to here then we recurse */
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct eapInput *in = ref->val;
    unsigned id = eapGraphAnyAncestorOfFormat(eg, in->fileId, format);
    if (id != 0)
        return id;
    }

/* Got to here, nothing left to search, just have to return 0 */
return 0;
}


void eapGraphAncestorsOfFormat(struct eapGraph *eg, unsigned fileId, 
    char *format, int maxGenerations, struct slRef **retList)
/* Return list of all ancestors of format.  Set maxGenerations to -1 for any number of
 * generations back,   otherwise 1 will stop at parents, 2 at grandparents, etc. The vals
 * on the returned list are eapInputs. This returned value should be slFreeList()'d when
 * done. */
{
/* Guard against excessive depth.  If they give us a negative maxGeneration will not be true ever */
if (maxGenerations == 0)
    return;
maxGenerations -= 1;

/* Look through parents. */
struct slRef *ref, *refList = eapGraphParentList(eg, fileId);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct eapInput *in = ref->val;
    struct edwValidFile *valid = eapGraphValidFromId(eg, in->fileId);
    if (sameString(valid->format, format))
	{
	refAdd(retList, in);
	}
    eapGraphAncestorsOfFormat(eg, in->fileId, format, maxGenerations, retList);
    }
}

unsigned eapGraphAnyChild(struct eapGraph *eg, unsigned fileId)
/* Return fileId of a child chosen at convenience, zero if none */
{
struct slRef *refList = eapGraphChildList(eg, fileId);
if (refList == NULL)
    return 0;
struct eapInput *in = refList->val;
unsigned child = in->fileId;
slFreeList(&refList);
return child;
}

unsigned eapGraphSingleChild(struct eapGraph *eg, unsigned fileId)
/* Return fileId of a child, and make sure no more than one.  Returns zero if none. */
{
struct slRef *refList = eapGraphChildList(eg, fileId);
if (refList == NULL)
   return 0;
if (refList->next != NULL)
   errAbort("Multiple children for %u in eapGraphSingleChild", fileId);
struct eapInput *in = refList->val;
unsigned child = in->fileId;
slFreeList(&refList);
return child;
}

unsigned eapGraphOneSingleChild(struct eapGraph *eg, unsigned fileId)
/* Get file ID of child gauranteeing that it exists and there is just one or aborting. */
{
unsigned id = eapGraphSingleChild(eg, fileId);
if (id == 0)
    errAbort("Can't find child for %u in eapGraphOnSingleChild", fileId);
return id;
}

struct slRef *eapGraphChildList(struct eapGraph *eg, unsigned fileId)
/* Return list of all children, possibly NULL.  List is in form of slRefs with eapOutput vals. 
 * You can slFreeList result when done. */
{
struct slRef *outList = NULL;
struct slRef *in, *inList = longToListFindVal(eg->inputByFile, fileId);
for (in = inList; in != NULL; in = in->next)
    {
    struct eapInput *input = in->val;
    unsigned runId = input ->runId;
    struct slRef *runOut, *runOutList = longToListFindVal(eg->outputByRun, runId);
    for (runOut = runOutList; runOut != NULL; runOut = runOut->next)
        {
	struct eapOutput *output = runOut->val;
	refAdd(&outList, output);
	}
    }
return outList;
}

unsigned eapGraphAnyDescendantOfFormat(struct eapGraph *eg, unsigned fileId, char *format)
/* Return first convenient descendant of given format */
{
unsigned result = 0;
struct slRef *ref, *refList = eapGraphChildList(eg, fileId);

/* We do breadth first searching */
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct eapOutput *out = ref->val;
    struct edwValidFile *valid = eapGraphValidFromId(eg, out->fileId);
    if (sameString(valid->format, format))
	{
	result = valid->fileId;
	break;
	}
    }

/* If we got to here then we recurse */
if (result == 0)
    {
    for (ref = refList; ref != NULL; ref = ref->next)
	{
	struct eapOutput *out = ref->val;
	unsigned id = eapGraphAnyDescendantOfFormat(eg, out->fileId, format);
	if (id != 0)
	    {
	    result = id;
	    break;
	    }
	}
    }

/* Clean up and return result */
slFreeList(&refList);
return result;
}

void eapGraphDescendantsOfFormat(struct eapGraph *eg, unsigned fileId, 
    char *format, int maxGenerations, struct slRef **retList)
/* Return list of all descendants of format.  Set maxGenerations to -1 for any number of
 * generations back,   otherwise 1 will stop at children, 2 at grandchildren, etc. The vals
 * on the returned list are eapOutputs. This returned value should be slFreeList()'d when
 * done. */
{
/* Guard against excessive depth.  If they give us a negative maxGeneration will not be true ever */
if (maxGenerations == 0)
    return;
maxGenerations -= 1;

/* Look through children. */
struct slRef *ref, *refList = eapGraphChildList(eg, fileId);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct eapOutput *out = ref->val;
    struct edwValidFile *valid = eapGraphValidFromId(eg, out->fileId);
    if (sameString(valid->format, format))
	{
	refAdd(retList, out);
	}
    eapGraphDescendantsOfFormat(eg, out->fileId, format, maxGenerations, retList);
    }
slFreeList(&refList);  // This little bit of cleanup not needed for ancestors
}

void rEapGraphAllAncestors(struct eapGraph *eg, unsigned fileId, struct slRef **retList)
/* Recursively add all ancestors. */
{
struct slRef *parent, *parentList = eapGraphParentList(eg, fileId);
for (parent = parentList; parent != NULL; parent = parent->next)
    {
    struct eapOutput *out = parent->val;
    refAdd(retList, out);
    rEapGraphAllAncestors(eg, out->fileId, retList);
    }
}

struct slRef *eapGraphAllAncestors(struct eapGraph *eg, unsigned fileId)
/* Return list of all ancestors.  Values of slRef are eapOutputs from which you can
 * harvest either ancestral fileIds or analysis runIds. */
{
/* In implementation this is just a minor wrapper around recursive routine. */
struct slRef *list = NULL;
rEapGraphAllAncestors(eg, fileId, &list);
return list;
}


void rEapGraphAllDescendants(struct eapGraph *eg, unsigned fileId, struct slRef **retList)
/* Recursively add all descendants. */
{
struct slRef *child, *childList = eapGraphChildList(eg, fileId);
for (child = childList; child != NULL; child = child->next)
    {
    struct eapInput *in = child->val;
    refAdd(retList, in);
    rEapGraphAllDescendants(eg, in->fileId, retList);
    }
slFreeList(&childList);
}

struct slRef *eapGraphAllDescendants(struct eapGraph *eg, unsigned fileId)
/* Return list of all ancestors.  Values of slRef are eapOutputs from which you can
 * harvest either ancestral fileIds or analysis runIds. */
{
/* In implementation this is just a minor wrapper around recursive routine. */
struct slRef *list = NULL;
rEapGraphAllDescendants(eg, fileId, &list);
return list;
}




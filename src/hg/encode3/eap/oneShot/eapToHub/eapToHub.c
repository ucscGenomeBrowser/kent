/* eapToHub - Convert some analysis results to a hub for easy viz.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "htmshell.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapDb.h"
#include "eapLib.h"
#include "eapGraph.h"

char *assemblyArray[] = {"hg19", /* "mm9" */ };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapToHub - Convert some analysis results to a hub for easy viz.\n"
  "usage:\n"
  "   eapToHub outHubDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void writeGenomesTxt(struct sqlConnection *conn, char *outFile)
/* Write genome.txt part of hub */
{
FILE *f = mustOpen(outFile, "w");
int i;
for (i=0; i<ArraySize(assemblyArray); ++i)
    {
    char *a = assemblyArray[i];
    fprintf(f, "genome %s\n", a);
    fprintf(f, "trackDb %s/trackDb.txt\n", a);
    fprintf(f, "\n");
    }
carefulClose(&f);
}

void writeHubTxt(struct sqlConnection *conn, char *outFile)
/* Write out simple hub.txt file. */
{
FILE *f = mustOpen(outFile, "w");
fprintf(f, "hub eap_dnase_hotspot_01\n");
fprintf(f, "shortLabel EAP DNase x1\n");
fprintf(f, "longLabel EAP DNase results including BWA alignments and Hotspot signal and peak output.\n");
fprintf(f, "genomesFile genomes.txt\n");
fprintf(f, "email genome@soe.ucsc.edu\n");
carefulClose(&f);
}

/* After the genome.txt and hub.txt it gets harder.   Here's a bunch of data structures
 * to help us build up trackDb.txt */

struct replicate
/* Info on one replicate */
    {
    struct replicate *next;
    char *name;	/* Replicate name, usually "1" or "2" */
    struct vFile *bamList;   /* List of bam format files made by pipeline */
    struct vFile *bigWigList;	/* List of wig format files made by pipeline  */
    struct vFile *narrowList;/* Narrow peaks made by pipeline  */
    struct vFile *broadList; /* Broad peaks made by pipeline  */
    };


struct fullExperiment
/* An experiment that has enough data for us to work with */
    {
    struct fullExperiment *next;
    char *name;			    /* Same as exp->accession */
    struct edwExperiment *exp;	    /* Experiment from edwExperiment table */
    struct replicate *repList;	    /* Replicate list */
    };

struct outputType
/* List of output types */
    {
    struct outputType *next;
    char *name;
    };

struct vFile
/* Combination of file + validFile */
    {
    struct vFile *next;
    struct edwFile *file;
    struct edwValidFile *valid;
    struct eapOutput *eapOutput;
    };

void addOutputsFromFiles(struct outputType **pList, struct hash *hash, struct vFile *vfList)
/* Add outputs from vFileList to hash/pList if they are not there already */
{
struct vFile *vf;
for (vf = vfList; vf != NULL; vf = vf->next)
    {
    char *outputType = vf->valid->outputType;
    if (!hashLookup(hash, outputType))
        {
	struct outputType *ot;
	AllocVar(ot);
	hashAddSaveName(hash, outputType, ot, &ot->name);
	slAddHead(pList, ot);
	}
    }
}

struct outputType *outputTypesForExpList(struct fullExperiment *expList, struct hash *hash)
/* Get list of output types associated with any experiment in list.  Fill these with
 * files from the experiments */
{
struct outputType *otList = NULL;
struct fullExperiment *exp;
for (exp = expList; exp != NULL; exp = exp->next)
    {
    struct replicate *rep;
    for (rep = exp->repList; rep != NULL; rep = rep->next)
        {
	addOutputsFromFiles(&otList, hash, rep->bamList);
	addOutputsFromFiles(&otList, hash, rep->bigWigList);
	addOutputsFromFiles(&otList, hash, rep->narrowList);
	addOutputsFromFiles(&otList, hash, rep->broadList);
	}
    }
slReverse(&otList);
return otList;
}

struct slName *getReplicateNames(struct fullExperiment *expList)
/* Return list of all replicate names */
{
struct slName *repList = NULL;
struct fullExperiment *exp;
for (exp = expList; exp != NULL; exp = exp->next)
    {
    struct replicate *rep;
    for (rep = exp->repList; rep != NULL; rep = rep->next)
        {
	char *name = rep->name;
	if (isEmpty(name))
	    name = "pooled";
	slNameStore(&repList, name);
	}
    }
slSort(&repList, slNameCmpStringsWithEmbeddedNumbers);
return repList;
}

boolean viewableOutput(char *output)
/* Returns TRUE if this is a viewable output type */
{
return differentWord(output, "reads");
}

char *symbolify(char *string)
/* Turn things not alphanumeric to _.  Not reentrant */
{
static char buf[256];
int len = strlen(string);
if (len >= sizeof(buf))
    errAbort("String '%s' too long in symbolify", string);
char *s = string, *d = buf, c;
while ((c = *s++) != 0)
    {
    if (!isalnum(c))
        c = '_';
    *d++ = c;
    }
*d=0;
return buf;  // Nonreentrant but this is probably one-time-use code
}

char *trackTypeForOutputType(char *outputType)
/* Given an output type try and come up with trackType that would work. */
{
char buf[256];
if (sameString(outputType, "alignments"))
    {
    safef(buf, sizeof(buf), "bam");
    }
else if (sameString(outputType, "hotspot_broad_peaks"))
    {
    safef(buf, sizeof(buf), "bigBed");
    }
else if (sameString(outputType, "hotspot_signal"))
    {
    safef(buf, sizeof(buf), "bigWig");
    }
else if (sameString(outputType, "hotspot_narrow_peaks"))
    {
    safef(buf, sizeof(buf), "bigBed");
    }
else if (sameString(outputType, "macs2_dnase_peaks"))
    {
    safef(buf, sizeof(buf), "bigBed");
    }
else if (sameString(outputType, "macs2_dnase_signal"))
    {
    safef(buf, sizeof(buf), "bigBed");
    }
else if (sameString(outputType, "pooled_signal"))
    {
    safef(buf, sizeof(buf), "bigWig");
    }
else if (sameString(outputType, "replicated_broadPeak"))
    {
    safef(buf, sizeof(buf), "bigBed");
    }
else if (sameString(outputType, "replicated_narrowPeak"))
    {
    safef(buf, sizeof(buf), "bigBed");
    }
else
    {
    errAbort("Unrecognized outputType %s in trackTypeForOutputType", outputType);
    }
return cloneString(buf);
}

char *shortOutputType(char *outputType)
/* Return a shortened version of outputType if possible */
{
if (sameString(outputType, "pooled_signal"))
    return "signal";
else if (sameString(outputType, "replicated_narrowPeak"))
    return "narrow";
else if (sameString(outputType, "replicated_broadPeak"))
    return "broad";
else if (sameString(outputType, "alignments"))
    return "bam";
else if (sameString(outputType, "hotspot_signal"))
    return "signal";
else if (sameString(outputType, "hotspot_narrow_peaks"))
    return "narrow";
else if (sameString(outputType, "hotspot_broad_peaks"))
    return "broad";
else
    return outputType;
}

void outputMatchingTrack(char *parentName, struct fullExperiment *exp, struct replicate *rep, 
    char *outputType, struct vFile *vf,  char *indent, FILE *f)
/* If vf is of outputType then write a track based around it to f */
{
if (vf != NULL)
    {
    if (sameString(outputType, vf->valid->outputType))
	{
	char *repName = rep->name;
	if (isEmpty(repName))
	    repName = "pooled";
	fprintf(f, "%strack %s\n", indent, vf->valid->licensePlate);
	char *biosample = exp->exp->biosample;
	fprintf(f, "%sshortLabel %s %s %c\n", indent, biosample, 
		shortOutputType(outputType), repName[0]);
	fprintf(f, "%sparent %s\n", indent, parentName);
	fprintf(f, "%slongLabel %s %s replicate %s from %s %s\n", 
	    indent, biosample, exp->exp->dataType, repName, exp->exp->lab, vf->valid->licensePlate);
	fprintf(f, "%ssubGroups view=%s biosample=%s replicate=%s\n", 
		indent, outputType, biosample, repName);
	fprintf(f, "%sbigDataUrl /warehouse/%s\n", indent, vf->file->edwFileName);
	fprintf(f, "%stype %s\n", indent, trackTypeForOutputType(outputType));
	fprintf(f, "\n");
	}
    }
}

boolean vfMatchOutput(struct vFile *vf, char *outputType)
/* Return TRUE if vf's outputType same as outputType */
{
if (vf == NULL)
    return FALSE;
return sameString(vf->valid->outputType, outputType);
}

boolean anyMatchOutputType(struct fullExperiment *expList, char *outputType)
/* Return TRUE if any replicated of any experiment has a member matching outputType */
{
struct fullExperiment *exp;
struct replicate *rep;
for (exp = expList; exp != NULL; exp = exp->next)
    {
    for (rep = exp->repList; rep != NULL; rep = rep->next)
	{
	if (vfMatchOutput(rep->bamList, outputType))
	    return TRUE;
	if (vfMatchOutput(rep->bigWigList, outputType))
	    return TRUE;
	if (vfMatchOutput(rep->narrowList, outputType))
	    return TRUE;
	if (vfMatchOutput(rep->broadList, outputType))
	    return TRUE;
	}
    }
return FALSE;
}


void writeView(struct sqlConnection *conn, char *rootName, char *outputType,
    struct fullExperiment *expList, FILE *f)
/* Write one view - the view stanza and all the actual track stanzas */
{
if (!anyMatchOutputType(expList, outputType))
     return;
char *cappedOutputType = cloneString(outputType);
cappedOutputType[0] = toupper(cappedOutputType[0]);
char viewTrackName[256];
safef(viewTrackName, sizeof(viewTrackName), "%sView%s", rootName, cappedOutputType);
fprintf(f, "    track %s\n", viewTrackName);
fprintf(f, "    shortLabel %s\n", outputType);
fprintf(f, "    view %s\n", outputType);
fprintf(f, "    parent %s\n", rootName);
fprintf(f, "\n");

struct fullExperiment *exp;
for (exp = expList; exp != NULL; exp = exp->next)
    {
    struct replicate *rep;
    for (rep = exp->repList; rep != NULL; rep = rep->next)
        {
	outputMatchingTrack(viewTrackName, exp, rep, outputType, rep->bamList, "        ", f);
	outputMatchingTrack(viewTrackName, exp, rep, outputType, rep->bigWigList, "        ", f);
	outputMatchingTrack(viewTrackName, exp, rep, outputType, rep->narrowList, "        ", f);
	outputMatchingTrack(viewTrackName, exp, rep, outputType, rep->broadList, "        ", f);
	}
    }
}

void writeTrackDb(struct sqlConnection *conn, struct eapGraph *eg, struct fullExperiment *expList, 
    struct slName *bioList, char *outRa)
/* Write out trackDb info */
{
FILE *f = mustOpen(outRa, "w");
struct hash *otHash = hashNew(0);
struct outputType *ot, *otList = outputTypesForExpList(expList, otHash);
verbose(1, "%d outputTypes\n", slCount(otList));

/* Write out a bunch of easy fields in top level stanza */
char *rootName = "dnase3";
fprintf(f, "track %s\n", rootName);
fprintf(f, "compositeTrack on\n");
fprintf(f, "shortLabel EAP DNase\n");
fprintf(f, "longLabel EAP DNase on hg19 using Hotspot5 and BWA 0.7.0\n");
fprintf(f, "group regulation\n");

/* Group 1 - the views */
fprintf(f, "subGroup1 view Views");
for (ot = otList; ot != NULL; ot = ot->next)
    {
    if (viewableOutput(ot->name) && anyMatchOutputType(expList, ot->name))
	fprintf(f, " %s=%s", ot->name, ot->name);
    }
fprintf(f, "\n");

/* Group 2 the cellTypes */
fprintf(f, "subGroup2 biosample Biosamples");
struct slName *bio;
for (bio = bioList; bio != NULL; bio = bio->next)
    {
    fprintf(f, " %s=%s", symbolify(bio->name), bio->name);
    }
fprintf(f, "\n");

/* Group 3, the replicates */
fprintf(f, "subGroup3 replicate Replicates");
struct slName *rep, *repList = getReplicateNames(expList);
for (rep = repList; rep != NULL; rep = rep->next)
     fprintf(f, " %s=%s", symbolify(rep->name), rep->name);
fprintf(f, "\n");

/* Some more scalar fields in first stanza */
fprintf(f, "sortOrder biosamples=+ replicates=+ view=+\n");
fprintf(f, "dimensions dimensionY=biosample dimensionX=replicate\n");
fprintf(f, "dragAndDrop subTracks\n");
fprintf(f, "noInherit on\n");
fprintf(f, "configurable on\n");
fprintf(f, "type bigBed\n");  // Need something, seems like even if ignored
fprintf(f, "\n");

/* Now move on to views */
for (ot = otList; ot != NULL; ot = ot->next)
    {
    writeView(conn, rootName, ot->name, expList, f);
    }

carefulClose(&f);
}

struct vFile *vFileNew(struct edwFile *file, struct edwValidFile *valid, 
    struct eapOutput *eapOutput)
/* Build new vFile around file and valid */
{
struct vFile *vf;
AllocVar(vf);
vf->file = file;
vf->valid = valid;
vf->eapOutput = eapOutput;
return vf;
}

struct vFile *moveToHead(struct vFile *newHead, struct vFile *list)
/* Move newHead to head of list */
{
if (newHead != list)
     {
     slRemoveEl(&list, newHead);
     newHead->next = list;
     }
return newHead;
}

struct vFile *findRelativeInList(struct sqlConnection *conn, unsigned runId, struct vFile *list,
    char *table)
/* Return first file if any in list connected to runId by table.  Table is eapInput or eapOutput */
{
struct vFile *result = NULL;
char query[256];
sqlSafef(query, sizeof(query), "select fileId from %s where runId=%u", table, runId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while (result == NULL && (row = sqlNextRow(sr)) != NULL)
    {
    struct vFile *vf;
    unsigned fileId = sqlUnsigned(row[0]);
    for (vf = list; vf != NULL; vf = vf->next)
        {
	if (vf->file->id == fileId)
	    {
	    result = vf;
	    break;
	    }
	}
    }
sqlFreeResult(&sr);
return result;
}

struct vFile *findChildInList(struct sqlConnection *conn, unsigned runId, struct vFile *list)
/* Given a list full of possibilities, find the one that connects to runId.  Return NULL
 * if none of them do. */
{
return findRelativeInList(conn, runId, list, "eapOutput");
}

struct vFile *findParentInList(struct sqlConnection *conn, unsigned runId, struct vFile *list)
/* Given a list full of possibilities, find the one that connects to runId.  Return NULL
 * if none of them do. */
{
return findRelativeInList(conn, runId, list, "eapInput");
}


boolean replicateIsComplete(struct sqlConnection *conn, struct replicate *r)
/* Return TRUE only if have enough to process replicate.  If this returns true
 * then the head of the bamList will point to a pipeline generated alignment,
 * and the bigWig/narrow/broad lists will start with files generated from it. */
{
// Rule out obviously incomplete ones, missing an expected part.
if (r->broadList == NULL || r->narrowList == NULL || r->bigWigList == NULL || r->bamList == NULL)
    return FALSE;

// Now we know all we have all the pieces.  Sadly we don't know if they all fit together.
// Figure if there is a way that has taken the analysis pipeline the whole way from
// bwa->hotspot.  

// We start with broad peaks because they are relatively rare, only generated by hotspot
struct vFile *vf;
for (vf = r->broadList; vf != NULL; vf = vf->next)
    {
    unsigned runId = vf->eapOutput->runId;
    struct vFile *parentBam = findParentInList(conn, runId, r->bamList);
    uglyf("    fileId %u, runId %u, parentBam %p\n", vf->file->id, runId, parentBam);
    if (parentBam != NULL)
        {
	struct vFile *bigWig = findChildInList(conn, runId, r->bigWigList);
	struct vFile *narrow = findChildInList(conn, runId, r->narrowList);
	uglyf("    parent->runId %u, bigWig %p, narrow %p\n", runId, bigWig, narrow);
	if (bigWig != NULL && narrow != NULL)
	    {
	    // We found a compatable way.  Move all players to the head of their respective lists.
	    r->bamList = moveToHead(parentBam, r->bamList);
	    r->bigWigList = moveToHead(bigWig, r->bigWigList);
	    r->narrowList = moveToHead(narrow, r->narrowList);
	    r->broadList = moveToHead(vf, r->broadList);
	    return TRUE;
	    }
	}
    }
return FALSE;
}

struct replicate *findOrMakeReplicate(char *name, struct replicate **pList)
/* Find replicate of given name on list.  If it's not there make up new replicate
 * with that name and add it to head of list.  In any case return replicate with the
 * given name. */
{
struct replicate *rep;
for (rep = *pList; rep != NULL; rep = rep->next)
    if (sameString(rep->name, name))
        return rep;

/* Couldn't find it, we have to make it. */
AllocVar(rep);
rep->name = cloneString(name);
slAddHead(pList, rep);
return rep;
}

void fixOutputType(struct edwValidFile *valid)
/* Convert pooled_signal to hotspot_signal */
{
if (sameString("pooled_signal", valid->outputType))
   valid->outputType = cloneString("hotspot_signal");
}

struct fullExperiment *getFullExperimentList(struct sqlConnection *conn,
    struct edwExperiment *eeList, char *assembly, struct hash **retHash)
/* Given list of edwExperiments, return list of ones replicated with full file sets on
 * both replicates.  If optional retHash is non-NULL then return a hash full of same 
 * experiments keyed by experiment accession */
{
/* Build up a list of fullExperiments and a hash keyed by name. */
struct hash *hash = hashNew(14);
struct fullExperiment *fullList = NULL;
struct edwExperiment *ee;
for (ee = eeList; ee != NULL; ee = ee->next)
    {
    struct fullExperiment *full = hashFindVal(hash, ee->accession);
    if (full == NULL)
        {
	AllocVar(full);
	full->name = cloneString(ee->accession);
	full->exp = ee;
	slAddHead(&fullList, full);
	hashAdd(hash, full->name, full);
	}
    }
uglyf("Got %d in eeList, %d in fullList, %d in hash\n", slCount(eeList), slCount(fullList), hash->elCount);

/* Build up SQL query to efficiently fetch all good files and valid files from our experiment */
struct dyString *q = dyStringNew(16*1024);
sqlDyStringPrintf(q, "select edwValidFile.*,edwFile.*,eapOutput.* "
		  " from edwValidFile,edwFile,eapOutput "
                  " where edwValidFile.fileId = edwFile.id and edwFile.id = eapOutput.fileId "
		  " and edwFile.deprecated='' and edwFile.errorMessage='' "
		  " and edwValidFile.ucscDb != 'centro.hg19' "
		  " and edwValidFile.ucscDb like '%%%s' and edwValidFile.experiment in ("
		  , assembly);
for (ee = eeList; ee != NULL; ee = ee->next)
    {
    dyStringPrintf(q, "'%s'", ee->accession);
    if (ee->next != NULL)
        dyStringAppendC(q, ',');
    }
dyStringAppendC(q, ')');

/* Loop through this making up vFiles that ultimately are attached to replicates. */
int vCount = 0;
struct sqlResult *sr = sqlGetResult(conn, q->string);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++vCount;
    struct edwValidFile *valid = edwValidFileLoad(row);
    fixOutputType(valid);
    struct edwFile *file = edwFileLoad(row + EDWVALIDFILE_NUM_COLS);
    struct eapOutput *eapOutput = eapOutputLoad(row + EDWVALIDFILE_NUM_COLS + EDWFILE_NUM_COLS);
    struct vFile *vf = vFileNew(file, valid, eapOutput);
    struct fullExperiment *full = hashMustFindVal(hash, valid->experiment);
    struct replicate *rep = findOrMakeReplicate(valid->replicate, &full->repList);
    char *format = valid->format;
    if (sameString(format, "bam"))
        slAddHead(&rep->bamList, vf);
    else if (sameString(format, "bigWig"))
        slAddHead(&rep->bigWigList, vf);
    else if (sameString(format, "narrowPeak") && !sameString(valid->outputType, "replicated_narrowPeak"))
        slAddHead(&rep->narrowList, vf);
    else if (sameString(format, "broadPeak") && !sameString(valid->outputType, "replicated_broadPeak"))
        slAddHead(&rep->broadList, vf);
    }
sqlFreeResult(&sr);
uglyf("Got %d vFiles\n", vCount);

dyStringFree(&q);

/* Free hash or return it, and return list. */
if (retHash == NULL)
    hashFree(&hash);
else
    *retHash = hash;
return fullList;
}

struct edwExperiment *getUwDnaseExps(struct sqlConnection *conn)
/* Get list of UW DNase experiments */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from edwExperiment where dataType='DNase-seq' "
    " and lab in ('UW', 'John Stamatoyannopoulos, UW')");
return edwExperimentLoadByQuery(conn, query);
}

struct fullExperiment *getGoodExperiments(struct sqlConnection *conn, struct edwExperiment *eeList,
    char *assembly)
/* Get list of experiments that are good in the sense of being replicated and having
 * bam,broadPeak and so forth all from hotspot  in the same run */
{
uglyf("Got %d on eeList\n", slCount(eeList));
struct fullExperiment *fullList = getFullExperimentList(conn, eeList, assembly, NULL);
uglyf("Got %d on fullList\n", slCount(fullList));
int replicatedCount = 0, goodCount = 0, goodIndividualRepCount = 0, totalRepCount = 0;
struct fullExperiment *full, *next, *goodList = NULL, *badList = NULL;
for (full = fullList; full != NULL; full = next)
     {
     next = full->next;
     int repCount = slCount(full->repList);
     totalRepCount += repCount;
     if (repCount > 0)
         {
	 uglyf("%s %d\n", full->exp->accession, repCount);
	 if (repCount > 1)
	     ++replicatedCount;
	 int goodRepCount = 0;
	 struct replicate *r;
	 for (r = full->repList; r != NULL; r = r->next)
	     {
	     if (replicateIsComplete(conn, r))
	         {
		 ++goodRepCount;
		 ++goodIndividualRepCount;
		 }
	     }
	 if (goodRepCount >= 2)
	     {
	     ++goodCount;
	     slAddHead(&goodList, full);
	     }
         else
	     slAddHead(&badList, full);
	 }
     }
slReverse(&goodList);
uglyf("%d replicates %d are good\n", totalRepCount, goodIndividualRepCount);
uglyf("%d replicated experiments,  %d of which look good\n", replicatedCount, goodCount);

return goodList;
}


void writeTrackDbTxt(struct sqlConnection *conn, struct eapGraph *eg, 
    struct fullExperiment *expList, char *assembly, char *outRa)
/* Write out a trackDb that represents the replicated hotspot experiments we have on the 
 * given assembly */
{
uglyf("%d in expList\n", slCount(expList));

/* Make up a list and hash of unique biosamples. */
struct hash *bioHash = hashNew(0);
struct slName *bioList=NULL;
struct fullExperiment *exp;
for (exp = expList; exp != NULL; exp = exp->next)
    {
    char *name = exp->exp->biosample;
    if (!hashLookup(bioHash, name))
        {
	slNameAddHead(&bioList, name);
	hashAdd(bioHash, name, NULL);
	}
    }
slSort(&bioList, slNameCmp);
writeTrackDb(conn, eg, expList, bioList, outRa);
}

struct replicate *findReplicateInList(struct replicate *list, char *name)
/* Find replicate of given name on list */
{
struct replicate *rep;
for (rep = list; rep != NULL; rep = rep->next)
    {
    if (sameString(name, rep->name))
        return rep;
    }
return NULL;
}

struct replicate *mustFindReplicateInList(struct replicate *list, char *name)
/* Find next replicate of given name in list or die trying */
{
struct replicate *rep = findReplicateInList(list, name);
if (rep == NULL)
    errAbort("mustFindReplicateInList couldn't find %s in list of %d", name, slCount(list));
return rep;
}

struct replicate *mustFindPooledReplicate(struct replicate *list)
/* Find pooled replicate - not sure honestly if it is called '' or 'pooled' */
{
struct replicate *pooled = findReplicateInList(list, "");
if (pooled == NULL)
    pooled = mustFindReplicateInList(list, "pooled");
return pooled;
}

struct edwBamFile *bamFilesAncestralToFile(struct sqlConnection *conn, struct eapGraph *eg, 
    long long fileId)
/* Given a fileId return list of all bamFiles that led to it */
{
struct slRef *bamRef, *bamRefList = NULL;
eapGraphAncestorsOfFormat(eg, fileId, "bam", -1, &bamRefList);
struct edwBamFile *bamFileList = NULL;
for (bamRef = bamRefList; bamRef != NULL; bamRef = bamRef->next)
    {
    struct eapInput *bamInput = bamRef->val;
    struct edwBamFile *bamFile = edwBamFileFromFileId(conn, bamInput->fileId);
    slAddHead(&bamFileList, bamFile);
    }
slFreeList(&bamRefList);
return bamFileList;
}

long long pooledBroadPeaksForExp(struct fullExperiment *exp)
/* Attempt to find the pooled broad peak file associated with experiment. */
{
struct replicate *pooled = mustFindPooledReplicate(exp->repList);
struct vFile *broad;
int bestScore = 0;
struct vFile *bestFile = NULL;
for (broad = pooled->broadList; broad != NULL; broad = broad->next)
    {
    int oneScore = 1;
    if (sameString(broad->valid->outputType, "hotspot_broad_peaks"))
        oneScore += 100;
    if (oneScore > bestScore)
        {
	bestScore = oneScore;
	bestFile = broad;
	}
    }
if (bestFile == NULL)
    return 0;
return bestFile->valid->fileId;
}

void bamListStats(struct edwBamFile *bamList, 
    long long *retReadCount, long long *retMappedCount, long long *retUniqueMappedCount
    ,double *retU4mRatio)
/* Gather statistics on a list of bams. */
{
long long readCount = 0, mappedCount = 0, uniqueMappedCount = 0;
long long u4mReadCountTotal = 0, u4mUniquePosTotal = 0, fileCount = 0;
struct edwBamFile *bamFile;
for (bamFile = bamList; bamFile != NULL; bamFile = bamFile->next)
    {
    readCount += bamFile->readCount;
    mappedCount += bamFile->mappedCount;
    uniqueMappedCount = bamFile->uniqueMappedCount;
    u4mReadCountTotal += bamFile->u4mReadCount;
    u4mUniquePosTotal += bamFile->u4mUniquePos;
    ++fileCount;
    }
*retReadCount = readCount;
*retMappedCount = mappedCount;
*retUniqueMappedCount = uniqueMappedCount;
*retU4mRatio = (double)u4mUniquePosTotal/u4mReadCountTotal;
}

int fullExperimentCmp(const void *va, const void *vb)
/* Compare two fullExperiments. */
{
const struct fullExperiment *a = *((struct fullExperiment **)va);
const struct fullExperiment *b = *((struct fullExperiment **)vb);
return strcmp(a->exp->biosample, b->exp->biosample);
}
#ifdef SOON
#endif /* SOON */

struct edwQaEnrich *edwQaEnrichFind(struct sqlConnection *conn, long long fileId, char *target)
/* Given a file and a target, return corresponding edwQaEnrich record if any */
{
char query[256];
sqlSafef(query, sizeof(query),
    "select edwQaEnrich.* from edwQaEnrich "
    " join edwQaEnrichTarget on qaEnrichTargetId=edwQaEnrichTarget.id "
    " where edwQaEnrich.fileId = %lld and edwQaEnrichTarget.name = '%s' "
    " order by enrichment desc limit 1"
    ,  fileId, target);
// uglyf really?
return edwQaEnrichLoadByQuery(conn, query);
}

void printEnrichment(struct sqlConnection *conn, long long fileId, char *target, FILE *f)
/* Get enrichment for file and target and print it if possible. */
{
struct edwQaEnrich *enrich = edwQaEnrichFind(conn, fileId, target);
if (enrich != NULL)
    {
    fprintf(f, "<TD>%4.2fx</TD>\n", enrich->enrichment);
    fprintf(f, "<TD>%4.3f</TD>\n", enrich->coverage);
    }
else
    {
    fprintf(f, "<TD>n/a</TD>\n");
    fprintf(f, "<TD>n/a</TD>\n");
    }
edwQaEnrichFree(&enrich);
}

boolean uglyOne;

struct edwQaPairCorrelation *edwQaPairCorrelationFromFiles(struct sqlConnection *conn,
	    long long aFileId, long long bFileId)
/* Return edwQaPairSampleOverlap if any that exists in database betweeen two files */
{
char query[256];
sqlSafef(query, sizeof(query), 
	"select * from edwQaPairCorrelation where (elderFileId=%lld and youngerFileId=%lld) "
	" or (elderFileId = %lld and youngerFileId=%lld) "
	, aFileId, bFileId, bFileId, aFileId);
return edwQaPairCorrelationLoadByQuery(conn, query);
}

struct edwQaPairSampleOverlap *edwQaPairSampleOverlapFromFiles(struct sqlConnection *conn,
	    long long aFileId, long long bFileId)
/* Return edwQaPairSampleOverlap if any that exists in database betweeen two files */
{
char query[256];
sqlSafef(query, sizeof(query), 
	"select * from edwQaPairSampleOverlap where (elderFileId=%lld and youngerFileId=%lld) "
	" or (elderFileId = %lld and youngerFileId=%lld) "
	, aFileId, bFileId, bFileId, aFileId);
return edwQaPairSampleOverlapLoadByQuery(conn, query);
}

struct edwQaPairSampleOverlap *findSampleOverlap(struct sqlConnection *conn, 
	struct eapGraph *eg, long long pooledFileId)
/* Return sampleOverlap between two replicates given fileId of a pooling of 2 replicates */
{
struct edwQaPairSampleOverlap *result = NULL;
struct slRef *refList = NULL;
eapGraphAncestorsOfFormat(eg, pooledFileId, "bam", -1, &refList);
if (refList != NULL && refList->next != NULL)
    {
    struct slRef *aRef = refList, *bRef = refList->next;
    struct eapInput *aIn = aRef->val, *bIn = bRef->val;
    result = edwQaPairSampleOverlapFromFiles(conn, aIn->fileId, bIn->fileId);
    }
slFreeList(&refList);
return result;
}

struct edwQaPairCorrelation *findCorrelation(struct sqlConnection *conn, 
	struct eapGraph *eg, long long pooledFileId)
/* Return pairCorrelation between two replicates given fileId of a pooling of 2 replicates */
{
struct edwQaPairCorrelation *result = NULL;
#ifdef SOON
struct slRef *refList = NULL;
eapGraphAncestorsOfFormat(eg, pooledFileId, "bam", -1, &refList);
if (refList != NULL && refList->next != NULL)
    {
    struct slRef *aRef = refList, *bRef = refList->next;
    struct eapInput *aIn = aRef->val, *bIn = bRef->val;
    result = edwQaPairCorrelation(conn, aIn->fileId, bIn->fileId);
    }
slFreeList(&refList);
#endif /* SOON */
return result;
}

struct edwBamFile *makeBamFileList(struct sqlConnection *conn, struct vFile *vFileList)
/* Given vFile list make bamFile list */
{
struct edwBamFile *bam, *bamList = NULL;
struct vFile *vFile;
for (vFile = vFileList; vFile != NULL; vFile = vFile->next)
    {
    bam = edwBamFileFromFileId(conn, vFile->file->id);
    slAddHead(&bamList, bam);
    }
slReverse(&bamList);
return bamList;
}

void printOneRep(struct sqlConnection *conn, struct eapGraph *eg,
    struct fullExperiment *exp, struct replicate *rep, char *color, FILE *f)
{
long long readCount = 0, mappedCount = 0, uniqueMappedCount = 0;
double u4mUniqueRatio = 0;
struct edwBamFile *bamFileList = makeBamFileList(conn, rep->bamList);
bamListStats(bamFileList, &readCount, &mappedCount, &uniqueMappedCount, &u4mUniqueRatio);
fprintf(f, "<TD BGCOLOR='%s'>%0.1fM</TD>\n",  color, readCount * 1.0e-6);
fprintf(f, "<TD BGCOLOR='%s'>%4.1f%%</TD>\n", color, 100.0 * mappedCount / readCount);
fprintf(f, "<TD BGCOLOR='%s'>%4.1f%%</TD>\n", color, 100.0 * uniqueMappedCount / readCount);
fprintf(f, "<TD BGCOLOR='%s'>%4.1f%%</TD>\n", color, 100.0 * u4mUniqueRatio);
int bamFileId = bamFileList->fileId;
char query[256];
safef(query, sizeof(query), "select * from edwQaDnaseSingleStats5m where fileId=%u limit 1", bamFileId);
struct edwQaDnaseSingleStats5m *stats = edwQaDnaseSingleStats5mLoadByQuery(conn, query);
if (stats != NULL)
    {
    fprintf(f, "<TD BGCOLOR='%s'>%4.2f</TD>\n", color, stats->spotRatio);
    fprintf(f, "<TD BGCOLOR='%s'>%4.1fx</TD>\n", color, stats->enrichment);
    fprintf(f, "<TD BGCOLOR='%s'>%4.2f</TD>\n", color, stats->nsc);
    fprintf(f, "<TD BGCOLOR='%s'>%4.2f</TD>\n", color, stats->rsc);
    }
else
    {
    fprintf(f, "<TD BGCOLOR='%s'>n/a</TD>\n", color);
    fprintf(f, "<TD BGCOLOR='%s'>n/a</TD>\n", color);
    fprintf(f, "<TD BGCOLOR='%s'>n/a</TD>\n", color);
    fprintf(f, "<TD BGCOLOR='%s'>n/a</TD>\n", color);
    }
edwQaDnaseSingleStats5mFree(&stats);
}

struct edwQaWigSpot *findSpot(struct sqlConnection *conn, long long fileId)
/* Find best spot score if any associated with given spot/peak file */
{
char query[256];
sqlSafef(query, sizeof(query),
    "select * from edwQaWigSpot where spotId=%lld order by spotRatio desc limit 1", fileId);
return edwQaWigSpotLoadByQuery(conn, query);
}

long long totalReadsForExp(struct sqlConnection *conn, struct fullExperiment *exp, 
    struct edwExperiment *ee, struct eapGraph *eg)
/* Return total number of reads in both good replicates of this experiment */
{
boolean uglyOne = TRUE;
if (uglyOne) uglyf("Got you %s %s\n", exp->name, exp->exp->biosample);
// We start with broad peaks because they are relatively rare, only generated by hotspot
long long total = 0;
long long pooledPeakId = pooledBroadPeaksForExp(exp);
if (uglyOne) uglyf("Broad is %lld\n", pooledPeakId);
struct slRef *ref, *refList = NULL;
eapGraphAncestorsOfFormat(eg, pooledPeakId, "fastq", -1, &refList);
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct eapInput *in = ref->val;
    struct edwValidFile *vf = edwValidFileFromFileId(conn, in->fileId);
    if (uglyOne) uglyf("Fastq is %u, reads %lld\n", vf->fileId, (long long)vf->itemCount);
    total += vf->itemCount;
    }
return total;
}

void writeTableHtml(struct sqlConnection *conn, struct eapGraph *eg, 
    struct fullExperiment *expList, char *assembly, char *outFile)
/* Write out a table in simple html based on eeList*/
{
char *lightGreen = "#D8FFD8";
char *brightWhite = "#FFFFFF";
FILE *f = mustOpen(outFile, "w");
htmStart(f, "DNase-seq experiment table");
fprintf(f, "<TABLE>\n");
fprintf(f, "<TR>\n");
fprintf(f, "<TH></TH>");
fprintf(f, "<TH>promo</TH>");
fprintf(f, "<TH>promo</TH>");
fprintf(f, "<TH>open</TH>");
fprintf(f, "<TH>open</TH>");
fprintf(f, "<TH>cross</TH>");
fprintf(f, "<TH>&nbsp; </TH>");
fprintf(f, "<TH></TH>");
fprintf(f, "<TH></TH>");
fprintf(f, "<TH></TH>");
fprintf(f, "<TH></TH>");
fprintf(f, "<TH></TH>");
fprintf(f, "<TH></TH>");
fprintf(f, "<TH></TH>");
fprintf(f, "</TR>\n");
fprintf(f, "<TR>\n");
fprintf(f, "<TH>Biosample</TH>");
fprintf(f, "<TH>enrich</TH>");
fprintf(f, "<TH>cover</TH>");
fprintf(f, "<TH>enrich</TH>");
fprintf(f, "<TH>cover</TH>");
fprintf(f, "<TH>enrich</TH>");
fprintf(f, "<TH>&nbsp; </TH>");
fprintf(f, "<TH BGCOLOR='%s'>reads(1)</TH>", lightGreen);
fprintf(f, "<TH BGCOLOR='%s'>map(1)</TH>", lightGreen);
fprintf(f, "<TH BGCOLOR='%s'>umap(1)</TH>", lightGreen);
fprintf(f, "<TH BGCOLOR='%s'>U4M(1)</TH>", lightGreen);
fprintf(f, "<TH BGCOLOR='%s'>Spot(1)</TH>", lightGreen);
fprintf(f, "<TH BGCOLOR='%s'>SpotX(1)</TH>", lightGreen);
fprintf(f, "<TH BGCOLOR='%s'>NSC(1)</TH>", lightGreen);
fprintf(f, "<TH BGCOLOR='%s'>RSC(1)</TH>", lightGreen);
fprintf(f, "<TH>&nbsp; </TH>");
fprintf(f, "<TH>reads(2)</TH>");
fprintf(f, "<TH>map(2)</TH>");
fprintf(f, "<TH>umap(2)</TH>");
fprintf(f, "<TH>U4M(2)</TH>");
fprintf(f, "<TH>Spot(2)</TH>");
fprintf(f, "<TH>SpotX(2)</TH>");
fprintf(f, "<TH>NSC(2)</TH>");
fprintf(f, "<TH>RSC(2)</TH>");
fprintf(f, "</TR>\n");
struct fullExperiment *exp;
for (exp = expList; exp != NULL; exp = exp->next)
    {
    struct edwExperiment *ee = exp->exp;
    fprintf(f, "<TR>\n");
    fprintf(f, "<TD>%s</TD>\n", ee->biosample);
    long long pooledPeakId = pooledBroadPeaksForExp(exp);
    if (pooledPeakId > 0)
	{
	struct edwBamFile *bamFileList = bamFilesAncestralToFile(conn, eg, pooledPeakId);
	long long readCount = 0, mappedCount = 0, uniqueMappedCount = 0;
	double u4mUniqueRatio = 0;
	bamListStats(bamFileList, &readCount, &mappedCount, &uniqueMappedCount, &u4mUniqueRatio);
	printEnrichment(conn, pooledPeakId, "promoter", f);
	printEnrichment(conn, pooledPeakId, "open", f);
	}
    else
        {
	fprintf(f, "<TD>n/a</TD>\n");
	fprintf(f, "<TD>n/a</TD>\n");
	fprintf(f, "<TD>n/a</TD>\n");
	fprintf(f, "<TD>n/a</TD>\n");
	fprintf(f, "<TD>n/a</TD>\n");
	fprintf(f, "<TD>n/a</TD>\n");
	fprintf(f, "<TD>n/a</TD>\n");
	}
    uglyOne = (pooledPeakId == 74961);
    struct edwQaPairSampleOverlap *so = findSampleOverlap(conn, eg, pooledPeakId);
    if (so != NULL)
        {
	fprintf(f, "<TD>%4.2fx</TD>\n", so->sampleSampleEnrichment);
	}
    else
        {
	uglyf("Trouble finding sample/overlap for file %lld exp %s\n", pooledPeakId, exp->name);
	fprintf(f, "<TD>n/a</TD>\n");
	}
    fprintf(f, "<TD>&nbsp;</TD>\n");
    printOneRep(conn, eg, exp, mustFindReplicateInList(exp->repList, "1"), lightGreen, f);
    fprintf(f, "<TH>&nbsp; </TH>");
    printOneRep(conn, eg, exp, mustFindReplicateInList(exp->repList, "2"), brightWhite, f);
    fprintf(f, "</TR>\n");
    }
fprintf(f, "</TABLE>\n");
htmEnd(f);
carefulClose(&f);
}

void writeFastqTab(struct sqlConnection *conn, struct eapGraph *eg, 
    struct fullExperiment *expList, char *assembly, char *outFile)
/* Write out a table in simple html based on eeList*/
{
FILE *f = mustOpen(outFile, "w");
struct fullExperiment *exp;
for (exp = expList; exp != NULL; exp = exp->next)
    {
    long long pooledPeakId = pooledBroadPeaksForExp(exp);
    if (pooledPeakId > 0)
	{
	struct slRef *ref, *refList = NULL;
	eapGraphAncestorsOfFormat(eg, pooledPeakId, "fastq", -1, &refList);
	for (ref = refList; ref != NULL; ref = ref->next)
	    {
	    struct eapInput *in = ref->val;
	    struct edwFile *file = edwFileFromId(conn, in->fileId);
	    fprintf(f, "%s\t%s\t%s\n", file->edwFileName, exp->name, exp->exp->biosample);
	    edwFileFree(&file);
	    }
	slFreeList(&refList);
	}
    }

carefulClose(&f);
}

void eapToHub(char *outDir)
/* eapToHub - Convert some analysis results to a hub for easy viz.. */
{
struct sqlConnection *conn = eapConnect();
struct eapGraph *eg = eapGraphNew(conn);
makeDirsOnPath(outDir);
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", outDir, "hub.txt");
writeHubTxt(conn, path);
safef(path, sizeof(path), "%s/%s", outDir, "genomes.txt");
writeGenomesTxt(conn, path);
int i;
struct edwExperiment *eeList = getUwDnaseExps(conn);
for (i=0; i<ArraySize(assemblyArray); ++i)
    {
    char *a = assemblyArray[i];
    safef(path, sizeof(path), "%s/%s", outDir, a);
    makeDir(path);
    safef(path, sizeof(path), "%s/%s/%s", outDir, a, "trackDb.txt");
    struct fullExperiment *expList = getGoodExperiments(conn, eeList, a);
    slSort(&expList, fullExperimentCmp);
    writeTrackDbTxt(conn, eg, expList, a, path);
    safef(path, sizeof(path), "%s/%s/%s", outDir, a, "table.html");
    writeTableHtml(conn, eg, expList, a, path);
    safef(path, sizeof(path), "%s/%s/%s", outDir, a, "fastq.tab");
    writeFastqTab(conn, eg, expList, a, path);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
eapToHub(argv[1]);
return 0;
}

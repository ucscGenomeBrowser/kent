/* eapToHub - Convert some analysis results to a hub for easy viz.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapDb.h"
#include "eapLib.h"

char *assemblyArray[] = {"hg19", "mm9"};

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

void writeTrackDb(struct sqlConnection *conn, struct fullExperiment *expList, 
    struct slName *bioList, char *outFile)
/* Write out trackDb info */
{
FILE *f = mustOpen(outFile, "w");
struct hash *otHash = hashNew(0);
struct outputType *ot, *otList = outputTypesForExpList(expList, otHash);
verbose(1, "%d outputTypes\n", slCount(otList));

char *rootName = "dnaseTop";
fprintf(f, "track %s\n", rootName);
fprintf(f, "compositeTrack on\n");
fprintf(f, "shortLabel EAP DNase\n");
fprintf(f, "longLabel EAP DNase on hg19 using Hotspot5 and BWA 0.7.0\n");
fprintf(f, "group regulation\n");
fprintf(f, "subGroup1 views Views");
for (ot = otList; ot != NULL; ot = ot->next)
    {
    if (viewableOutput(ot->name))
	fprintf(f, " %s=%s", ot->name, ot->name);
    }
fprintf(f, "\n");
fprintf(f, "subGroup2 biosamples Biosamples");
struct slName *bio;
for (bio = bioList; bio != NULL; bio = bio->next)
    {
    fprintf(f, " %s=%s", symbolify(bio->name), bio->name);
    }
fprintf(f, "\n");

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
uglyf("  replicateIsComplete: broad %d, narrow %d, bigWig %d, bam %d\n", slCount(r->broadList), slCount(r->narrowList), slCount(r->bigWigList), slCount(r->bamList));
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
    else if (sameString(format, "narrowPeak"))
        slAddHead(&rep->narrowList, vf);
    else if (sameString(format, "broadPeak"))
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


void writeTrackDbTxt(struct sqlConnection *conn, struct edwExperiment *eeList, 
    char *assembly, char *outFile)
/* Write out a trackDb that represents the replicated hotspot experiments we have on the 
 * given assembly */
{
struct fullExperiment *expList = getGoodExperiments(conn, eeList, assembly);
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
writeTrackDb(conn, expList, bioList, outFile);
}


void eapToHub(char *outDir)
/* eapToHub - Convert some analysis results to a hub for easy viz.. */
{
struct sqlConnection *conn = eapConnect();
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
    writeTrackDbTxt(conn, eeList, a, path);
    uglyAbort("All for now");
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
#ifdef SOON
#endif /* SOON */

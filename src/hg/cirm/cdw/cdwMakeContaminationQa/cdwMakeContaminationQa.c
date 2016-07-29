/* cdwMakeContaminationQa - Screen for contaminants by aligning against contaminant genomes.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "options.h"
#include "sqlNum.h"
#include "jksql.h"
#include "basicBed.h"
#include "genomeRangeTree.h"
#include "correlate.h"
#include "hmmstats.h"
#include "portable.h"
#include "cdw.h"
#include "cdwLib.h"

boolean keepTemp = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwMakeContaminationQa - Screen for contaminants by aligning against contaminant genomes.\n"
  "usage:\n"
  "   cdwMakeContaminationQa startId endId\n"
  "where startId and endId are id's in the cdwFile table\n"
  "options:\n"
  "   -keepTemp\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"keepTemp", OPTION_BOOLEAN},
   {NULL, 0},
};

struct cdwFile *cdwFileLoadIdRange(struct sqlConnection *conn, long long startId, long long endId)
/* Return list of all files in given id range */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from cdwFile where id>=%lld and id<=%lld and endUploadTime != 0 "
    "and updateTime != 0 and deprecated = ''", 
    startId, endId);
return cdwFileLoadByQuery(conn, query);
}

#define FASTQ_SAMPLE_SIZE 100000

int cdwQaContamMade(struct sqlConnection *conn, long long fileId, int targetId)
/* Return number of times have fileId paired with targetId in cdwQaContam table. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select count(*) from cdwQaContam where fileId=%lld and qaContamTargetId=%d",
    fileId, targetId);
return sqlQuickNum(conn, query);
}

struct cdwQaContamTarget *getContamTargets(struct sqlConnection *conn, 
    struct cdwFile *ef, struct cdwValidFile *vf)
/* Get list of contamination targets for file - basically all targets that aren't in same
 * taxon as self. */
{
assert(vf->ucscDb != NULL);
struct cdwAssembly *origAsm = cdwAssemblyForUcscDb(conn, vf->ucscDb);
assert(origAsm != NULL);
char query[256];
sqlSafef(query, sizeof(query), 
    "select cdwQaContamTarget.* from cdwQaContamTarget,cdwAssembly "
    "where cdwQaContamTarget.assemblyId = cdwAssembly.id "
         " and cdwAssembly.taxon != %d", origAsm->taxon);
struct cdwQaContamTarget *targetList  = cdwQaContamTargetLoadByQuery(conn, query);
cdwAssemblyFree(&origAsm);
return targetList;
}

void screenFastqForContaminants(struct sqlConnection *conn, 
    struct cdwFile *ef, struct cdwValidFile *vf)
/* The ef/vf point to same file, which is fastq format.  Set alignments up for a sample against all
 * contamination targets. */
{
/* Get target list and see if we have any work to do. */
struct cdwQaContamTarget *target, *targetList;
targetList = getContamTargets(conn, ef, vf);
boolean needScreen = FALSE;
for (target = targetList; target != NULL; target = target->next)
    {
    if (cdwQaContamMade(conn, ef->id, target->id) <= 0)
        {
	needScreen = TRUE;
	break;
	}
    }

if (needScreen)
    {
    verbose(1, "screenFastqForContaminants(%u(%s))\n", ef->id, ef->submitFileName);

    // Get the assay tag. 
    struct cgiParsedVars *tags = cdwMetaVarsList(conn, ef);
    char *assay = cdwLookupTag(tags, "assay");

    /* Get fastq record. */
    struct cdwFastqFile *fqf = cdwFastqFileFromFileId(conn, ef->id);
    if (fqf == NULL)
        errAbort("No cdwFastqFile record for file id %lld", (long long)ef->id);

    char sampleFastqName[PATH_LEN];
    
    /* Create downsampled fastq in temp directory - downsampled more than default even. */
    cdwMakeTempFastqSample(fqf->sampleFileName, FASTQ_SAMPLE_SIZE, sampleFastqName);
    verbose(1, "downsampled %s into %s\n", vf->licensePlate, sampleFastqName);
    
    for (target = targetList; target != NULL; target = target->next)
	{
	/* Get assembly associated with target */
	int assemblyId = target->assemblyId;
	char query[512];
	sqlSafef(query, sizeof(query), "select * from cdwAssembly where id=%d", assemblyId);
	struct cdwAssembly *newAsm = cdwAssemblyLoadByQuery(conn, query);
	if (newAsm == NULL)
	    errAbort("warehouse cdwQaContamTarget %d not found", assemblyId);

	/* If we don't already have a match, do work to create contam record. */
	int matchCount = cdwQaContamMade(conn, ef->id, target->id);
	if (matchCount <= 0)
	    {
	    /* We run the bed-file maker, just for side effect calcs. */
	    double mapRatio = 0, depth = 0, sampleCoverage = 0, uniqueMapRatio;
	    cdwAlignFastqMakeBed(ef, newAsm, sampleFastqName, vf, NULL,
		&mapRatio, &depth, &sampleCoverage, &uniqueMapRatio, assay);

	    verbose(1, "%s mapRatio %g, depth %g, sampleCoverage %g\n", 
		newAsm->name, mapRatio, depth, sampleCoverage);
	    struct cdwQaContam contam = 
		    {.fileId=ef->id, .qaContamTargetId=target->id, .mapRatio = mapRatio};
	    cdwQaContamSaveToDb(conn, &contam, "cdwQaContam", 256);
	    }
	cdwAssemblyFree(&newAsm);
	}
    cdwQaContamTargetFreeList(&targetList);
    if (keepTemp)
        verbose(1, "%s\n", sampleFastqName);
    else
	remove(sampleFastqName);
    cdwFastqFileFree(&fqf);
    }
}

void doContaminationQa(struct sqlConnection *conn, struct cdwFile *ef)
/* Try and do contamination level QA - mostly mapping fastq files to other
 * genomes. */
{
/* Get validated file info.  If not validated we don't bother. */
struct cdwValidFile *vf = cdwValidFileFromFileId(conn, ef->id);
if (vf == NULL)
    return;

/* We only work on fastq. */
if (!sameString(vf->format, "fastq"))
    return;

screenFastqForContaminants(conn, ef, vf);
}


void cdwMakeContaminationQa(int startId, int endId)
/* cdwMakeContaminationQa - Screen for contaminants by aligning against contaminant genomes.. */
{
/* Make list with all files in ID range */
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwFile *ef, *efList = cdwFileLoadIdRange(conn, startId, endId);

for (ef = efList; ef != NULL; ef = ef->next)
    {
    doContaminationQa(conn, ef);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
keepTemp = optionExists("keepTemp");
cdwMakeContaminationQa(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]));
return 0;
}

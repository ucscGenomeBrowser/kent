/* edwFinishAnalysis - Look for analysis jobs that have completed and integrate results into 
 * database. */
#include <uuid/uuid.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "cheapcgi.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "encode3/encode3Valid.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFinishAnalysis - Look for analysis jobs that have completed and integrate results into\n"
  "database.\n"
  "usage:\n"
  "   edwFinishAnalysis now\n"
  "Where 'now' is just a parameter that is ignored for now.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static char *sharedVal(struct cgiDictionary *dictList, char *var)
/* Return value if var is present and identical in all members of dictList */
{
char *val = NULL;
struct cgiDictionary *dict;
for (dict = dictList; dict != NULL; dict = dict->next)
    {
    struct cgiVar *cgi = hashFindVal(dict->hash, var);
    if (cgi == NULL)
        return NULL;
    char *newVal = cgi->val;
    if (val == NULL)
        val = newVal;
    else if (!sameString(val, newVal))
        return NULL;
    }
return val;
}

char *fakeTags(struct sqlConnection *conn, struct edwAnalysisRun *run, 
    struct edwFile *inputFileList, int fileIx, char *validKey, char *ucscDb)
/* Return tags for out output */
{
/* Fill in tags we actually know will be there */
struct dyString *dy = dyStringNew(0);
cgiEncodeIntoDy("format", run->outputFormats[fileIx], dy);
cgiEncodeIntoDy("ucsc_db", ucscDb, dy);
cgiEncodeIntoDy("valid_key", validKey, dy);
cgiEncodeIntoDy("output_type", run->outputTypes[fileIx], dy);

/* Next set up stuff to handle variable parts we only put in if all inputs agree. */

/* Make list of dictionaries corresponding to list of files. */
struct cgiDictionary *dict, *dictList = NULL;
struct edwFile *ef;
for (ef = inputFileList; ef != NULL; ef = ef->next)
    {
    dict = cgiDictionaryFromEncodedString(ef->tags);
    slAddTail(&dictList, dict);
    }

/* Loop through fields that might be shared putting them into tags if they are. */
static char *fakeIfShared[] = {"experiment", "replicate", "enriched_in", 
    "technical_replicate", "paired_end", };
int i;
for (i=0; i<ArraySize(fakeIfShared); ++i)
    {
    char *var = fakeIfShared[i];
    char *val = sharedVal(dictList, var);
    if (val != NULL)
        cgiEncodeIntoDy(var, val, dy);
    }

/* Clean up and go home */
cgiDictionaryFreeList(&dictList);
return dyStringCannibalize(&dy);
}

void markRunAsFailed(struct sqlConnection *conn, struct edwAnalysisRun *run)
/* Mark that the run failed,  prevent reanalysis */
{
char query[128];
sqlSafef(query, sizeof(query), 
    "update edwAnalysisRun set createStatus = -1 where id=%u", run->id);
sqlUpdate(conn, query);
}

boolean efWithSubmitNameExists(struct sqlConnection *conn, char *submitName)
/* Return TRUE if have file with this submit name in warehouse already */
{
char query[PATH_LEN*2];
sqlSafef(query, sizeof(query), 
    "select count(*) from edwFile where submitFileName='%s'", submitName);
return  (sqlQuickNum(conn, query) > 0);
}

struct edwUser *edwUserForPipeline(struct sqlConnection *conn)
/* Get user associated with automatic processes. */
{
return edwUserFromEmail(conn, "edw@encodedcc.sdsc.edu");
}

unsigned eapNewSubmit(struct sqlConnection *conn)
/* Create new submission record for eap */
{
struct edwUser *eapUser = edwUserForPipeline(conn);
char query[512];
sqlSafef(query, sizeof(query), 
    "insert edwSubmit (userId,url,startUploadTime) values (%u,'%s',%lld)"
    , eapUser->id, "EAP", edwNow());
sqlUpdate(conn, query);
unsigned submitId = sqlLastAutoId(conn);
edwUserFree(&eapUser);
return submitId;
}

struct edwSubmit *eapCurrentSubmit(struct sqlConnection *conn)
/* Get submit record for pipeline. */
{
char query[256];
struct edwUser *eapUser = edwUserForPipeline(conn);
sqlSafef(query, sizeof(query),
    "select max(id) from edwSubmit where userId=%u", eapUser->id);
unsigned submitId = sqlQuickNum(conn, query);
edwUserFree(&eapUser);
if (submitId == 0)
    {
    verbose(1, "No submit id for pipeline, creating new one\n");
    submitId = eapNewSubmit(conn);
    }
sqlSafef(query, sizeof(query), "select * from edwSubmit where id=%u", submitId);
return edwSubmitLoadByQuery(conn, query);
}


void updateSubmissionRecord(struct sqlConnection *conn, struct edwFile *ef)
/* Add some information about file to submission record. */
{
/* Get submit record and adjust fields. */
struct edwSubmit *submit = eapCurrentSubmit(conn);
submit->fileCount += 1;
submit->newFiles += 1;
submit->byteCount += ef->size;
submit->newBytes += ef->size;

/* Update database with new fields. */
char query[512];
sqlSafef(query, sizeof(query),
    "update edwSubmit set fileCount=%u, newFiles=%u, byteCount=%lld, newBytes=%lld where id=%u"
    , submit->fileCount, submit->newFiles, submit->byteCount, submit->newBytes, submit->id);
sqlUpdate(conn, query);
edwSubmitFree(&submit);
}

void finishGoodRun(struct sqlConnection *conn, struct edwAnalysisRun *run, 
    struct edwAnalysisJob *job)
/* Looks like the job for the run completed successfully, so let's grab results
 * and store them permanently */
{
/* Do a version check before committing to database. */
edwAnalysisCheckVersions(conn, run->analysisStep);

/* Look up UCSC db. */
char query[1024];
sqlSafef(query, sizeof(query), "select ucscDb from edwAssembly where id=%u", run->assemblyId);
char *ucscDb = sqlQuickString(conn, query);

/* Generate 16 bytes of random sequence with uuid generator */
unsigned char uu[16];
uuid_generate(uu);
uuid_unparse(uu, run->uuid);

/* Get list of input files */
struct edwFile *inputFile, *inputFileList = NULL;
int i;
for (i=0; i<run->inputFileCount; ++i)
    {
    inputFile = edwFileFromId(conn, run->inputFileIds[i]);
    slAddTail(&inputFileList, inputFile);
    }

struct edwSubmit *submit = eapCurrentSubmit(conn);

/* Generate initial edwRun records for each of the file outputs. */
struct edwFile *outputFile, *outputFileList = NULL;
struct dyString *outputFileIds = dyStringNew(0);
for (i=0; i<run->outputFileCount; ++i)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s/%s", run->tempDir, run->outputNamesInTempDir[i]);

    // Check for edwFile already made - possible race condition (should consider
    // only running this program serially to be honest)
    if (efWithSubmitNameExists(conn, path))
        {
	verbose(1, "Skipping %s, already in progress\n", path);
	continue;
	}

    verbose(1, "processing %s\n", path);
    if (!fileExists(path))
        {
	markRunAsFailed(conn, run);
	errAbort("Expected output %s not present", path);
	}

    /* Make most of edwFile record */
    AllocVar(outputFile);
    outputFile->submitId = submit->id;
    outputFile->submitFileName = cloneString(path);
    outputFile->size = fileSize(path);
    outputFile->updateTime = fileModTime(path);
    edwMd5File(path, outputFile->md5);
    char *validKey = encode3CalcValidationKey(outputFile->md5, outputFile->size);
    outputFile->tags = fakeTags(conn, run, inputFileList, i, validKey, ucscDb);
    outputFile->startUploadTime = edwNow();
    outputFile->edwFileName = outputFile->errorMessage = outputFile->deprecated = "";

    // Check for edwFile already made - possible race condition (should consider
    // only running this program serially to be honest)
    if (efWithSubmitNameExists(conn, path))
        {
	verbose(1, "Skipping %s, already in progress\n", path);
	continue;
	}

    /* Save to database so get file ID */
    edwFileSaveToDb(conn, outputFile, "edwFile", 0);
    outputFile->id = sqlLastAutoId(conn);

    /* Build up file name based on ID and rename Record time to rename as upload time. */
    char babyName[PATH_LEN], edwFileName[PATH_LEN];
    edwMakeFileNameAndPath(outputFile->id, outputFile->submitFileName, babyName, edwFileName);
    outputFile->edwFileName = cloneString(babyName);
    mustRename(path, edwFileName);
    outputFile->endUploadTime = edwNow();

    /* Update last few fields in database. */
    sqlSafef(query, sizeof(query),
	"update edwFile set endUploadTime=%lld, edwFileName='%s' where id=%u",
	outputFile->endUploadTime, outputFile->edwFileName, outputFile->id);
    sqlUpdate(conn, query);

    updateSubmissionRecord(conn, outputFile);

    /* Schedule validation and finishing */
    char command[256];
    safef(command, sizeof(command), "bash -exc 'edwQaAgent %u;edwAnalysisAddJson %u'",
	   outputFile->id, run->id);
    edwAddJob(conn, command);

    dyStringPrintf(outputFileIds, "%u,", outputFile->id);
    slAddTail(&outputFileList, outputFile);
    }

sqlSafef(query, sizeof(query), 
   "update edwAnalysisRun set uuid='%s', createStatus=1, createCount=%d, createFileIds='%s'"
   "where id=%u",
   run->uuid, slCount(outputFileList), outputFileIds->string, run->id);
sqlUpdate(conn, query);

dyStringFree(&outputFileIds);
}

void edwFinishAnalysis(char *how)
/* edwFinishAnalysis - Look for analysis jobs that have createStatus and integrate results into 
 * database. */
{
struct sqlConnection *conn = edwConnectReadWrite();
char query[512];
sqlSafef(query, sizeof(query), "select * from edwAnalysisRun where createStatus = 0");
struct edwAnalysisRun *run, *runList = edwAnalysisRunLoadByQuery(conn, query);
verbose(1, "Got %d unfinished analysis\n", slCount(runList));


for (run = runList; run != NULL; run = run->next)
    {
    sqlSafef(query, sizeof(query), "select * from edwAnalysisJob where id=%u", run->jobId);
    struct edwAnalysisJob *job = edwAnalysisJobLoadByQuery(conn, query);
    char *command = job->commandLine;
    if (job->endTime > 0)
        {
	if (job->returnCode == 0)
	    {
	    verbose(2, "succeeded %s\n", command);
	    finishGoodRun(conn, run, job);
	    }
	else
	    {
	    verbose(1, "failed %s\n", command);
	    markRunAsFailed(conn, run);
	    }
	}
    else
        {
	verbose(2, "running %s\n", command);
	}
    };
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwFinishAnalysis(argv[1]);
return 0;
}

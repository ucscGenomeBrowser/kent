/* eapFinish - Look for analysis jobs that have completed and integrate results into 
 * database. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include <uuid/uuid.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "../../encodeDataWarehouse/inc/edwQaWigSpotFromRa.h"
#include "encode3/encode3Valid.h"
#include "eapDb.h"
#include "eapLib.h"

boolean noClean = FALSE;
boolean keepFailing = FALSE;
boolean maxCount = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapFinish - Look for analysis jobs that have completed and integrate results into\n"
  "database.\n"
  "usage:\n"
  "   eapFinish now\n"
  "Where 'now' is just a parameter that is ignored for now.\n"
  "options:\n"
  "   -noClean if set then don't clean up temp dirs."
  "   -keepFailing - if set then don't always abort at first error\n"
  "   -maxCount=N - maximum number to finish\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"noClean", OPTION_BOOLEAN},
   {"keepFailing", OPTION_BOOLEAN},
   {"maxCount", OPTION_INT},
   {NULL, 0},
};

void markRunAsFailed(struct sqlConnection *conn, struct eapRun *run)
/* Mark that the run failed,  prevent reanalysis */
{
char query[128];
sqlSafef(query, sizeof(query), 
    "update eapRun set createStatus = -1 where id=%u", run->id);
sqlUpdate(conn, query);
}

void conditionalFail(struct sqlConnection *conn, struct eapRun *run)
/* Note particular run is failed, and abort rest of finishing unless they've set the magic
 * flag */
{
markRunAsFailed(conn, run);
if (!keepFailing)
    {
    errAbort("Aborting - use -keepFailing flag to burn through more than one such error");
    }
}

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

char *fakeTags(struct sqlConnection *conn, struct eapRun *run, 
    struct edwFile *inputFileList, int fileIx, char *validKey, char *ucscDb)
/* Return tags for our output */
{
struct eapStep *step = eapStepFromNameOrDie(conn, run->analysisStep);

/* Fill in tags we actually know will be there */
struct dyString *dy = dyStringNew(0);
cgiEncodeIntoDy("format", step->outputFormats[fileIx], dy);
cgiEncodeIntoDy("ucsc_db", ucscDb, dy);
cgiEncodeIntoDy("valid_key", validKey, dy);
cgiEncodeIntoDy("output_type", step->outputTypes[fileIx], dy);
cgiEncodeIntoDy("experiment", run->experiment, dy);

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
static char *fakeIfShared[] = {"replicate", "enriched_in", 
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

boolean efWithSubmitNameExists(struct sqlConnection *conn, char *submitName)
/* Return TRUE if have file with this submit name in warehouse already */
{
char query[PATH_LEN*2];
sqlSafef(query, sizeof(query), 
    "select count(*) from edwFile where submitFileName='%s'", submitName);
return  (sqlQuickNum(conn, query) > 0);
}

unsigned eapNewSubmit(struct sqlConnection *conn)
/* Create new submission record for eap */
{
/* In practice this just gets called if there is no existing submission for the EAP.
 * Most of the time the eap just hooks up to the most recent existing submission.
 * We make new EAP submissions sometimes to help lump together major versions. */
struct edwUser *eapUser = eapUserForPipeline(conn);
char query[512];
// The "EAP" url is clearly fake, but more informative than blank I guess.
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
struct edwUser *eapUser = eapUserForPipeline(conn);
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

void removeTempDir(char *dir)
/* Remove a temporary directory.  Out of paranoia make sure it is fully qualified. */
{
if (dir[0] != '/')
    errAbort("removeTempDir(%s) - dir must be absolute not relative", dir);
verbose(2, "cleaning up %s\n", dir);
char command[2*PATH_LEN];
safef(command, sizeof(command), "rm -r %s", dir);
mustSystem(command);
}

void mustMv(char *oldName, char *newName)
/* Make a system command to move oldName to newName.  Unlike mustRename
 * which relies on the rename() unix function, where both old and new have
 * to be on the same device, this one can cross devices. */
{
char command[3*PATH_LEN];
safef(command, sizeof(command), "mv %s %s", oldName, newName);
verbose(1, "%s\n", command);
mustSystem(command);
}

struct eapOutput *eapOutputNamedOnList(struct eapOutput *list, char *name)
/* Find named output on list */
{
struct eapOutput *out;
for (out = list; out != NULL; out = out->next)
    if (sameString(out->name, name))
        return out;
warn("Can't find %s on list of %d in eapOutputNamedOnList", name, slCount(list));
return NULL;
}

void deprecateAndReplaceRunOutputs(struct sqlConnection *conn, 
    struct eapRun *oldRun, struct eapRun *newRun)
/* Deprecated outputs of old run and replace them with new run outputs */
{
char query[512];
sqlSafef(query, sizeof(query), "select * from eapOutput where runId=%u", oldRun->id);
struct eapOutput *oldOutList = eapOutputLoadByQuery(conn, query);
sqlSafef(query, sizeof(query), "select * from eapOutput where runId=%u", newRun->id);
struct eapOutput *newOutList = eapOutputLoadByQuery(conn, query);
struct eapOutput *newOut, *oldOut;
for (newOut = newOutList; newOut != NULL; newOut = newOut->next)
    {
    oldOut = eapOutputNamedOnList(oldOutList, newOut->name);
    if (oldOut != NULL)
        {
	sqlSafef(query, sizeof(query),
	    "update edwFile set replacedBy=%u, deprecated='%s' where id=%u", 
	    newOut->fileId, "Replaced by newer version of same analysis step.", oldOut->fileId);
	verbose(2, "%s\n", query);
	sqlUpdate(conn, query);
	}
    }
}

struct eapInput *eapInputFileOnList(struct eapInput *list, unsigned fileId)
/* Find named output on list */
{
struct eapInput *in;
for (in = list; in != NULL; in = in->next)
    if (in->fileId == fileId)
        return in;
return NULL;
}


boolean eapInputListsMatch(struct eapInput *aList, struct eapInput *bList)
/* Return TRUE if aList and bList refer to same files*/
{
if (slCount(aList) != slCount(bList))
    return FALSE;
struct eapInput *a;
for (a = aList; a != NULL; a = a->next)
    {
    if (eapInputFileOnList(bList, a->fileId) == NULL)
        return FALSE;
    }
return TRUE;
}

void perhapsDeprecateEarlierVersions(struct sqlConnection *conn, struct eapRun *run)
/* Look for runs on same input and deprecate/replace files */
{
/* Get list of old runs we might need to deprecate */
char query[512];
sqlSafef(query, sizeof(query),
    "select * from eapRun where experiment='%s' and id<%u and analysisStep='%s' "
    "  and assemblyId=%u and createStatus>0 and stepVersionId < %u"
    ,  run->experiment, run->id, run->analysisStep, run->assemblyId, run->stepVersionId);
struct eapRun *oldRun, *oldRunList = eapRunLoadByQuery(conn, query);
verbose(2, "perhapsDeprecateEarlierVersions: run %u, %d old runs on %s\n", 
    run->id, slCount(oldRunList), run->experiment);
if (oldRunList == NULL)
    return;

/* Get list of inputs to existing run */
sqlSafef(query, sizeof(query), "select * from eapInput where runId=%u", run->id);
struct eapInput *inList = eapInputLoadByQuery(conn, query);
for (oldRun = oldRunList; oldRun != NULL; oldRun = oldRun->next)
    {
    sqlSafef(query, sizeof(query), "select * from eapInput where runId=%u", oldRun->id);
    struct eapInput *oldList = eapInputLoadByQuery(conn, query);
    if (eapInputListsMatch(inList, oldList))
        {
	deprecateAndReplaceRunOutputs(conn, oldRun, run);
	}
    eapInputFreeList(&oldList);
    }
eapInputFreeList(&inList);
eapRunFreeList(&oldRunList);
}

void absorbPhantomPeakStats(struct sqlConnection *conn, 
    struct eapStep *step, struct eapRun *run, struct eapJob *job, struct edwFile *inList)
/* Absorb any statistical output from phantompeaks/run_spp */ 
{
#ifdef JUST_FOR_DOC
The output we parse is a single tab-separated line with the following columns as defined in 
https://code.google.com/p/phantompeakqualtools/.  
You might need to check this against the autoSql definition for eapPhantomPeakStats, which
should be the same except col1 is replaced by the fileId of the associated file. 

COL1: Filename: tagAlign/BAM filename
COL2: numReads: effective sequencing depth i.e. total number of mapped reads in input file
COL3: estFragLen: comma separated strand cross-correlation peak(s) in decreasing order of correlation.
          The top 3 local maxima locations that are within 90% of the maximum cross-correlation value are output.
          In almost all cases, the top (first) value in the list represents the predominant fragment length.
          If you want to keep only the top value simply run
          sed -r 's/,[^\t]+//g' <outFile> > <newOutFile>
COL4: corr_estFragLen: comma separated strand cross-correlation value(s) in decreasing order (col2 follows the same order)
COL5: phantomPeak: Read length/phantom peak strand shift
COL6: corr_phantomPeak: Correlation value at phantom peak
COL7: argmin_corr: strand shift at which cross-correlation is lowest
COL8: min_corr: minimum value of cross-correlation
COL9: Normalized strand cross-correlation coefficient (NSC) = COL4 / COL8
COL10: Relative strand cross-correlation coefficient (RSC) = (COL4 - COL8) / (COL6 - COL8)
COL11: QualityTag: Quality tag based on thresholded RSC (codes: -2:veryLow,-1:Low,0:Medium,1:High,2:veryHigh)
#endif /* JUST_FOR_DOC */

/* Get path of stats file - this must match command line in schedulePhantomPeakSpp() in 
 * eapSchedule.c */
char path[PATH_LEN];
safef(path, sizeof(path), "%s%s", run->tempDir, "out.tab");

/* Read in first row of file */
struct lineFile *lf = lineFileOpen(path, TRUE);
char *row[EAPPHANTOMPEAKSTATS_NUM_COLS];
if (!lineFileRow(lf, row))
    {
    errAbort("Empty output %s on phantomPeakStats", path);
    }

/* Replace first column with file ID */
assert(slCount(inList) == 1);
struct edwFile *ef = inList;
char numBuf[32];
safef(numBuf, sizeof(numBuf), "%u", ef->id);
row[0] = numBuf;

/* Load into our data structure and save to database */
struct eapPhantomPeakStats stats;
eapPhantomPeakStatsStaticLoad(row, &stats);
eapPhantomPeakStatsSaveToDb(conn, &stats, "eapPhantomPeakStats", 512);
}

void absorbDnaseStats(struct sqlConnection *conn, 
    struct eapStep *step, struct eapRun *run, struct eapJob *job, struct edwFile *inList)
/* Absorb any statistical output from phantompeaks/run_spp */ 
{
/* Make sure we have expected single input. */
assert(slCount(inList) == 1);
struct edwFile *ef = inList;

/* This routine builds up the stats5 structure below and saves it to database. */
struct edwQaDnaseSingleStats5m stats5 = {.next=NULL, .fileId=ef->id};

/* Get path of spot.ra file  and load it. */
char spotPath[PATH_LEN];
safef(spotPath, sizeof(spotPath), "%s%s", run->tempDir, "spot.ra");
struct edwQaWigSpot *spot = edwQaWigSpotOneFromRa(spotPath);

/* Transfer spot data into stats5 */
stats5.spotRatio = spot->spotRatio;
stats5.enrichment = spot->enrichment;
stats5.basesInGenome = spot->basesInGenome;
stats5.basesInSpots = spot->basesInSpots;
stats5.sumSignal = spot->sumSignal;
stats5.spotSumSignal = spot->spotSumSignal;

/* Get path of phantomPeaks.tab file  */
char ppsPath[PATH_LEN];
safef(ppsPath, sizeof(ppsPath), "%s%s", run->tempDir, "phantomPeaks.tab");

/* Read in first row of file */
struct lineFile *lf = lineFileOpen(ppsPath, TRUE);
char *row[EAPPHANTOMPEAKSTATS_NUM_COLS];
if (!lineFileRow(lf, row))
    errAbort("Empty output %s in absorbDnaseStats", ppsPath);

/* Replace first column with 0 since it needs to be an int for fileId, not fileName. */
row[0] = "0";

/* Load phantom peaks data structure */
struct eapPhantomPeakStats pps;
eapPhantomPeakStatsStaticLoad(row, &pps);


/* Transfer phantomPeak info to stats5. */
stats5.sampleReads = pps.numReads;
stats5.estFragLength = pps.estFragLength;
stats5.corrEstFragLen= pps.corrEstFragLen;
stats5.phantomPeak = pps.phantomPeak;
stats5.corrPhantomPeak = pps.corrPhantomPeak;
stats5.argMinCorr = pps.argMinCorr;
stats5.minCorr = pps.minCorr;
stats5.nsc = pps.nsc;
stats5.rsc = pps.rsc;
stats5.rscQualityTag = pps.qualityTag;
 
/* Save to database */
edwQaDnaseSingleStats5mSaveToDb(conn, &stats5, "edwQaDnaseSingleStats5m", 512);
}

void absorbStats(struct sqlConnection *conn, 
    struct eapStep *step, struct eapRun *run, struct eapJob *job, struct edwFile *inList)
/* Absorb any statistical output files left by run into database */
{
/* This routine is just a dispatch based on step type.  Most steps do not produce
 * statistical output */

/* Wrap in error collection */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *stepName = step->name;
    if (sameString(stepName, "phantom_peak_stats"))
	absorbPhantomPeakStats(conn, step, run, job, inList);
    else if (sameString(stepName, "dnase_stats"))
	absorbDnaseStats(conn, step, run, job, inList);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    conditionalFail(conn, run);
    }
errCatchFree(&errCatch);
}

void finishGoodRun(struct sqlConnection *conn, struct eapRun *run, 
    struct eapJob *job)
/* Looks like the job for the run completed successfully, so let's grab results
 * and store them permanently */
{
/* Do a version check before committing to database. */
eapCheckVersions(conn, run->analysisStep);

/* Look up UCSC db. */
char query[1024];
sqlSafef(query, sizeof(query), "select ucscDb from edwAssembly where id=%u", run->assemblyId);
char *ucscDb = sqlQuickString(conn, query);

/* What step are we running under what submit? */
struct eapStep *step = eapStepFromNameOrDie(conn, run->analysisStep);
struct edwSubmit *submit = eapCurrentSubmit(conn);

/* Get list of input files */
struct edwFile *inputFileList = NULL;
sqlSafef(query, sizeof(query), "select * from eapInput where runId=%u order by id", 
    run->id);
struct eapInput *input, *inputList = eapInputLoadByQuery(conn, query);
for (input = inputList; input != NULL; input = input->next)
    {
    struct edwFile *inputFile = edwFileFromId(conn, input->fileId);
    slAddTail(&inputFileList, inputFile);
    }

/* Generate initial records for each of the file outputs. */
int i;
for (i=0; i<step->outCount; ++i)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s%s", run->tempDir, step->outputNamesInTempDir[i]);

    verbose(1, "processing %s\n", path);
    if (!fileExists(path))
        {
	warn("Expected output %s not present", path);
	conditionalFail(conn, run);
	break;
	}

    /* Make most of edwFile record */
    struct edwFile *outputFile;
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

    /* Save to database so get file ID */
    edwFileSaveToDb(conn, outputFile, "edwFile", 0);
    outputFile->id = sqlLastAutoId(conn);

    /* Build up file name based on ID and rename Record time to rename as upload time. */
    char babyName[PATH_LEN], edwFileName[PATH_LEN];
    edwMakeFileNameAndPath(outputFile->id, outputFile->submitFileName, babyName, edwFileName);
    outputFile->edwFileName = cloneString(babyName);
    mustMv(path, edwFileName);
    outputFile->endUploadTime = edwNow();

    /* Update last few fields in database. */
    sqlSafef(query, sizeof(query),
	"update edwFile set endUploadTime=%lld, edwFileName='%s' where id=%u",
	outputFile->endUploadTime, outputFile->edwFileName, outputFile->id);
    sqlUpdate(conn, query);

    updateSubmissionRecord(conn, outputFile);

    /* Make eapOutput record */
    sqlSafef(query, sizeof(query),
	    "insert eapOutput (runId,name,ix,fileId) values (%u,'%s',%u,%u)"
	    , run->id, step->outputTypes[i], 0, outputFile->id);
    sqlUpdate(conn, query);

    /* Schedule validation and finishing */
    edwAddQaJob(conn, outputFile->id);

    }

/* Absorb stats if any into database */
absorbStats(conn, step, run, job, inputFileList);

sqlSafef(query, sizeof(query), 
	"update eapRun set createStatus=1 where id=%u", run->id);
sqlUpdate(conn, query);

if (!noClean)
    {
    removeTempDir(run->tempDir);
    }
perhapsDeprecateEarlierVersions(conn, run);
}

void eapFinish(char *how)
/* eapFinish - Look for analysis jobs that have createStatus and integrate results into 
 * database. */
{
struct sqlConnection *conn = edwConnectReadWrite();
char query[512];
sqlSafef(query, sizeof(query), "select * from eapRun where createStatus = 0");
struct eapRun *run, *runList = eapRunLoadByQuery(conn, query);
verbose(1, "Got %d unfinished analysis\n", slCount(runList));
int count = 0;


for (run = runList; run != NULL; run = run->next)
    {
    sqlSafef(query, sizeof(query), "select * from eapJob where id=%u", run->jobId);
    struct eapJob *job = eapJobLoadByQuery(conn, query);
    if (job == NULL)
	{
        warn("No job for run %u, jobId %u doesn't exist", run->id, run->jobId);
	markRunAsFailed(conn, run);
	}
    else
	{
	char *command = job->commandLine;
	if (job->endTime > 0)
	    {
	    ++count;
	    if (maxCount > 0 && count > maxCount)
		{
		verbose(1, "Did all %d\n", maxCount);
		break;
		}
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
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
noClean = optionExists("noClean");
keepFailing = optionExists("keepFailing");
maxCount = optionInt("maxCount", maxCount);
eapFinish(argv[1]);
return 0;
}

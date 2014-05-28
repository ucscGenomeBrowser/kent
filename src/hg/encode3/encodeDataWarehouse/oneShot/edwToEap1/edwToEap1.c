/* edwToEap1 - Help transforme edw format analysis tables to eap formatted ones.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "../../eap/inc/eapDb.h"
#include "edwAnalysis.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwToEap1 - Help transforme edw format analysis tables to eap formatted ones.\n"
  "usage:\n"
  "   edwToEap1 outDir\n"
  "options:\n"
  "   -load - Load output into encodeDataWarehouse tables too.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"load", OPTION_BOOLEAN},
   {NULL, 0},
};

void transformJobTable(struct sqlConnection *conn, struct edwAnalysisJob *jobList, char *outName)
/* Transform edwAnalysisJob to edwAnalysis */
{
FILE *f = mustOpen(outName, "w");
struct edwAnalysisJob *job;
for (job = jobList; job != NULL; job = job->next)
     {
     struct eapJob j = {0};
     j.id = job->id;
     j.commandLine = sqlEscapeTabFileString(job->commandLine);
     assert(job->commandLine);
     j.startTime = job->startTime;
     j.endTime = job->endTime;
     j.stderr = sqlEscapeTabFileString(emptyForNull(job->stderr));
     j.returnCode = job->returnCode;
     j.cpusRequested = max(1, job->cpusRequested);
     char buf[16];
     if (isEmpty(job->parasolId))
	 {
	 safef(buf, sizeof(buf), "%d", job->pid);
	 j.parasolId = buf;
	 }
     else
         j.parasolId = job->parasolId;
     eapJobTabOut(&j, f);
     freez(&j.commandLine);
     freez(&j.stderr);
     }
carefulClose(&f);
}

void transformSoftwareTable(struct sqlConnection *conn, struct edwAnalysisSoftware *swList, 
    char *softwareFile, char *swVersionFile)
/* Transform edwSoftware to eapSoftware and version tables. */
{
int id = 0;
struct edwAnalysisSoftware *sw;
FILE *f = mustOpen(softwareFile, "w");
for (sw = swList; sw != NULL; sw = sw->next)
    {
    struct eapSoftware out = {0};
    out.id = ++id;
    out.name = sw->name;
    out.url = "";
    out.email = "";
    eapSoftwareTabOut(&out, f);
    sw->id = id;   // Now using eap instead of edw IDs. 
    }
carefulClose(&f);

id = 0;
f = mustOpen(swVersionFile, "w");
for (sw = swList; sw != NULL; sw = sw->next)
    {
    struct eapSwVersion out = {0};
    out.id = ++id;
    out.software = sw->name;
    out.version = sw->version;
    out.notes = "";
    memcpy(out.md5, sw->md5, sizeof(out.md5));
    eapSwVersionTabOut(&out, f);
    }
carefulClose(&f);
}

char **formatsFromFiles(struct sqlConnection *conn, int fileCount, unsigned *fileIds)
/* Given an array of file IDs, return a comma separated list of the formats of thos
 * files. */
{
char **formats;
AllocArray(formats, fileCount);
int i;
for (i=0; i<fileCount; ++i)
    {
    char query[256];
    char format[256];
    sqlSafef(query, sizeof(query), "select format from edwValidFile where fileId=%u", fileIds[i]);
    if (sqlQuickQuery(conn, query, format, sizeof(format)) == NULL)
        errAbort("in formatsFromFiles fileId %u is not found in edwValidFile table", fileIds[i]);
    formats[i] = cloneString(format);
    }
return formats;
}

void transformStepTable(struct sqlConnection *conn, 
    struct edwAnalysisStep *stepList,  struct hash *swHash,
    char *stepFile, char *stepVersionFile, char *stepSoftwareFile)
/* Transform edwStep to eapStep and related tables */
{
/* Make three files while going through stepList in one pass,  in case want to
 * share overhead of load of subobjects. . */
FILE *fStep = mustOpen(stepFile, "w");
FILE *fStepSoftware = mustOpen(stepSoftwareFile, "w");
FILE *fStepVersion = mustOpen(stepVersionFile, "w");

/* IDs for the two tables we're forking off. */
int stepSoftwareId = 0;
int stepVersionId = 0;

/* The main loop. */
struct edwAnalysisStep *step;
for (step = stepList; step != NULL; step = step->next)
    {
    /* Start to build step */
    struct eapStep s = {0};
    s.id = step->id;
    s.name = step->name;
    s.cpusRequested = step->cpusRequested;

    /* Attempt to load subobject */
    char query[512];
    sqlSafef(query, sizeof(query), "select * from edwAnalysisRun where analysisStep='%s' order by outputFileCount desc limit 1",
	step->name);
    struct edwAnalysisRun *run = edwAnalysisRunLoadByQuery(conn, query);

    /* If we have subobject fill in info from it */
    if (run != NULL)
        {
	s.inCount = run->inputFileCount;
	s.inputTypes = run->inputTypes;
	s.inputFormats = formatsFromFiles(conn, run->inputFileCount, run->inputFileIds);
	s.outCount = run->outputFileCount;
	s.outputNamesInTempDir = run->outputNamesInTempDir;
	s.outputFormats = run->outputFormats;
	s.outputTypes = run->outputTypes;
	}
    /* Output eapStep object */
    eapStepTabOut(&s, fStep);

    /* Output software/step connector objects */
    struct eapStepSoftware ess = {0};
    int i;
    for (i=0; i<step->softwareCount; ++i)
        {
	ess.id = ++stepSoftwareId;
	ess.step = s.name;
	struct edwAnalysisSoftware *sw = hashMustFindVal(swHash, step->software[i]);
	ess.software = sw->name;
	eapStepSoftwareTabOut(&ess, fStepSoftware);
	}

    /* Output 'version 1' version objects */
    struct eapStepVersion v = {0};
    v.id = ++stepVersionId;
    v.step = s.name;
    v.version = 1;
    eapStepVersionTabOut(&v, fStepVersion);
    }
carefulClose(&fStep);
carefulClose(&fStepSoftware);
carefulClose(&fStepVersion);
}

void versionVsVersion(struct sqlConnection *conn, 
    struct edwAnalysisStep *stepList, struct hash *stepHash, 
    struct edwAnalysisSoftware *swList, struct hash *swHash, 
    char *outName)
/* Make the relationally useful but crypticly named eapStepVersionSwVersion table. 
 * This table keeps tracks of which versions of analysis software are used in a
 * particular version of a step in the pipeline. */
{
FILE *f = mustOpen(outName, "w");
struct edwAnalysisStep *step;
int id = 0;
for (step = stepList; step != NULL; step = step->next)
    {
    int i;
    for (i=0; i < step->softwareCount; ++i)
        {
	char *swName = step->software[i];
	struct edwAnalysisSoftware *sw = hashFindVal(swHash, swName);
	if (sw == NULL)
	    errAbort("Can't find %s in swHash from edwAnalysisSoftware on edwAnalysisStep %u",
		    swName, step->id);
	struct eapStepSwVersion vv = {0};
	vv.id = ++id;
	vv.stepVersionId = step->id;	// True when there's only one version as is case now
	vv.swVersionId = sw->id;    // True when there's only one version as is case now
	eapStepSwVersionTabOut(&vv, f);
	}
    }
carefulClose(&f);
}

struct hash *hashSwList(struct edwAnalysisSoftware *swList)
/* Return hashes of edwAnalysisSoftware keyed by ->name */
{
struct hash *hash = hashNew(0);
struct edwAnalysisSoftware *sw;
for (sw = swList; sw != NULL; sw = sw->next)
    hashAdd(hash, sw->name, sw);
return hash;
}

struct hash *hashStepList(struct edwAnalysisStep *stepList)
/* Return hashes of edwSteps keyed by step->name */
{
struct hash *hash = hashNew(0);
struct edwAnalysisStep *step;
for (step = stepList; step != NULL; step = step->next)
    hashAdd(hash, step->name, step);
return hash;
}

void transformRun(struct sqlConnection *conn, struct edwAnalysisRun *runList, 
    struct hash *stepHash,
    char *analysisFile, char *inputFile, char *outputFile)
/* Relationalize a bit by splitting a run into 3 table,  a run,  it's inputs, and
 * it's outputs. */
{
/* Overall we'll just do each output one at a time.  No need to get fancy. */

FILE *f = mustOpen(analysisFile, "w");
struct edwAnalysisRun *run;
for (run = runList; run != NULL; run = run->next)
    {
    if (startsWith("macs2_chip_", run->analysisStep) && run->inputFileCount != 2)
        continue;   // Bad inputFileCount on this step.
    struct edwAnalysisStep *step = hashFindVal(stepHash, run->analysisStep);
    if (step == NULL)
        errAbort("Can't find %s in stepHash in run %u", run->analysisStep, run->id);
    struct eapRun out = {0};
    out.id = run->id;
    out.jobId = run->jobId;
    memcpy(out.experiment, run->experiment, sizeof(out.experiment));
    out.analysisStep = run->analysisStep;
    out.stepVersionId = step->id;   
    out.tempDir = run->tempDir;
    out.assemblyId = run->assemblyId;
    out.jsonResult = sqlEscapeTabFileString(run->jsonResult);
    out.createStatus = run->createStatus;
    eapRunTabOut(&out, f);
    freez(&out.jsonResult);
    }
carefulClose(&f);

/* Now write inputs */
int inId = 0;
f = mustOpen(inputFile, "w");
for (run = runList; run != NULL; run = run->next)
    {
    if (startsWith("macs2_chip_", run->analysisStep) && run->inputFileCount != 2)
        continue;   // Bad inputFileCount on this step.
    if (run->createStatus > 0)
	{
	int i;
	for (i=0; i<run->inputFileCount; ++i)
	    {
	    struct eapInput in;
	    in.id = ++inId;
	    in.runId = run->id;
	    in.name = run->inputTypes[i];
	    in.ix = 0;
	    in.fileId = run->inputFileIds[i];
	    in.val = "";
	    eapInputTabOut(&in, f);
	    }
	}
    }
carefulClose(&f);

/* Finally outputs */
int outId = 0;
f = mustOpen(outputFile, "w");
for (run = runList; run != NULL; run = run->next)
    {
    if (startsWith("macs2_chip_", run->analysisStep) && run->inputFileCount != 2)
        continue;   // Bad inputFileCount on this step.
    if (run->createStatus > 0 && run->createCount == run->outputFileCount)
	{
	int i;
	for (i=0; i<run->outputFileCount; ++i)
	    {
	    struct eapOutput out;
	    out.id = ++outId;
	    out.runId = run->id;
	    out.name = run->outputTypes[i];
	    out.ix = 0;
	    out.fileId = run->createFileIds[i];
	    out.val = "";
	    eapOutputTabOut(&out, f);
	    }
	}
    }
}

void loadEapDb(char *dir)
/* Load up EAP portion of database from tab file directory. */
{
struct sqlConnection *conn = edwConnectReadWrite();
char *tables[] = {"eapJob", "eapSoftware", "eapSwVersion", "eapStep", "eapStepSoftware", "eapStepVersion",
"eapStepSwVersion", "eapRun", "eapInput", "eapOutput",};
int i;
for (i=0; i<ArraySize(tables); ++i)
    {
    char *table = tables[i];

    /* Delete what used to be in table */
    char query[2*PATH_MAX];
    sqlSafef(query, sizeof(query), "delete from %s", table);
    sqlUpdate(conn,query);

    /* Make up tab separated file name and ask database to load files into the table. */
    char tabName[PATH_LEN];
    safef(tabName, PATH_LEN, "%s/%s.tab", dir, table);
    sqlSafef(query, sizeof(query), "load data local infile '%s' into table %s", tabName, table);
    verbose(2, "%s\n", query);
    sqlUpdate(conn, query);
    }
}

void edwToEap1(char *dir)
/* edwToEap1 - Help transforme edw format analysis tables to eap formatted ones.. */
{
makeDirsOnPath(dir);
struct sqlConnection *conn = edwConnect();

struct edwAnalysisJob *jobList = edwAnalysisJobLoadByQuery(conn, "select * from edwAnalysisJob order by id");
char jobFile[PATH_LEN];
safef(jobFile, PATH_LEN, "%s/%s", dir, "eapJob.tab");
transformJobTable(conn, jobList, jobFile);

struct edwAnalysisSoftware *swList = edwAnalysisSoftwareLoadByQuery(conn, "select * from edwAnalysisSoftware order by id");
struct hash *swHash = hashSwList(swList);
char softwareFile[PATH_LEN], swVersionFile[PATH_LEN];
safef(softwareFile, PATH_LEN, "%s/%s", dir, "eapSoftware.tab");
safef(swVersionFile, PATH_LEN, "%s/%s", dir, "eapSwVersion.tab");
transformSoftwareTable(conn, swList, softwareFile, swVersionFile);

struct edwAnalysisStep *stepList = edwAnalysisStepLoadByQuery(conn, "select * from edwAnalysisStep order by id");
struct hash *stepHash = hashStepList(stepList);
verbose(1, "stepHash has %d els\n", stepHash->elCount);
char stepFile[PATH_LEN], stepVersionFile[PATH_LEN], stepSoftwareFile[PATH_LEN];
safef(stepFile, PATH_LEN, "%s/%s", dir, "eapStep.tab");
safef(stepVersionFile,  PATH_LEN, "%s/%s", dir, "eapStepVersion.tab");
safef(stepSoftwareFile, PATH_LEN, "%s/%s", dir, "eapStepSoftware.tab");
transformStepTable(conn, stepList, swHash, stepFile, stepVersionFile, stepSoftwareFile);

char stepVersionSwVersionFile[PATH_LEN];
safef(stepVersionSwVersionFile, PATH_LEN, "%s/%s", dir, "eapStepSwVersion.tab");
versionVsVersion(conn, stepList, stepHash, swList, swHash, stepVersionSwVersionFile);

struct edwAnalysisRun *runList = edwAnalysisRunLoadByQuery(conn, "select * from edwAnalysisRun order by id");
char analysisFile[PATH_LEN], inputFile[PATH_LEN], outputFile[PATH_LEN];
safef(analysisFile, PATH_LEN, "%s/%s", dir, "eapRun.tab");
safef(inputFile, PATH_LEN, "%s/%s", dir, "eapInput.tab");
safef(outputFile, PATH_LEN, "%s/%s", dir, "eapOutput.tab");
transformRun(conn, runList, stepHash, analysisFile, inputFile, outputFile);

if (optionExists("load"))
     {
     loadEapDb(dir);
     }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwToEap1(argv[1]);
return 0;
}

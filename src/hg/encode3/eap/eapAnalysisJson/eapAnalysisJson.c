/* edwAnalysisAddJson - Add json string to edwAnalysisRun record.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "jsonParse.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwAnalysisAddJson - Add json string to edwAnalysisRun record.\n"
  "usage:\n"
  "   edwAnalysisAddJson analysisRunId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void dyJsonTag(struct dyString *dy, char *var)
/* Print out quoted tag followed by colon */
{
dyStringPrintf(dy, "\"%s\": ", var);
}

void dyJsonEndLine(struct dyString *dy, boolean isMiddle)
/* Write comma if in middle, and then newline regardless. */
{
if (isMiddle)
   dyStringAppendC(dy, ',');
dyStringAppendC(dy, '\n');
}

void dyJsonString(struct dyString *dy, char *var, char *string, boolean isMiddle)
/* Print out "var": "val" */
{
dyJsonTag(dy, var);
dyStringPrintf(dy, "\"%s\"", string);
dyJsonEndLine(dy, isMiddle);
}

void dyJsonNumber(struct dyString *dy, char *var, long long val, boolean isMiddle)
/* print out "var": val as number */
{
dyJsonTag(dy, var);
dyStringPrintf(dy, "%lld", val);
dyJsonEndLine(dy, isMiddle);
}

void dyJsonListStart(struct dyString *dy, char *var)
/* Start an array in JSON */
{
dyJsonTag(dy, var);
dyStringAppend(dy, "[\n");
}

void dyJsonListEnd(struct dyString *dy, boolean isMiddle)
/* End an array in JSON */
{
dyStringAppendC(dy, ']');
dyJsonEndLine(dy, isMiddle);
}

void dyJsonObjectStart(struct dyString *dy)
/* Print start of object */
{
dyStringAppend(dy, "{\n");
}

void dyJsonObjectEnd(struct dyString *dy, boolean isMiddle)
/* End object in JSON */
{
dyStringAppendC(dy, '}');
dyJsonEndLine(dy, isMiddle);
}

char *jsonForRun(struct sqlConnection *conn, struct edwAnalysisRun *run, 
    struct edwFile *inputFileList, struct edwFile *outputFileList, struct edwAnalysisJob *job)
/* Generate json for run given input and output file lists. */
{
struct dyString *dy = dyStringNew(0);

dyStringAppend(dy, "{\n");
dyJsonString(dy, "uuid", run->uuid, TRUE);
dyJsonNumber(dy, "start_time", job->startTime, TRUE);
dyJsonNumber(dy, "end_time", job->endTime, TRUE);
dyJsonString(dy, "analysis", run->analysisStep, TRUE);
dyJsonString(dy, "configuration", run->configuration, TRUE);

/* Print sw_version list */
struct edwAnalysisStep *step = edwAnalysisStepFromName(conn, run->analysisStep);
if (step == NULL)
    errAbort("edwAnalysisStep named %s not found", run->analysisStep);
dyJsonListStart(dy, "sw_version");
int i;
for (i=0; i<step->softwareCount; ++i)
    {
    char query[256];
    sqlSafef(query, sizeof(query), 
	"select * from edwAnalysisSoftware where name = '%s'", step->software[i]);
    struct edwAnalysisSoftware *software = edwAnalysisSoftwareLoadByQuery(conn, query);
    if (software == NULL)
         errAbort("%s not found in edwAnalysisSoftware table", step->software[i]);
    
    dyJsonObjectStart(dy);
    dyJsonString(dy, "software", software->name, TRUE);
    dyJsonString(dy, "version", software->version, TRUE);
    dyJsonString(dy, "md5", software->md5, FALSE);
    dyJsonObjectEnd(dy, i != step->softwareCount-1);
    edwAnalysisSoftwareFree(&software);
    }
dyJsonListEnd(dy, TRUE);

/* Print input list */
dyJsonListStart(dy, "inputs");
struct edwFile *ef;
for (i=0, ef = inputFileList; ef != NULL; ef = ef->next, ++i)
    {
    dyJsonObjectStart(dy);
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    assert(vf != NULL);
    dyJsonString(dy, "type", run->inputTypes[i], TRUE);
    dyJsonString(dy, "value", vf->licensePlate, FALSE);
    edwValidFileFree(&vf);
    dyJsonObjectEnd(dy, ef->next != NULL);
    }
dyJsonListEnd(dy, TRUE);

/* Print output list */
dyJsonListStart(dy, "outputs");
for (i=0, ef = outputFileList; ef != NULL; ef = ef->next, ++i)
    {
    dyJsonObjectStart(dy);
    dyJsonString(dy, "type", run->outputTypes[i], TRUE);
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    assert(vf != NULL);
    dyJsonString(dy, "value", vf->licensePlate, FALSE);
    dyJsonObjectEnd(dy, ef->next != NULL);
    }
dyJsonListEnd(dy, TRUE);

/* Print last field and finish up */
dyJsonString(dy, "command_line", job->commandLine, FALSE);
dyStringAppend(dy, "}\n");
return dyStringCannibalize(&dy);
}

void edwAnalysisAddJson(unsigned analysisRunId)
/* edwAnalysisAddJson - Add json string to edwAnalysisRun record.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
char query[16*1024]; // Json might get big
safef(query, sizeof(query), "select * from edwAnalysisRun where id=%u", analysisRunId);
struct edwAnalysisRun *run = edwAnalysisRunLoadByQuery(conn, query);

if (run->outputFileCount != run->createCount)
    errAbort("edwAnalysisRun record #%u,  createCount less than outputFileCount", run->id);

/* Get list of input files */
struct edwFile *inputFile, *inputFileList = NULL;
int i;
for (i=0; i<run->inputFileCount; ++i)
    {
    inputFile = edwFileFromId(conn, run->inputFileIds[i]);
    slAddTail(&inputFileList, inputFile);
    }

/* Get list of output files */
struct edwFile *outFile, *outFileList = NULL;
int validCount = 0;
for (i=0; i<run->createCount; ++i)
    {
    outFile = edwFileFromId(conn, run->createFileIds[i]);
    struct edwValidFile *vf = edwValidFileFromFileId(conn, outFile->id);
    if (vf != NULL)
	++validCount;
    slAddTail(&outFileList, outFile);
    }

if (validCount == run->createCount)
    {
    sqlSafef(query, sizeof(query), "select * from edwAnalysisJob where id=%u", run->jobId);
    struct edwAnalysisJob *job = edwAnalysisJobLoadByQuery(conn, query);
    run->jsonResult = jsonForRun(conn, run, inputFileList, outFileList, job);
    verbose(1, "creating json of %d characters\n", (int)strlen(run->jsonResult));
    jsonParse(run->jsonResult);  // Just for validation
    uglyf("after parse creating json of %d characters\n", (int)strlen(run->jsonResult));

    sqlSafef(query, sizeof(query), 
       "update edwAnalysisRun set jsonResult='%s' where id=%u", run->jsonResult, run->id);
    sqlUpdate(conn, query);
    }
else
    {
    verbose(1, "%d of %d files validated\n", validCount, run->createCount);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwAnalysisAddJson(sqlUnsigned(argv[1]));
return 0;
}

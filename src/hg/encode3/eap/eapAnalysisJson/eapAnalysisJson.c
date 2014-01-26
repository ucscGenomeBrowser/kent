/* eapAnalysisJson - Add json string to eapAnalysis record.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapDb.h"
#include "eapLib.h"
#include "jsonParse.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapAnalysisJson - Add json string to eapAnalysis record.\n"
  "usage:\n"
  "   eapAnalysisJson analysisRunId\n"
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

char *jsonForRun(struct sqlConnection *conn, struct eapAnalysis *run,  struct eapStep *step,
    struct edwFile *inputFileList, struct edwFile *outputFileList, struct eapJob *job)
/* Generate json for run given input and output file lists. */
{
struct dyString *dy = dyStringNew(0);

dyStringAppend(dy, "{\n");
dyJsonNumber(dy, "start_time", job->startTime, TRUE);
dyJsonNumber(dy, "end_time", job->endTime, TRUE);
dyJsonString(dy, "analysis", run->analysisStep, TRUE);

/* Print sw_version list */
dyJsonListStart(dy, "sw_version");

char query[512];
sqlSafef(query, sizeof(query), 
    "select software,version,md5 from eapStepSwVersion,eapSwVersion "
    " where eapStepSwVersion.swVersionId = eapSwVersion.id"
    " and eapStepSwVersion.stepVersionId='%u'"
    , run->stepVersionId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
boolean firstTime = TRUE;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0], *version = row[1], *md5 = row[2];
    if (firstTime)
        firstTime = FALSE;
    else
        dyStringAppendC(dy, ',');
    dyStringAppendC(dy, '{');
    dyJsonString(dy, "software", name, TRUE);
    dyJsonString(dy, "version", version, TRUE);
    dyJsonString(dy, "md5", md5, FALSE);
    dyStringAppendC(dy, '}');
    }
dyJsonListEnd(dy, TRUE);

/* Print input list */
dyJsonListStart(dy, "inputs");
struct edwFile *ef;
int i;
for (i=0, ef = inputFileList; ef != NULL; ef = ef->next, ++i)
    {
    dyJsonObjectStart(dy);
    dyJsonString(dy, "type", step->inputTypes[i], TRUE);
    dyStringAppend(dy, "\"value\": [");
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    assert(vf != NULL);
    dyStringPrintf(dy, "\"%s\"", vf->licensePlate);
    edwValidFileFree(&vf);
    dyStringAppend(dy, "]\n");
    dyJsonObjectEnd(dy, ef->next != NULL);
    }
dyJsonListEnd(dy, TRUE);

/* Print output list */
dyJsonListStart(dy, "outputs");
for (i=0, ef = outputFileList; ef != NULL; ef = ef->next, ++i)
    {
    dyJsonObjectStart(dy);
    dyJsonString(dy, "type", step->outputTypes[i], TRUE);
    dyStringAppend(dy, "\"value\": [");
    struct edwValidFile *vf = edwValidFileFromFileId(conn, ef->id);
    assert(vf != NULL);
    dyStringPrintf(dy, "\"%s\"", vf->licensePlate);
    edwValidFileFree(&vf);
    dyStringAppend(dy, "]\n");
    dyJsonObjectEnd(dy, ef->next != NULL);
    }
dyJsonListEnd(dy, TRUE);

/* Print last field and finish up */
dyJsonString(dy, "command_line", job->commandLine, FALSE);
dyStringAppend(dy, "}\n");
return dyStringCannibalize(&dy);
}

void eapAnalysisJson(unsigned analysisRunId)
/* eapAnalysisJson - Add json string to eapAnalysis record.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
char query[16*1024]; // Json might get big
safef(query, sizeof(query), "select * from eapAnalysis where id=%u", analysisRunId);
struct eapAnalysis *run = eapAnalysisLoadByQuery(conn, query);
if (run == NULL)
     return;

struct eapStep *step = eapStepFromName(conn, run->analysisStep);
if (step == NULL)
    errAbort("eapStep named %s not found", run->analysisStep);

/* Get list of input files */
safef(query, sizeof(query), 
    "select edwFile.* from edwFile,eapInput where edwFile.id=eapInput.fileId "
    " and eapInput.analysisId = %u "
    " order by eapInput.id"
    , analysisRunId);
struct edwFile *inputFileList = edwFileLoadByQuery(conn, query);

/* Get list of output files */
safef(query, sizeof(query), 
    "select edwFile.* from edwFile,eapOutput where edwFile.id=eapOutput.fileId "
    " and eapOutput.analysisId = %u "
    " order by eapOutput.id"
    , analysisRunId);
struct edwFile *outFile, *outFileList = edwFileLoadByQuery(conn, query);

/* Count how many outputs are validated */
int validCount = 0;
for (outFile = outFileList; outFile != NULL; outFile = outFile->next)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, outFile->id);
    if (vf != NULL)
	++validCount;
    }

/* If all are validated then attempt to attach json... */
if (validCount == step->outCount)
    {
    sqlSafef(query, sizeof(query), "select * from eapJob where id=%u", run->jobId);
    struct eapJob *job = eapJobLoadByQuery(conn, query);
    run->jsonResult = jsonForRun(conn, run, step, inputFileList, outFileList, job);
    verbose(1, "creating json of %d characters\n", (int)strlen(run->jsonResult));
    jsonParse(run->jsonResult);  // Just for validation

    sqlSafef(query, sizeof(query), 
       "update eapAnalysis set jsonResult='%s' where id=%u", run->jsonResult, run->id);
    sqlUpdate(conn, query);
    }
else
    {
    verbose(1, "%d of %d files validated\n", validCount, step->outCount);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
eapAnalysisJson(sqlUnsigned(argv[1]));
return 0;
}

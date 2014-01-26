/* eapRunAddJson - Add json string to eapRun record.. */
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
  "eapRunAddJson - Add json string to eapRun record.\n"
  "usage:\n"
  "   eapRunAddJson analysisRunId\n"
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

void printIoListJson(struct sqlConnection *conn,
    struct dyString *dy, struct eapInput *list, char *listName)
/* Print out input (or output) list to JSON in dy*/
{
dyJsonListStart(dy, listName);
struct eapInput *el;
char *lastName = "";
for (el = list; el != NULL; el = el->next)
    {
    if (!sameString(lastName, el->name))
        {
	dyJsonObjectStart(dy);
	dyJsonString(dy, "type", el->name, TRUE);
	dyStringAppend(dy, "\"value\": [");
	}
    struct edwValidFile *vf = edwValidFileFromFileId(conn, el->fileId);
    assert(vf != NULL);
    dyStringPrintf(dy, "\"%s\"", vf->licensePlate);
    edwValidFileFree(&vf);
    struct eapInput *next = el->next;
    if (next != NULL && sameString(next->name, el->name))
        dyStringAppendC(dy, ',');
    else
	{
	dyStringAppend(dy, "]\n");
	dyJsonObjectEnd(dy, el->next != NULL);
	}
    lastName = el->name;
    }
dyJsonListEnd(dy, TRUE);
}

char *jsonForRun(struct sqlConnection *conn, struct eapRun *run,  struct eapStep *step,
    struct eapInput *inList, struct eapOutput *outList, struct eapJob *job)
/* Generate json for run given input and output lists. */
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
printIoListJson(conn, dy, inList, "inputs");

/* Do some checking eapInput and eapOutput really same before casting. */
assert(EAPINPUT_NUM_COLS == EAPOUTPUT_NUM_COLS);
printIoListJson(conn, dy, (struct eapInput *)outList, "outputs");

/* Print last field and finish up */
dyJsonString(dy, "command_line", job->commandLine, FALSE);
dyStringAppend(dy, "}\n");
return dyStringCannibalize(&dy);
}

void eapRunAddJson(unsigned analysisRunId)
/* eapRunAddJson - Add json string to eapRun record.. */
{
struct sqlConnection *conn = edwConnectReadWrite();
char query[16*1024]; // Json might get big
safef(query, sizeof(query), "select * from eapRun where id=%u", analysisRunId);
struct eapRun *run = eapRunLoadByQuery(conn, query);
if (run == NULL)
     return;

struct eapStep *step = eapStepFromName(conn, run->analysisStep);
if (step == NULL)
    errAbort("eapStep named %s not found", run->analysisStep);

/* Get list of input files */
safef(query, sizeof(query), 
    "select * from eapInput where eapInput.runId = %u order by eapInput.id" , analysisRunId);
struct eapInput *inputList = eapInputLoadByQuery(conn, query);

/* Get list of output files */
safef(query, sizeof(query), 
    "select * from eapOutput where eapOutput.runId = %u order by eapOutput.id" , analysisRunId);
struct eapOutput *out, *outList = eapOutputLoadByQuery(conn, query);

/* Count how many outputs are validated */
int validCount = 0;
for (out = outList; out != NULL; out = out->next)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, out->fileId);
    if (vf != NULL)
	++validCount;
    }

/* If all are validated then attempt to attach json... */
if (validCount == step->outCount)
    {
    sqlSafef(query, sizeof(query), "select * from eapJob where id=%u", run->jobId);
    struct eapJob *job = eapJobLoadByQuery(conn, query);
    run->jsonResult = jsonForRun(conn, run, step, inputList, outList, job);
    verbose(1, "creating json of %d characters\n", (int)strlen(run->jsonResult));
    jsonParse(run->jsonResult);  // Just for validation

    sqlSafef(query, sizeof(query), 
       "update eapRun set jsonResult='%s' where id=%u", run->jsonResult, run->id);
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
eapRunAddJson(sqlUnsigned(argv[1]));
return 0;
}

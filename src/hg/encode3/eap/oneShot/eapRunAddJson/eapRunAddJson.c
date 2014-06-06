/* eapRunAddJson - Add json string to eapRun record.. */
/* THIS MODULE IS OBSOLETE AND IS JUST HERE TO BE CANNIBALIZED FOR PARTS */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "jsonWrite.h"

boolean clAll = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapRunAddJson - Create json string based largely on eapRun table contents.\n"
  "Notes works on runIds, not on fileIds.\n"
  "usage:\n"
  "   eapRunAddJson startRunId endRunId output.json\n"
  "options:\n"
  "   -all - Include software, step, and version info.\n"
  );
}

int schemaVersion = 1;	// Current version of JSON schema

/* Command line validation table. */
static struct optionSpec options[] = {
   {"all", OPTION_BOOLEAN},
   {NULL, 0},
};

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
	dyJsonString(dy, "name", el->name, TRUE);
	dyStringAppend(dy, "\"files\": [");
	}
    struct edwValidFile *vf = edwValidFileFromFileId(conn, el->fileId);
    assert(vf != NULL);
    dyStringPrintf(dy, "\"%s%s\"", "/files/", vf->licensePlate);
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
dyJsonLinkNum(dy, "@id", "/analysis_run/", run->id, TRUE);
dyJsonListStart(dy, "@type");
dyStringPrintf(dy, "\"analysis_run\",\n\"item\"\n");
dyJsonListEnd(dy, TRUE);
dyJsonNumber(dy, "schema_version", schemaVersion, TRUE);

dyJsonNumber(dy, "run_id", run->id, TRUE);
dyJsonDateFromUnix(dy, "start_time", job->startTime, TRUE);
dyJsonDateFromUnix(dy, "end_time", job->endTime, TRUE);
dyJsonLink(dy, "step", "/analysis_step/", run->analysisStep, TRUE);

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
        dyStringAppend(dy, ",\n");
    dyStringPrintf(dy, "\"/software/%s/%s/%s\"", name, version, md5);
    }
dyStringAppend(dy, "\n");
dyJsonListEnd(dy, TRUE);

/* Print mapping target  */
struct edwAssembly *assembly = edwAssemblyForId(conn, run->assemblyId);
dyJsonLink(dy, "mapping_target", "/mapping_target/", assembly->name, TRUE);
edwAssemblyFree(&assembly);

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

void printRunJson(struct sqlConnection *conn, unsigned runId, char *rootName, FILE *f)
/* printRunJson - figure out JSON for run object and print it to f. */
{
char query[16*1024]; // Json might get big
safef(query, sizeof(query), "select * from eapRun where id=%u", runId);
struct eapRun *run = eapRunLoadByQuery(conn, query);
if (run == NULL)
     return;

struct eapStep *step = eapStepFromName(conn, run->analysisStep);
if (step == NULL)
    errAbort("eapStep named %s not found", run->analysisStep);

/* Get list of input files */
safef(query, sizeof(query), 
    "select * from eapInput where eapInput.runId = %u order by eapInput.id" , runId);
struct eapInput *inList = eapInputLoadByQuery(conn, query);

/* Get list of output files */
safef(query, sizeof(query), 
    "select * from eapOutput where eapOutput.runId = %u order by eapOutput.id" , runId);
struct eapOutput *out, *outList = eapOutputLoadByQuery(conn, query);

/* Count how many outputs are validated */
int validCount = 0;
for (out = outList; out != NULL; out = out->next)
    {
    struct edwValidFile *vf = edwValidFileFromFileId(conn, out->fileId);
    if (vf != NULL)
	{
	++validCount;
	edwValidFileFree(&vf);
	}
    }

/* If all are validated then attempt to attach json... */
if (validCount == step->outCount)
    {
    sqlSafef(query, sizeof(query), "select * from eapJob where id=%u", run->jobId);
    struct eapJob *job = eapJobLoadByQuery(conn, query);
    if (job != NULL)  
	{
	char *jsonText= jsonForRun(conn, run, step, inList, outList, job);
	verbose(1, "creating json of %d characters on runId %u\n", 
		(int)strlen(jsonText), run->id);
	fprintf(f, "%s\n", jsonText);
	jsonParse(jsonText);  // Just for validation

#ifdef OLD
	sqlSafef(query, sizeof(query), 
	   "update eapRun set jsonResult='%s' where id=%u", run->jsonResult, run->id);
	sqlUpdate(conn, query);
#endif /* OLD */
	eapJobFree(&job);
	}
    }
else
    {
    verbose(1, "%d of %d files validated\n", validCount, step->outCount);
    }
eapInputFreeList(&inList);
eapOutputFreeList(&outList);
eapStepFree(&step);
eapRunFree(&run);
}

void printSoftwareJson(struct eapSoftware *software, FILE *f)
/* Print out eapSoftware as json object. */
{
struct dyString *dy = dyStringNew(0);
dyStringAppend(dy, "{\n");
dyJsonLink(dy, "@id", "/software/", software->name, TRUE);
dyJsonListStart(dy, "@type");
dyStringPrintf(dy, "\"software\",\n\"item\"\n");
dyJsonListEnd(dy, TRUE);
dyJsonNumber(dy, "schema_version", schemaVersion, TRUE);
dyJsonString(dy, "name", software->name, TRUE);
dyJsonString(dy, "title", software->name, TRUE);
dyJsonString(dy, "url", software->url, TRUE);
dyJsonString(dy, "email", software->email, FALSE);
dyStringAppend(dy, "}");
fprintf(f, "%s", dy->string);
dyStringFree(&dy);
}

void printSwVersionJson(struct eapSwVersion *sv, FILE *f)
/* Print out eapSwVersion as json object. */
{
struct dyString *dy = dyStringNew(0);
dyStringAppend(dy, "{\n");
dyJsonLinkNum(dy, "@id", "/software_version/", sv->id, TRUE);
dyJsonListStart(dy, "@type");
dyStringPrintf(dy, "\"software_version\",\n\"item\"\n");
dyJsonListEnd(dy, TRUE);
dyJsonNumber(dy, "schema_version", schemaVersion, TRUE);
dyJsonNumber(dy, "id", sv->id, TRUE);
dyJsonLink(dy, "software", "/software/", sv->software, TRUE);
dyJsonString(dy, "version", sv->version, TRUE);
dyJsonString(dy, "dcc_md5", sv->md5, FALSE);
dyStringAppend(dy, "}");
fprintf(f, "%s", dy->string);
dyStringFree(&dy);
}

void printStepJson(struct sqlConnection *conn, struct eapStep *step, FILE *f)
/* Print out eapSwVersion as json object. */
{
/* Print opening brace for object, @id, @type and schema_version fields */
struct dyString *dy = dyStringNew(0);
dyStringAppend(dy, "{\n");
dyJsonLink(dy, "@id", "/analysis_step/", step->name, TRUE);
dyJsonListStart(dy, "@type");
dyStringPrintf(dy, "\"analysis_step\",\n\"item\"\n");
dyJsonListEnd(dy, TRUE);
dyJsonNumber(dy, "schema_version", schemaVersion, TRUE);

/* Print name,  and title which is just name with spaces for underbars */
dyJsonString(dy, "name", step->name, TRUE);
subChar(step->name, '_', ' ');
dyJsonString(dy, "title", step->name, TRUE);
subChar(step->name, ' ', '_');

/* Get and print step version */
char query[512];
sqlSafef(query, sizeof(query), 
    "select max(id) from eapStepVersion where step='%s'", step->name);
int versionId = sqlQuickNum(conn, query);
dyJsonNumber(dy, "version", versionId, TRUE);


/* Get and print software used */
sqlSafef(query, sizeof(query), 
    "select eapSwVersion.* from eapStepSwVersion,eapSwVersion "
    "   where eapSwVersion.id = eapStepSwVersion.swVersionId and stepVersionId=%d", versionId);
struct eapSwVersion *svList = eapSwVersionLoadByQuery(conn, query);
dyJsonListStart(dy, "software");
struct eapSwVersion *sv;
for (sv = svList; sv != NULL; sv = sv->next)
    {
    dyStringPrintf(dy, "\"%s%s\"", "/software/", sv->software);
    if (sv->next != NULL)
        dyStringAppendC(dy, ',');
    dyStringAppendC(dy, '\n');
    }
dyJsonListEnd(dy, TRUE);
eapSwVersionFreeList(&svList);


/* Print inputs */
dyJsonListStart(dy, "inputs");
int i;
for (i=0; i<step->inCount; ++i)
    {
    dyJsonObjectStart(dy);
    dyJsonString(dy, "name", step->inputTypes[i], TRUE);
    dyJsonString(dy, "format", step->inputFormats[i], FALSE);
    dyJsonObjectEnd(dy, (i != step->inCount-1));
    }
dyJsonListEnd(dy, TRUE);

/* Print outputs */
dyJsonListStart(dy, "outputs");
for (i=0; i<step->outCount; ++i)
    {
    dyJsonObjectStart(dy);
    dyJsonString(dy, "type", step->outputTypes[i], TRUE);
    dyJsonString(dy, "format", step->outputFormats[i], FALSE);
    dyJsonObjectEnd(dy, (i != step->outCount-1));
    }
dyJsonListEnd(dy, FALSE);


dyStringAppend(dy, "}");
fprintf(f, "%s", dy->string);
dyStringFree(&dy);
}

void printSoftwareRelatedJson(struct sqlConnection *conn, FILE *f)
/* Print information about software, versions, and steps in json to f */
{
char query[512];
sqlSafef(query, sizeof(query), "select * from eapSoftware");
struct eapSoftware *sw, *swList = eapSoftwareLoadByQuery(conn, query);
for (sw = swList; sw != NULL; sw = sw->next)
    {
    printSoftwareJson(sw, f);
    fprintf(f, ",\n\n");
    }

sqlSafef(query, sizeof(query), "select * from eapSwVersion");
struct eapSwVersion *sv, *svList = eapSwVersionLoadByQuery(conn, query);
for (sv = svList; sv != NULL; sv = sv->next)
    {
    printSwVersionJson(sv, f);
    fprintf(f, ",\n\n");
    }

sqlSafef(query, sizeof(query), "select * from eapStep");
struct eapStep *step, *stepList = eapStepLoadByQuery(conn, query);
for (step = stepList; step != NULL; step = step->next)
    {
    printStepJson(conn, step, f);
    fprintf(f, ",\n\n");
    }
eapStepFreeList(&stepList);
eapSwVersionFreeList(&svList);
eapSoftwareFreeList(&swList);
}

void eapRunAddJson(unsigned startId, unsigned endId, char *outJson)
/* Add json to eapRun records between startId and endId, including endId */
{
int id;
struct sqlConnection *conn = edwConnectReadWrite();
FILE *f = mustOpen(outJson, "w");
if (clAll)
    {
    fprintf(f, "[\n");
    printSoftwareRelatedJson(conn, f);
    }
for (id=startId; id<=endId; ++id)
    printRunJson(conn, id, "", f);
if (clAll)
    fprintf(f, "]\n");
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
clAll = optionExists("all");
eapRunAddJson(sqlUnsigned(argv[1]), sqlUnsigned(argv[2]), argv[3]);
return 0;
}

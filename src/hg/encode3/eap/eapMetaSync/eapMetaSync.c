/* eapMetaSync - Send JSON over to metadatabase at Stanford describing software, versions, steps, analysis runs, and the file relationships produced by these runs.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "net.h"
#include "htmlPage.h"
#include "jsonParse.h"
#include "jsonWrite.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapDb.h"
#include "eapLib.h"

/* Globals set via command line */
char *gHost, *gUserId, *gPassword;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapMetaSync - Send JSON over to metadatabase at Stanford describing software, versions, steps, analysis runs, and the file relationships produced by these runs.\n"
  "usage:\n"
  "   eapMetaSync host user password\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void syncOneRecord(struct sqlConnection *conn, char *type, struct jsonWrite *json, 
    char *table, long long id)
/* Send over one record and save UUID result to row of table defined by id in idField. */
{
/* Construct dyString for URL */
struct dyString *dyUrl = dyStringNew(0);
dyStringPrintf(dyUrl, "http://%s:%s@%s/%s/", gUserId, gPassword, gHost, type);
verbose(2, "%s\n", dyUrl->string);

/* Construct dyString for http header */
struct dyString *dyHeader = dyStringNew(0);
dyStringPrintf(dyHeader, "Content-length: %d\r\n", json->dy->stringSize);
dyStringPrintf(dyHeader, "Content-type: text/javascript\r\n");

/* Send header and then JSON */
int sd = netOpenHttpExt(dyUrl->string, "POST", dyHeader->string);
mustWriteFd(sd, json->dy->string, json->dy->stringSize);

/* Grab response */
struct dyString *dyText = netSlurpFile(sd);
close(sd);
uglyf("%s\n", dyText->string);

/* Turn it into htmlPage structure - this will also parse out http header */
struct htmlPage *response = htmlPageParse(dyUrl->string, dyText->string);
uglyf("status %s %d\n", response->status->version, response->status->status);

/* If we got bad status abort with hopefully very informative message. */
int status = response->status->status;
if (status != 200 && status != 201)  // HTTP codes
    {
    errAbort("ERROR - Metadatabase returns %d to our post request\n"
	     "POSTED JSON: %s\n"
             "URL: %s\n"
	     "FULL RESPONSE: %s\n",  status, json->dy->string, dyUrl->string, dyText->string);
    }

/* Parse uuid out of json response.  It should look something like
	{
	"status": "success", 
	"@graph": [
	     {
	     "description": "The macs2 peak calling software from Tao Liu.", 
	     "name": "macs2", "title": "macs2", "url": "https://github.com/taoliu/MACS/", 
	     "uuid": "9bda84fd-9872-49e3-9aa0-b71adbf9f31d", 
	     "schema_version": "1", 
	     "source_url": "https://github.com/taoliu/MACS/", 
	     "references": [], 
	     "@id": "/software/9bda84fd-9872-49e3-9aa0-b71adbf9f31d/", 
	     "@type": ["software", "item"], 
	     "aliases": []
	     }
	],     
	"@type": ["result"]
	}
*/
struct jsonElement *jsonRoot = jsonParse(response->htmlText);
struct jsonElement *graph = jsonMustFindNamedField(jsonRoot, "", "@graph");
struct slRef *ref = jsonListVal(graph, "@graph");
assert(slCount(ref) == 1);
struct jsonElement *graphEl = ref->val;
char *uuid = jsonStringField(graphEl, "uuid");
uglyf("Got uuid %s\n", uuid);

/* Save uuid to table */
char query[256];
sqlSafef(query, sizeof(query),
    "update %s set metaUuid='%s' where id=%lld", table, uuid, id);
sqlUpdate(conn, query);

/* Clean up */
dyStringFree(&dyUrl);
dyStringFree(&dyHeader);
dyStringFree(&dyText);
response->fullText = NULL;  // avoid double free of this
htmlPageFree(&response);
}

struct jsonWrite *jsonForSoftware(struct eapSoftware *sw)
/* Return JSON text for software.  This is something that looks
 * like:
	{
	"name" : "macs2",
	"title" : "macs2",
	"url" : "https://github.com/taoliu/MACS/"
	}
 */
{
struct jsonWrite *jw = jsonWriteNew(0);
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "name", sw->name);
jsonWriteString(jw, "title", sw->name);
jsonWriteString(jw, "url", sw->url);
jsonWriteObjectEnd(jw);
return jw;
}

void syncSoftware(struct sqlConnection *conn)
/* Sync the eapSoftware table */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from eapSoftware where metaUuid=''");
struct eapSoftware *sw, *swList = eapSoftwareLoadByQuery(conn, query);
for (sw = swList; sw != NULL; sw = sw->next)
    {
    if (isupper(sw->name[0]))
        {
	warn("Skipping %s until they handle upper case right", sw->name);
	continue;
	}
    struct jsonWrite *json = jsonForSoftware(sw);
    syncOneRecord(conn, "software", json, "eapSoftware", sw->id);
    jsonWriteFree(&json);
    }
}

struct jsonWrite *jsonForSwVersion(struct sqlConnection *conn, struct eapSwVersion *ver)
/* Construct JSON string describing ver. */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from eapSoftware where name='%s'", ver->software);
struct eapSoftware *sw = eapSoftwareLoadByQuery(conn, query);
if (sw == NULL || isEmpty(sw->metaUuid))
    {
    verbose(2, "Skipping %s %s because %s has not been registered with metadatabase\n", 
	ver->software, ver->version, ver->software);
    return NULL;
    }
uglyf("sw id %u, softare %s, metaUuid %s\n",  sw->id, sw->name, sw->metaUuid);
struct jsonWrite *jw = jsonWriteNew(0);
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "software", sw->metaUuid);
jsonWriteString(jw, "version", ver->version);
jsonWriteString(jw, "dcc_md5", ver->md5);
jsonWriteObjectEnd(jw);
return jw;
}

void syncSoftwareVersion(struct sqlConnection *conn)
/* Sync the eapSoftware table */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from eapSwVersion where metaUuid=''");
struct eapSwVersion *ver, *verList = eapSwVersionLoadByQuery(conn, query);
for (ver = verList; ver != NULL; ver = ver->next)
    {
    struct jsonWrite *json = jsonForSwVersion(conn, ver);
    if (json != NULL)
	{
	syncOneRecord(conn, "software_version", json, "eapSwVersion", ver->id);
	jsonWriteFree(&json);
	}
    else
        {
	verbose(2, "Skipping eapSwVersion id=%u\n", ver->id);
	}
    }
}

struct eapStepVersion *eapStepVersionLatest(struct sqlConnection *conn, char *stepName)
/* Return latest version of named step.  Aborts if no such step. */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from eapStepVersion where step='%s' order by version desc limit 1", stepName);
struct eapStepVersion *ver = eapStepVersionLoadByQuery(conn, query);
if (ver == NULL)
    errAbort("Couldn't find eapStepVersion for %s", stepName);
return ver;
}

struct eapSoftware *eapSoftwareLoadByName(struct sqlConnection *conn, char *name)
/* Return named software */
{
char query[256];
sqlSafef(query, sizeof(query), 
    "select * from eapSoftware where name='%s'", name);
return eapSoftwareLoadByQuery(conn, query);
}

struct jsonWrite *jsonForStep(struct sqlConnection *conn, struct eapStep *step)
/* Convert an eapStep to json. See step.json in same directory as the .c file. 
 * for an example. */
{
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);

/* Write name and description. */
jsonWriteString(jw, "name", step->name);
jsonWriteString(jw, "description", step->description);

/* Write version */
struct eapStepVersion *ver = eapStepVersionLatest(conn, step->name);
jsonWriteNumber(jw, "version", ver->version);

/* Write software */
jsonWriteListStart(jw, "software");
char query[512];
sqlSafef(query, sizeof(query), "select * from eapStepSoftware where step='%s'", step->name);
struct eapStepSoftware *ss, *ssList = eapStepSoftwareLoadByQuery(conn, query);
boolean isFirst = TRUE;
struct dyString *dy = jw->dy;
for (ss = ssList; ss != NULL; ss = ss->next)
    {
    struct eapSoftware *software = eapSoftwareLoadByName(conn, ss->software);
    assert(software != NULL);
    if (software->metaUuid)
        {
	if (!isFirst)
	    {
	    dyStringAppendC(dy, ',');
	    dyStringAppendC(dy, '\n');
	    }
	isFirst = FALSE;
	dyStringAppendC(dy, '"');
	dyStringPrintf(dy, "/software/%s/", software->metaUuid);
	dyStringAppendC(dy, '"');
	}
    }
if (!isFirst)
    dyStringAppendC(dy, '\n');
jsonWriteListEnd(jw);
/* Done with writing software list */


/* Write input list */
jsonWriteListStart(jw, "inputs");
int i;
for (i=0; i<step->inCount; ++i)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "format", step->inputFormats[i]);
    jsonWriteString(jw, "name", step->inputTypes[i]);
    jsonWriteString(jw, "description", step->inputDescriptions[i]);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);

/* Write output list */
jsonWriteListStart(jw, "outputs");
for (i=0; i<step->outCount; ++i)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "format", step->outputFormats[i]);
    jsonWriteString(jw, "name", step->outputTypes[i]);
    jsonWriteString(jw, "description", step->outputDescriptions[i]);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);

jsonWriteObjectEnd(jw);
return jw;
}

void syncStep(struct sqlConnection *conn)
/* Sync the eapStep table */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from eapStep where metaUuid=''");
struct eapStep *step, *stepList = eapStepLoadByQuery(conn, query);
uglyf("Got %d steps\n", slCount(stepList));
for (step = stepList; step != NULL; step = step->next)
    {
    struct jsonWrite *json = jsonForStep(conn, step);
    if (json != NULL)
	{
	syncOneRecord(conn, "analysis_step", json, "eapStep", step->id);
	jsonWriteFree(&json);
	}
    else
        {
	verbose(2, "Skipping eapStep id=%u\n", step->id);
	}
    }
}


void eapMetaSync(char *host, char *userId, char *password)
/* eapMetaSync - Send JSON over to metadatabase at Stanford describing software, versions, 
 * steps, analysis runs, and the file relationships produced by these runs.. */
{
gHost = host;
gUserId = userId;
gPassword = password;
struct sqlConnection *conn = eapConnectReadWrite();

syncSoftware(conn);
syncSoftwareVersion(conn);
syncStep(conn);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
eapMetaSync(argv[1], argv[2], argv[3]);
return 0;
}

/* eapMetaSync - Send JSON over to metadatabase at Stanford describing software, versions, steps, analysis runs, and the file relationships produced by these runs.. */
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

void syncOneRecord(struct sqlConnection *conn, char *type, struct dyString *json, 
    char *table, long long id)
/* Send over one record and save UUID result to row of table defined by id in idField. */
{
/* Construct dyString for URL */
struct dyString *dyUrl = dyStringNew(0);
dyStringPrintf(dyUrl, "http://%s:%s@%s/%s/", gUserId, gPassword, gHost, type);
uglyf("%s\n", dyUrl->string);

/* Construct dyString for http header */
struct dyString *dyHeader = dyStringNew(0);
dyStringPrintf(dyHeader, "Content-length: %d\r\n", json->stringSize);
dyStringPrintf(dyHeader, "Content-type: text/javascript\r\n");

/* Send header and then JSON */
int sd = netOpenHttpExt(dyUrl->string, "POST", dyHeader->string);
mustWriteFd(sd, json->string, json->stringSize);

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
             "Url: %s\n"
	     "Full response: %s\n",  status, dyUrl->string, dyText->string);
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

struct dyString *jsonForSoftware(struct eapSoftware *sw)
/* Return JSON text for software.  This is something that looks
 * like:
	{
	"name" : "macs2",
	"title" : "macs2",
	"url" : "https://github.com/taoliu/MACS/"
	}
 */
{
struct dyString *dy = dyStringNew(0);
dyJsonObjectStart(dy);
dyJsonString(dy, "name", sw->name, TRUE);
dyJsonString(dy, "title", sw->name, TRUE);
dyJsonString(dy, "url", sw->url, FALSE);
dyJsonObjectEnd(dy, FALSE);
return dy;
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
    struct dyString *json = jsonForSoftware(sw);
    syncOneRecord(conn, "software", json, "eapSoftware", sw->id);
    dyStringFree(&json);
    }
}

struct dyString *jsonForSwVersion(struct sqlConnection *conn, struct eapSwVersion *ver)
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
struct dyString *dy = dyStringNew(0);
dyJsonObjectStart(dy);
dyJsonString(dy, "software", sw->metaUuid, TRUE);
dyJsonString(dy, "version", ver->version, TRUE);
dyJsonString(dy, "dcc_md5", ver->md5, FALSE);
dyJsonObjectEnd(dy, FALSE);
return dy;
}

void syncSoftwareVersion(struct sqlConnection *conn)
/* Sync the eapSoftware table */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from eapSwVersion where metaUuid=''");
struct eapSwVersion *ver, *verList = eapSwVersionLoadByQuery(conn, query);
for (ver = verList; ver != NULL; ver = ver->next)
    {
    struct dyString *json = jsonForSwVersion(conn, ver);
    if (json != NULL)
	{
	syncOneRecord(conn, "software_version", json, "eapSwVersion", ver->id);
	dyStringFree(&json);
	}
    else
        {
	verbose(2, "Skipping eapSwVersion id=%u\n", ver->id);
	}
    }
}

struct dyString *jsonForStep(struct sqlConnection *conn, struct eapStep *step)
/* Convert an eapStep to json. See step.json in same directory as the .c file. 
 * for an example. */
{
struct dyString *dy = dyStringNew(0);
dyJsonObjectStart(dy);
dyJsonString(dy, "name", step->name, TRUE);
dyJsonString(dy, "title", step->name, TRUE);
dyJsonString(dy, "url", step->url, FALSE);
dyJsonObjectEnd(dy, FALSE);
return dy;
}

void syncStep(struct sqlConnection *conn)
/* Sync the eapStep table */
{
char query[256];
sqlSafef(query, sizeof(query), "select * from eapStep where metaUuid=''");
struct eapStep *step, *stepList = eapStepLoadByQuery(conn, query);
for (step = stepList; step != NULL; step = step->next)
    {
    struct dyString *json = jsonForStep(conn, step);
    if (json != NULL)
	{
	syncOneRecord(conn, "analysis_step", json, "eapStep", step->id);
	dyStringFree(&json);
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

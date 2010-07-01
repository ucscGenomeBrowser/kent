/* hgApi - provide a JSON based API to the browser. */

#include "common.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "dystring.h"

static char const rcsid[] = "$Id: hgApi.c,v 1.3 2010/05/30 21:11:47 larrym Exp $";

static void fail(char *msg)
{
puts("Status: 400\n\n");
puts(msg);
exit(-1);
}

void makeIndent(char *buf, int bufLen, int indent)
{
indent = min(indent, bufLen - 2);
memset(buf, '\t', indent);
buf[indent] = 0;
}

static void trackJson(struct dyString *json, struct trackDb *tdb, int *count, int indent)
{
char tabs[100];
makeIndent(tabs, sizeof(tabs), indent);
if((*count)++)
    dyStringPrintf(json, ",\n");
dyStringPrintf(json, "%s{\n", tabs);
makeIndent(tabs, sizeof(tabs), indent + 1);
dyStringPrintf(json, "%s\"track\": \"%s\",\n%s\"shortLabel\": \"%s\",\n%s\"longLabel\": \"%s\",\n%s\"group\": \"%s\"",
               tabs, tdb->track,
               tabs, tdb->shortLabel, tabs, tdb->longLabel,
               tabs, tdb->grp);
if(tdbIsComposite(tdb) && tdb->subtracks != NULL)
    {
    struct trackDb *ptr;
    dyStringPrintf(json, ",\n%s\"subtracks\":\n%s[\n", tabs, tabs);
    int count = 0;
    for (ptr = tdb->subtracks; ptr != NULL; ptr = ptr->next)
        {
        trackJson(json, ptr, &count, indent + 2);
        }
    dyStringPrintf(json, "\n%s]", tabs);
    }
makeIndent(tabs, sizeof(tabs), indent);
dyStringPrintf(json, "\n%s}", tabs);
}

int main(int argc, char *argv[])
{
struct dyString *output = newDyString(10000);
char *database = cgiOptionalString("db");
char *cmd = cgiOptionalString("cmd");
char *jsonp = cgiOptionalString("jsonp");
if(database)
    {
    database = sqlEscapeString(database);
    if(!hDbExists(database))
        fail("Invalid database");
    }
else
    fail("Missing 'db' parameter");

if(!cmd)
    fail("Missing 'cmd' parameter");

if(!strcmp(cmd, "trackList"))
    {
    // Return trackList for this assembly
    // e.g. http://genome.ucsc.edu/hgApi?db=hg18&cmd=trackList

    struct trackDb *tdb, *tdbList = NULL;
    tdbList = hTrackDb(database, NULL);
    dyStringPrintf(output, "[\n");
    int count = 0;
    for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
        {
        trackJson(output, tdb, &count, 1);
        count++;
        }
    dyStringAppend(output, "\n]\n");
    }
else if(!strcmp(cmd, "metaDb"))
    {
    // Return list of values for given metaDb var
    // e.g. http://genome.ucsc.edu/hgApi?db=hg18&cmd=metaDb&var=cell

    struct sqlConnection *conn = hAllocConn(database);
    boolean metaDbExists = sqlTableExists(conn, "metaDb");
    if(metaDbExists)
        {
        char query[256];
        struct sqlResult *sr = NULL;
        char **row;
        int i;
        struct slName *el, *termList = NULL;
        char *var = cgiOptionalString("var");
        if(var)
            var = sqlEscapeString(var);
        else
            fail("Missing var parameter");
        safef(query, sizeof(query), "select distinct val from metaDb where var = '%s'", var);
        sr = sqlGetResult(conn, query);
        while ((row = sqlNextRow(sr)) != NULL)
            slNameAddHead(&termList, row[0]);
        sqlFreeResult(&sr);
        slSort(&termList, slNameCmpCase);
        dyStringPrintf(output, "[\n");
        for (el = termList, i = 0; el != NULL; el = el->next, i++)
            {
            if(i)
                dyStringPrintf(output, ",\n");
            dyStringPrintf(output, "'%s'", javaScriptLiteralEncode(el->name));
            }
        dyStringPrintf(output, "\n]\n");
        }
    else
        fail("Assembly does not support metaDb");
    }
else
    fail("Unsupported 'cmd' parameter");

// It's debatable whether the type should be text/plain, text/javascript or application/javascript; I think
// any of the types containing "javascript" don't work with IE6, so I'm using text/plain

puts("Content-Type:text/javascript\n");
//puts("\n");
if(jsonp)
    printf("%s(%s)", jsonp, dyStringContents(output));
else 
    puts(dyStringContents(output));

return 0;
}

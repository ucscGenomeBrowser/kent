/* hgApi - provide a JSON based API to the browser. */

#include "common.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "dystring.h"

static char const rcsid[] = "$Id: hgApi.c,v 1.2 2010/05/21 22:29:49 larrym Exp $";

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
               tabs, javaScriptLiteralEncode(tdb->shortLabel), tabs, javaScriptLiteralEncode(tdb->longLabel),
               tabs, javaScriptLiteralEncode(tdb->grp));
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
struct dyString *output = newDyString(1000);
char *database = cgiOptionalString("db");
char *cmd = cgiOptionalString("cmd");
char *jsonp = cgiOptionalString("jsonp");
if(database)
    database = sqlEscapeString(database);

if(!cmd)
    fail("Missing 'cmd' parameter");

if(!strcmp(cmd, "trackList"))
    {
    if(database)
        {
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
    else
        {
        fail("Missing 'db' parameter");
        }
    }
else
    fail("Unsupported 'cmd' parameter");

// It's debatable whether the type should be text/plain, text/javascript or application/javascript; I think
// any of the types containing "javascript" don't work with IE6, so I'm using text/plain

puts("Content-Type:text/plain");
puts("\n");
if(jsonp)
    printf("%s(%s)", jsonp, dyStringContents(output));
else 
    puts(dyStringContents(output));

return 0;
}

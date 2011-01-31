/* hgApi - provide a JSON based API to the browser. */

#include "common.h"
#include "hdb.h"
#include "mdb.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "dystring.h"
#include "hui.h"

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

#define MDB_VAL_TRUNC_AT 64

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
    tdbList = hTrackDb(database);
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
        char *var = cgiOptionalString("var");
        if(var)
            var = sqlEscapeString(var);
        else
            fail("Missing var parameter");
        struct slPair *pairs = mdbValLabelSearch(conn, var, MDB_VAL_STD_TRUNCATION, TRUE, FALSE); // Tables not files
        struct slPair *pair;
        dyStringPrintf(output, "[\n");
        for (pair = pairs; pair != NULL; pair = pair->next)
            {
            if(pair != pairs)
                dyStringPrintf(output, ",\n");
            dyStringPrintf(output, "['%s','%s']", javaScriptLiteralEncode(pair->name), javaScriptLiteralEncode(pair->val));
            }
        dyStringPrintf(output, "\n]\n");
        }
    else
        fail("Assembly does not support metaDb");
    }
// TODO: move to lib since hgTracks and hgApi share
#define METADATA_VALUE_PREFIX    "hgt_mdbVal"
else if(startsWith(METADATA_VALUE_PREFIX, cmd))
    {
    // Returns metaDb value control: drop down or free text, with or without help link.
    // e.g. http://genome.ucsc.edu/hgApi?db=hg18&cmd=hgt_mdbVal3&var=cell

    // TODO: Move guts to lib, so that hgTracks::searchTracks.c and hgApi.c can share

    struct sqlConnection *conn = hAllocConn(database);
    boolean metaDbExists = sqlTableExists(conn, "metaDb");
    if(metaDbExists)
        {
        char *var = cgiOptionalString("var");
        if(var)
            var = sqlEscapeString(var);
        else
            fail("Missing var parameter");

        int ix = atoi(cmd+strlen(METADATA_VALUE_PREFIX)); // 1 based index
        if(ix == 0) //
            fail("Unsupported 'cmd' parameter");

        enum mdbCvSearchable searchBy = mdbCvSearchMethod(var);
        if (searchBy == cvsSearchBySingleSelect)
            {
            dyStringPrintf(output,"<SELECT NAME=\"%s%i\" class='mdbVal single' style='min-width:200px; font-size:.9em;' onchange='findTracksSearchButtonsEnable(true);'>\n",
                            METADATA_VALUE_PREFIX, ix);

            // Get options list
            struct slPair *pairs = mdbValLabelSearch(conn, var, MDB_VAL_STD_TRUNCATION, TRUE, FALSE); // Tables not files
            struct slPair *pair;
            if (pairs == NULL)
                fail("No selectable values for this metadata variable");

            dyStringPrintf(output, "<OPTION VALUE='Any'>Any</OPTION>\n");
            dyStringPrintf(output, "[\n");
            for (pair = pairs; pair != NULL; pair = pair->next)
                {
                dyStringPrintf(output, "<OPTION VALUE=\"%s\">%s</OPTION>\n", javaScriptLiteralEncode(pair->val), javaScriptLiteralEncode(pair->name));
                }
            dyStringPrintf(output,"</SELECT>\n");
            }
        else if (searchBy == cvsSearchByFreeText)
            {
            dyStringPrintf(output,"<input type='text' name='%s%i' value='' class='mdbVal freeText' onkeyup='findTracksSearchButtonsEnable(true);' style='max-width:310px; width:310px; font-size:.9em;'>",
                            METADATA_VALUE_PREFIX, ix);
            }
        //else if (searchBy == cvsSearchByMultiSelect)
        //    {
        //    // TO BE IMPLEMENTED
        //    }
        //else if (searchBy == cvsSearchByDateRange || searchBy == cvsSearchByDateRange)
        //    {
        //    // TO BE IMPLEMENTED
        //    }
        else
            fail("Metadata variable not searchable");
        }
    else
        fail("Assembly does not support metaDb");
    }
else if(!strcmp(cmd, "tableMetadata"))
    { // returns an html table with metadata for a given track
    char *trackName = cgiOptionalString("track");
    boolean showLonglabel = (NULL != cgiOptionalString("showLonglabel"));
    boolean showShortLabel = (NULL != cgiOptionalString("showShortLabel"));
    if (trackName != NULL)
        {
        struct trackDb *tdb = hTrackDbForTrackAndAncestors(database, trackName); // Doesn't get whole track list
        if (tdb != NULL)
            {
            char * html = metadataAsHtmlTable(database,tdb,showLonglabel,showShortLabel,NULL);
            if (html)
                {
                dyStringAppend(output,html);
                freeMem(html);
                }
            else
                dyStringPrintf(output,"No metadata found for track %s.",trackName);
            }
        else
            dyStringPrintf(output,"Track %s not found",trackName);
        }
        else
            dyStringAppend(output,"No track variable found");
    }
else
    {
    warn("unknwon cmd: %s",cmd);
    fail("Unsupported 'cmd' parameter");
    }

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

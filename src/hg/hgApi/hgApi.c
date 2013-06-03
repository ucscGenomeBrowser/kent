/* hgApi - provide a JSON based API to the browser. 

Required CGI parameters:

db: assembly
cmd: command (see below)

Optional CGI parameters:

jsonp: if present, the returned json is wrapped in a call to the value of the jsonp parameter (e.g. "jsonp=parseResponse").

Supported commands:

defaultPos: default position for this assembly

metaDb: return list of values for metaDb parameter

hgt_mdbVal: return metaDb value control - see code for details

tableMetadata: returns an html table with metadata for track parameter

codonToPos: returns genomic position for given codon; parameters: codon, table and name (which is gene name).

codonToPos: returns genomic position for given exon; parameters: exon, table and name (which is gene name).
*/

#include "common.h"
#include "hdb.h"
#include "mdb.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "dystring.h"
#include "hui.h"
#include "search.h"
#include "cv.h"
#include "api.h"

int main(int argc, char *argv[])
{
long enteredMainTime = clock1000();
struct dyString *output = newDyString(10000);

cgiSpoof(&argc, argv);
pushWarnHandler(apiWarnAbortHandler);
pushAbortHandler(apiWarnAbortHandler);

char *database = cgiString("db");
char *cmd = cgiString("cmd");
char *jsonp = cgiOptionalString("jsonp");
if (!hDbExists(database))
    errAbort("Invalid database '%s'", database);

if (!strcmp(cmd, "defaultPos"))
    {
    dyStringPrintf(output, "{\"pos\": \"%s\"}", hDefaultPos(database));
    }
else if (!strcmp(cmd, "metaDb"))
    {
    // Return list of values for given metaDb var
    // e.g. http://genome.ucsc.edu/hgApi?db=hg18&cmd=metaDb&var=cell

    struct sqlConnection *conn = hAllocConn(database);
    boolean metaDbExists = sqlTableExists(conn, "metaDb");
    if (metaDbExists)
        {
        char *var = cgiOptionalString("var");
        if (!var)
            errAbort("Missing var parameter");
        boolean fileSearch = (cgiOptionalInt("fileSearch",0) == 1);
        struct slPair *pairs = mdbValLabelSearch(conn, var, MDB_VAL_STD_TRUNCATION, FALSE,
                                                 !fileSearch, fileSearch);
        struct slPair *pair;
        dyStringPrintf(output, "[\n");
        for (pair = pairs; pair != NULL; pair = pair->next)
            {
            if (pair != pairs)
                dyStringPrintf(output, ",\n");
            dyStringPrintf(output, "['%s','%s']", javaScriptLiteralEncode(mdbPairLabel(pair)),
                           javaScriptLiteralEncode(mdbPairVal(pair)));
            }
        dyStringPrintf(output, "\n]\n");
        }
    else
        errAbort("Assembly does not support metaDb");
    }
// TODO: move to lib since hgTracks and hgApi share
#define METADATA_VALUE_PREFIX    "hgt_mdbVal"
else if (startsWith(METADATA_VALUE_PREFIX, cmd))
    {
    // Returns metaDb value control: drop down or free text, with or without help link.
    // e.g. http://genome.ucsc.edu/hgApi?db=hg18&cmd=hgt_mdbVal3&var=cell

    // TODO: Move guts to lib, so that hgTracks::searchTracks.c and hgApi.c can share

    struct sqlConnection *conn = hAllocConn(database);
    boolean metaDbExists = sqlTableExists(conn, "metaDb");
    if (metaDbExists)
        {
        char *var = cgiOptionalString("var");
        if (!var)
            errAbort("Missing var parameter");

        int ix = atoi(cmd+strlen(METADATA_VALUE_PREFIX)); // 1 based index
        if (ix == 0) //
            errAbort("Unsupported 'cmd' parameter");

        enum cvSearchable searchBy = cvSearchMethod(var);
        char name[128];
        safef(name,sizeof name,"%s%i",METADATA_VALUE_PREFIX,ix);
        if (searchBy == cvSearchBySingleSelect || searchBy == cvSearchByMultiSelect)
            {
            boolean fileSearch = (cgiOptionalInt("fileSearch",0) == 1);
            struct slPair *pairs = mdbValLabelSearch(conn, var, MDB_VAL_STD_TRUNCATION, FALSE,
                                                     !fileSearch, fileSearch);
            if (slCount(pairs) > 0)
                {
                char *dropDownHtml =
                                cgiMakeSelectDropList((searchBy == cvSearchByMultiSelect),
                                                      name, pairs, NULL, ANYLABEL, "mdbVal",
                                                      "style='min-width: 200px; font-size: .9em;' "
                                                      "onchange='findTracksMdbValChanged(this);'");
                if (dropDownHtml)
                    {
                    dyStringAppend(output,dropDownHtml);
                    freeMem(dropDownHtml);
                    }
                slPairFreeList(&pairs);
                }
            }
        else if (searchBy == cvSearchByFreeText)
            {
            dyStringPrintf(output,"<input type='text' name='%s' value='' class='mdbVal freeText' "
                           "onchange='findTracksMdbValChanged(this);' style='max-width:310px; "
                           "width:310px; font-size:.9em;'>", name);
            }
        else if (searchBy == cvSearchByWildList)
            {
            dyStringPrintf(output,"<input type='text' name='%s' value='' class='mdbVal wildList' "
                           "title='enter comma separated list of values' "
                           "onchange='findTracksMdbValChanged(this);' style='max-width:310px; "
                           "width:310px; font-size:.9em;'>", name);
            }
        else if (searchBy == cvSearchByDateRange || searchBy == cvSearchByIntegerRange)
            {
            // TO BE IMPLEMENTED
            }
        else
            errAbort("Metadata variable not searchable");

        dyStringPrintf(output,"<span id='helpLink%i'>&nbsp;</span>",ix);
        }
    else
        errAbort("Assembly does not support metaDb");
    }
else if (!strcmp(cmd, "tableMetadata"))
    { // returns an html table with metadata for a given track
    char *trackName = cgiOptionalString("track");
    boolean showLonglabel = (NULL != cgiOptionalString("showLonglabel"));
    boolean showShortLabel = (NULL != cgiOptionalString("showShortLabel"));
    if (trackName != NULL)
        {
        // hTrackDbForTrackAndAncestors avoids overhead of getting whole track list!
        struct trackDb *tdb = hTrackDbForTrackAndAncestors(database, trackName);
        if (tdb != NULL)
            {
            char * html = metadataAsHtmlTable(database,tdb,showLonglabel,showShortLabel);
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
else if (sameString(cmd, "codonToPos") || sameString(cmd, "exonToPos"))
    {
    char query[256];
    struct sqlResult *sr;
    char **row;
    struct genePred *gp;
    char *name = cgiString("name");
    char *table = cgiString("table");
    int num = cgiInt("num");
    struct sqlConnection *conn = hAllocConn(database);
    sqlSafef(query, sizeof(query), "select name, chrom, strand, txStart, txEnd, cdsStart, cdsEnd, exonCount, exonStarts, exonEnds from %s where name = '%s'", table, name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        gp = genePredLoad(row);
        boolean found;
        int start, end;
        if (sameString(cmd, "codonToPos"))
            found = codonToPos(gp, num, &start, &end);
        else
            found = exonToPos(gp, num, &start, &end);
        if (found)
            dyStringPrintf(output, "{\"pos\": \"%s:%d-%d\"}", gp->chrom, start + 1, end);
        else
            dyStringPrintf(output, "{\"error\": \"%d is an invalid %s for this gene\"}", num, sameString(cmd, "codonToPos") ? "codon" : "exon");
        }
    else
        dyStringPrintf(output, "{\"error\": \"Couldn't find item: %s\"}", name);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
else
    {
    warn("unknown cmd: %s",cmd);
    errAbort("Unsupported 'cmd' parameter");
    }

apiOut(dyStringContents(output), jsonp);
cgiExitTime("hgApi", enteredMainTime);
return 0;
}

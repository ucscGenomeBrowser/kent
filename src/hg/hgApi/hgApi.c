/* hgApi - provide a JSON based API to the browser. */

#include "common.h"
#include "hdb.h"
#include "mdb.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "dystring.h"
#include "hui.h"
#include "search.h"
#include "encode/encodeExp.h"
#include "cv.h"

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

static void encodeExpJson(struct dyString *json, struct encodeExp *el)
/* Print out encodeExp in JSON format. Manually converted from autoSql which outputs
 * to file pointer.
 */
// TODO: move to lib/encode/encodeExp.c
{
dyStringPrintf(json, "{");
dyStringPrintf(json, "\"ix\":%u", el->ix);
dyStringPrintf(json, ", ");
dyStringPrintf(json, "\"organism\":\"%s\"", el->organism);
dyStringPrintf(json, ", ");
dyStringPrintf(json, "\"lab\":\"%s\"", el->lab);
dyStringPrintf(json, ", ");
dyStringPrintf(json, "\"dataType\":\"%s\"", el->dataType);
dyStringPrintf(json, ", ");
dyStringPrintf(json, "\"cellType\":\"%s\"", el->cellType);
dyStringPrintf(json, ", ");
/* TODO: expand expVars to elements */
dyStringPrintf(json, "\"expVars\":\"%s\"", el->expVars);
dyStringPrintf(json, ", ");
dyStringPrintf(json, "\"accession\":\"%s\"", el->accession);
dyStringPrintf(json, "}");
}

static void cvTermJson(struct dyString *json, char *type, struct hash *termHash)
/* Print out CV term in JSON format. Currently just supports dataType, cellType, antibody
 * and antibody types */
// TODO: move to lib/cv.c
{
dyStringPrintf(json, "{");
dyStringPrintf(json, "\"term\":\"%s\"", (char *)hashFindVal(termHash, "term"));
dyStringPrintf(json, ",");

if (sameString(type, "dataType"))
    {
    dyStringPrintf(json, "\"label\":\"%s\"", (char *)hashOptionalVal(termHash, "label", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"dataGroup\":\"%s\"", (char *)hashOptionalVal(termHash, "dataGroup", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"description\":\"%s\"", (char *)hashOptionalVal(termHash, "description", "unknown"));
    }
else if (sameString(type, "cellType"))
    {
    dyStringPrintf(json, "\"description\":\"");
    // TODO: handle modularly
    dyStringAppendEscapeQuotes(json, (char *)hashOptionalVal(termHash, "description", "unknown"), '"', '\\');
    dyStringPrintf(json, "\",");
    dyStringPrintf(json, "\"tier\":\"%s\"", (char *)hashOptionalVal(termHash, "tier", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"karyotype\":\"");
    dyStringAppendEscapeQuotes(json, (char *)hashOptionalVal(termHash, "karyotype", "unknown"), '"', '\\');
    dyStringPrintf(json, "\",");
    dyStringPrintf(json, "\"organism\":\"%s\"", (char *)hashOptionalVal(termHash, "organism", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"sex\":\"%s\"", (char *)hashOptionalVal(termHash, "sex", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"tissue\":\"%s\"", (char *)hashOptionalVal(termHash, "tissue", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"vendorName\":\"%s\"", (char *)hashOptionalVal(termHash, "vendorName", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"vendorId\":\"%s\"", (char *)hashOptionalVal(termHash, "vendorId", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"lineage\":\"%s\"", (char *)hashOptionalVal(termHash, "lineage", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"termId\":\"%s\"", (char *)hashOptionalVal(termHash, "termId", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"termUrl\":\"%s\"", (char *)hashOptionalVal(termHash, "termUrl", "unknown"));
    // TODO: add URL protocol file ?
    }
else if (sameString(type, "antibody"))
    {
    dyStringPrintf(json, "\"target\":\"%s\"", (char *)hashOptionalVal(termHash, "target", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"antibodyDescription\":\"%s\"", (char *)hashOptionalVal(termHash, "antibodyDescription", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"targetDescription\":\"%s\"", (char *)hashOptionalVal(termHash, "targetDescription", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"vendorName\":\"%s\"", (char *)hashOptionalVal(termHash, "vendorName", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"vendorId\":\"%s\"", (char *)hashOptionalVal(termHash, "vendorId", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"lab\":\"%s\"", (char *)hashOptionalVal(termHash, "lab", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"targetId\":\"%s\"", (char *)hashOptionalVal(termHash, "targetId", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"targetUrl\":\"%s\"", (char *)hashOptionalVal(termHash, "targetUrl", "unknown"));
    dyStringPrintf(json, ",");
    dyStringPrintf(json, "\"orderUrl\":\"%s\"", (char *)hashOptionalVal(termHash, "orderUrl", "unknown"));
    // TODO: add validation file(s) ?
    }

dyStringPrintf(json, "}\n");
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
        boolean fileSearch = (cgiOptionalInt("fileSearch",0) == 1);
        struct slPair *pairs = mdbValLabelSearch(conn, var, MDB_VAL_STD_TRUNCATION, FALSE, !fileSearch, fileSearch); // not tags, either a file or table search
        struct slPair *pair;
        dyStringPrintf(output, "[\n");
        for (pair = pairs; pair != NULL; pair = pair->next)
            {
            if(pair != pairs)
                dyStringPrintf(output, ",\n");
            dyStringPrintf(output, "['%s','%s']", javaScriptLiteralEncode(mdbPairLabel(pair)), javaScriptLiteralEncode(mdbPairVal(pair)));
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

        enum cvSearchable searchBy = cvSearchMethod(var);
        char name[128];
        safef(name,sizeof name,"%s%i",METADATA_VALUE_PREFIX,ix);
        if (searchBy == cvSearchBySingleSelect || searchBy == cvSearchByMultiSelect)
            {
            boolean fileSearch = (cgiOptionalInt("fileSearch",0) == 1);
            struct slPair *pairs = mdbValLabelSearch(conn, var, MDB_VAL_STD_TRUNCATION, FALSE, !fileSearch, fileSearch); // not tags, either a file or table search
            if (slCount(pairs) > 0)
                {
                char *dropDownHtml = cgiMakeSelectDropList((searchBy == cvSearchByMultiSelect),
                        name, pairs,NULL, ANYLABEL,"mdbVal", "style='min-width: 200px; font-size: .9em;' onchange='findTracksMdbValChanged(this);'");
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
            dyStringPrintf(output,"<input type='text' name='%s' value='' class='mdbVal freeText' onchange='findTracksMdbValChanged(this);' style='max-width:310px; width:310px; font-size:.9em;'>",
                            name);
            }
        else if (searchBy == cvSearchByWildList)
            {
            dyStringPrintf(output,"<input type='text' name='%s' value='' class='mdbVal wildList' title='enter comma separated list of values' onchange='findTracksMdbValChanged(this);' style='max-width:310px; width:310px; font-size:.9em;'>",
                            name);
            }
        else if (searchBy == cvSearchByDateRange || searchBy == cvSearchByIntegerRange)
            {
            // TO BE IMPLEMENTED
            }
        else
            fail("Metadata variable not searchable");

        dyStringPrintf(output,"<span id='helpLink%i'>&nbsp;</span>",ix);
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
else if(sameString(cmd, "codonToPos") || sameString(cmd, "exonToPos"))
    {
    char query[256];
    struct sqlResult *sr;
    char **row;
    struct genePred *gp;
    char *name = cgiString("name");
    char *table = cgiString("table");
    int num = cgiInt("num");
    struct sqlConnection *conn = hAllocConn(database);
    safef(query, sizeof(query), "select name, chrom, strand, txStart, txEnd, cdsStart, cdsEnd, exonCount, exonStarts, exonEnds from %s where name = '%s'", sqlEscapeString(table), sqlEscapeString(name));
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
        gp = genePredLoad(row);
        boolean found;
        int start, end;
        if(sameString(cmd, "codonToPos"))
            found = codonToPos(gp, num, &start, &end);
        else
            found = exonToPos(gp, num, &start, &end);
        if(found)
            dyStringPrintf(output, "{\"pos\": \"%s:%d-%d\"}", gp->chrom, start + 1, end);
        else
            dyStringPrintf(output, "{\"error\": \"%d is an invalid %s for this gene\"}", num, sameString(cmd, "codonToPos") ? "codon" : "exon");
        }
    else
        dyStringPrintf(output, "{\"error\": \"Couldn't find item: %s\"}", name);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
else if(!strcmp(cmd, "encodeExperiments"))
    {
    // Return list of ENCODE experiments.  Note: database is ignored.
    // TODO: add selector for org=human|mouse, retire db=
    // e.g. http://genome.ucsc.edu/cgi-bin/hgApi?db=hg18&cmd=encodeExperiments
    struct sqlConnection *connExp = sqlConnect(ENCODE_EXP_DATABASE);
    /* TODO: any need to use connection pool ? */
    struct encodeExp *exp = NULL, *exps = encodeExpLoadAllFromTable(connExp, ENCODE_EXP_TABLE);
    dyStringPrintf(output, "[\n");
    while ((exp = slPopHead(&exps)) != NULL)
        {
        encodeExpJson(output, exp);
        dyStringAppend(output,",\n");
        }
    output->string[dyStringLen(output)-2] = '\n';
    output->string[dyStringLen(output)-1] = ']';
    dyStringPrintf(output, "\n");
    sqlDisconnect(&connExp);
    }
else if (!strcmp(cmd, "cv"))
    {
    // Return list of CV terms for the specified term type
    // Just supporting cellType, dataType, and antibody initially
    // TODO: retire db=
    // e.g. http://genome.ucsc.edu/cgi-bin/hgApi?db=hg19&cmd=cv&file=cv.ra&type=dataType
    char *type = cgiString("type");
    char *cvFile = cgiOptionalString("file");
    if (cvFile != NULL)
        cvFileDeclare(cvFile);
    if (differentString(type, "dataType") &&
        differentString(type, "cellType") &&
        differentString(type, "antibody"))
            {
            warn("Unsupported CV type %s (must be dataType, cellType, antibody)", type);
            fail("Unsupported 'cmd' parameter");
            }
    dyStringPrintf(output, "[\n");
    struct hash *typeHash = (struct hash *)cvTermHash(cvTermNormalized(type));
    struct hashCookie hc = hashFirst(typeHash);
    struct hashEl *hel;
    while ((hel = hashNext(&hc)) != NULL)
        {
        cvTermJson(output, type, hel->val);
        dyStringAppend(output,",");
        }
    output->string[dyStringLen(output)-1] = 0;
    output->stringSize--;
    dyStringPrintf(output, "\n]\n");
    }
else
    {
    warn("unknown cmd: %s",cmd);
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

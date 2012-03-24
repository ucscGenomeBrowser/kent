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

cv: Return list of CV terms for the specified term type; just supporting cellType, dataType, and antibody initially
    e.g. http://genome.ucsc.edu/cgi-bin/hgApi?db=hg19&cmd=cv&file=cv.ra&type=dataType

*/

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

static void encodeExpJson(struct dyString *json, struct encodeExp *el)
/* Print out encodeExp in JSON format. Manually converted from autoSql which outputs
 * to file pointer.
 */
// TODO: move to lib/encode/encodeExp.c, extend autoSql to support, and use json*() functions
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

static void warnAbortHandler(char *format, va_list args)
/* warnAbort handler that aborts with an HTTP 400 status code. */
{
puts("Status: 400\n\n");
vfprintf(stdout, format, args);
exit(-1);
}

int main(int argc, char *argv[])
{
struct dyString *output = newDyString(10000);

cgiSpoof(&argc,argv);
pushWarnHandler(warnAbortHandler);
pushAbortHandler(warnAbortHandler);

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
        if (var)
            var = sqlEscapeString(var);
        else
            errAbort("Missing var parameter");
        boolean fileSearch = (cgiOptionalInt("fileSearch",0) == 1);
        struct slPair *pairs = mdbValLabelSearch(conn, var, MDB_VAL_STD_TRUNCATION, FALSE, !fileSearch, fileSearch); // not tags, either a file or table search
        struct slPair *pair;
        dyStringPrintf(output, "[\n");
        for (pair = pairs; pair != NULL; pair = pair->next)
            {
            if (pair != pairs)
                dyStringPrintf(output, ",\n");
            dyStringPrintf(output, "['%s','%s']", javaScriptLiteralEncode(mdbPairLabel(pair)), javaScriptLiteralEncode(mdbPairVal(pair)));
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
        if (var)
            var = sqlEscapeString(var);
        else
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
        struct trackDb *tdb = hTrackDbForTrackAndAncestors(database, trackName); // Doesn't get whole track list
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
    safef(query, sizeof(query), "select name, chrom, strand, txStart, txEnd, cdsStart, cdsEnd, exonCount, exonStarts, exonEnds from %s where name = '%s'", sqlEscapeString(table), sqlEscapeString(name));
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
else if (!strcmp(cmd, "encodeExperiments"))
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
else if (!strcmp(cmd, "encodeExpId"))
    {
    // Return list of ENCODE expID's found in a database 
    struct sqlResult *sr;
    char **row;
    char query[256];
    struct sqlConnection *conn = hAllocConn(database);
    safef(query, sizeof(query), "select distinct(%s) from %s where %s='%s' order by (%s + 0)",
                        MDB_VAL, MDB_DEFAULT_NAME, MDB_VAR, MDB_VAR_ENCODE_EXP_ID, MDB_VAL);
    sr = sqlGetResult(conn, query);
    dyStringPrintf(output, "[\n");
    while ((row = sqlNextRow(sr)) != NULL)
        {
        dyStringPrintf(output, "{\"expId\": \"%s\"},\n", row[0]);
        }
    output->string[dyStringLen(output)-2] = '\n';
    output->string[dyStringLen(output)-1] = ']';
    dyStringPrintf(output, "\n");
    sqlFreeResult(&sr);
    hFreeConn(&conn);
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
            errAbort("Unsupported 'cmd' parameter");
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
    errAbort("Unsupported 'cmd' parameter");
    }

// It's debatable whether the type should be text/plain, text/javascript or application/javascript;
// text/javascript works with all our supported browsers, so we are using that one.
puts("Content-Type:text/javascript\n");

//puts("\n");
if (jsonp)
    printf("%s(%s)", jsonp, dyStringContents(output));
else
    puts(dyStringContents(output));

return 0;
}

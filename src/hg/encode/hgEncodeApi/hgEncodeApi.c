/* hgEncodeApi - a web API to selected ENCODE resources

The API requests here are intended to follow the REST architectural approach.  
Each API request accesses a resource, and no state is maintained. While implemented
as a CGI, to follow the REST paradigm, access should be via resource identifiers translated
from the CGI command and paramters via Apache rewriting.

Initial implementation is limited to a few requests needed by UCSC applications, 
and only supports JSON format responses.

Example use (with rewriting) might be:

http://api.wgEncode.ucsc.edu/cv/cells
    hgEncodeApi?cmd=cv&type=cells

http://api.wgEncode.ucsc.edu/cv/cells/K562
    hgEncodeApi?cmd=cv&type=cells&term=K562

http://api.wgEncode.ucsc.edu/cv/cells/organism=mouse
    hgEncodeApi?cmd=cv&type=cells&organism=mouse

Note: full RESTful style would limit response lists to specified count, providing
URL's and next/previous links in a paging style (not implemented here).
*/

#include "common.h"
#include "hdb.h"
#include "mdb.h"
#include "cheapcgi.h"
#include "hPrint.h"
#include "dystring.h"
/*
#include "hui.h"
#include "search.h"
*/
#include "encode/encodeExp.h"
#include "cv.h"

/*
http://api.wgEncode.ucsc.edu/cv/assays
http://api.wgEncode.ucsc.edu/cv/assays/ChipSeq
http://api.wgEncode.ucsc.edu/cv/antibodies
http://api.wgEncode.ucsc.edu/cv/antibodies/Pol2
http://api.wgEncode.ucsc.edu/experiments
http://api.wgEncode.ucsc.edu/experiments/2312
http://api.wgEncode.ucsc.edu/experiments/assembly=hg19
*/

/* JSON representations of API resources */

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

/* Error handling */

// TODO: move to lib/api.c
static void warnAbortHandler(char *format, va_list args)
/* warnAbort handler that aborts with an HTTP 400 status code. */
{
puts("Status: 400\n\n");
vfprintf(stdout, format, args);
exit(-1);
}

int main(int argc, char *argv[])
/* Process command line */
{
struct dyString *output = newDyString(10000);

cgiSpoof(&argc, argv);
pushWarnHandler(warnAbortHandler);
pushAbortHandler(warnAbortHandler);

char *cmd = cgiString("cmd");
char *jsonp = cgiOptionalString("jsonp");
if (!strcmp(cmd, "experiments"))
    {
    // Return list of ENCODE experiments
    // TODO: add filtering by assembly and retire experimentIds command
    // e.g. http://genome.ucsc.edu/cgi-bin/hgApi?db=hg18&cmd=experiments
    // NOTE:  This table lives only on development and preview servers -- use preview
    //  if not found on localhost
    struct sqlConnection *connExp = sqlConnect(ENCODE_EXP_DATABASE);
    if (!sqlTableExists(connExp, ENCODE_EXP_TABLE))
        {
        sqlDisconnect(&connExp);
        connExp = sqlConnectProfile("preview", ENCODE_EXP_DATABASE);
        }
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
else if (!strcmp(cmd, "experimentIds"))
    {
    // Return list of ENCODE expID's found in a database 
    struct sqlResult *sr;
    char **row;
    char query[256];

    char *database = cgiString("db");
    if (!hDbExists(database))
        errAbort("Invalid database '%s'", database);

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

// TODO: move to lib/api.c
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

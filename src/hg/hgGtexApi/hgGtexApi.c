/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* hgGtexApi - a web API to selected GTEx resources
 *
 * usage:  http://hgwdev-kate.gi.ucsc.edu/cgi-bin/hgGtexApi?gene=ENSG00000223972.4
 *
 * returns JSON in the format:
 *      { "id": <donor>, "tissue": <tissue>, "rpkm": <rpkm> }
*/

#include "common.h"
#include "hdb.h"
#include "htmshell.h"
#include "hPrint.h"
#include "dystring.h"
#include "api.h"
#include "gtexSampleData.h"

/* Requests */

// TODO: Move to lib

void gtexSampleDataJson(struct dyString *json, struct gtexSampleData *data)
/* Print out gtexSampleData in JSON format. */
{
dyStringPrintf(json, "{");
dyStringPrintf(json, "\"sample\":\"%s\"", data->sample);
dyStringPrintf(json, ", ");
dyStringPrintf(json, "\"tissue\":\"%s\"", data->tissue);
dyStringPrintf(json, ", ");
dyStringPrintf(json, "\"rpkm\":%.3f", data->score);
dyStringPrintf(json, "}");
}

void geneResource(struct dyString *output)
/* Retrieve gene expression for specified gene, and create JSON to output. 
   Initially supporting only cellType, dataType, and antibody (type= parameter).
   Allows specifying cv file with file= parameter
        e.g. http://genome.ucsc.edu/cgi-bin/hgApi?db=hg19&cmd=cv&file=cv.ra&type=dataType
*/
{
char query[256];
struct sqlResult *sr;
char **row;
dyStringPrintf(output, "[\n");
struct sqlConnection *conn = sqlConnect("hgFixed");
char *gene = cgiString("gene");
sqlSafef(query, sizeof(query), "select * from gtexSampleData where geneId='%s'", gene);
sr = sqlGetResult(conn, query);
struct gtexSampleData *data;
while ((row = sqlNextRow(sr)) != NULL)
    {
    data = gtexSampleDataLoad(row);
    gtexSampleDataJson(output, data);
    dyStringAppend(output,",\n");
    }
output->string[dyStringLen(output)-2] = '\n';
output->string[dyStringLen(output)-1] = ']';
dyStringPrintf(output, "\n");
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line */
{
long enteredMainTime = clock1000();
struct dyString *output = newDyString(10000);

cgiSpoof(&argc, argv);
pushWarnHandler(htmlVaBadRequestAbort);
pushAbortHandler(htmlVaBadRequestAbort);

char *jsonp = cgiOptionalString("jsonp");
geneResource(output);
apiOut(dyStringContents(output), jsonp);
cgiExitTime("hgGtexApi", enteredMainTime);
return 0;
}

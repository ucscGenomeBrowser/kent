/* hgEncodeApi - a web API to selected ENCODE resources

The API requests here are intended to follow the REST architectural approach.  
Each API request accesses a resource, and no state is maintained. While implemented
as a CGI, to follow the REST paradigm, access should be via resource identifiers translated
from the CGI command and parameters via Apache rewriting.

Initial implementation is limited to a few requests needed by UCSC applications, 
and only supports JSON format responses.

Example use (with rewriting) might be:

http://api.wgEncode.ucsc.edu/cv/cells
    hgEncodeApi?cmd=cv&type=cellType

http://api.wgEncode.ucsc.edu/cv/cells/K562
    hgEncodeApi?cmd=cv&type=cellType&term=K562

http://api.wgEncode.ucsc.edu/cv/cells/organism=mouse
    hgEncodeApi?cmd=cv&type=cellType&organism=mouse

Note: full RESTful style would limit response lists to specified count, providing
URL's and next/previous links in a paging style (not implemented here).
*/

/*
Some additional requests in the same style (TBD):
http://api.wgEncode.ucsc.edu/cv/assays
http://api.wgEncode.ucsc.edu/cv/assays/ChipSeq
http://api.wgEncode.ucsc.edu/cv/antibodies
http://api.wgEncode.ucsc.edu/cv/antibodies/Pol2
http://api.wgEncode.ucsc.edu/experiments
http://api.wgEncode.ucsc.edu/experiments/2312
http://api.wgEncode.ucsc.edu/experiments/assembly=hg19
*/

#include "common.h"
#include "hdb.h"
#include "mdb.h"
#include "hPrint.h"
#include "dystring.h"
#include "api.h"
#include "encode/encodeExp.h"
#include "cv.h"

/* Requests */

void cvResource(struct dyString *output)
/* Retrieve CV entries from CV file and format as JSON to output. 
   Initially supporting only cellType, dataType, and antibody (type= parameter).
   Allows specifying cv file with file= parameter
        e.g. http://genome.ucsc.edu/cgi-bin/hgApi?db=hg19&cmd=cv&file=cv.ra&type=dataType
*/
{
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

void experimentsResource(struct dyString *output)
/* Retrieve experiments from database, optionally filtering by assembly (db= parameter)
   e.g. http://genome.ucsc.edu/cgi-bin/hgEncodeApi?cmd=experiments&db=hg19
*/
{
struct sqlConnection *connExp = sqlConnect(ENCODE_EXP_DATABASE);
if (!sqlTableExists(connExp, ENCODE_EXP_TABLE))
    {
    errAbort("Experiment table not found on server");
    }

int *ids = NULL;
struct sqlConnection *conn;
char *database = cgiOptionalString("db");
if (database)
    {
    // Get ids of all experiments in metadata of this database
    // so those not in this assembly can be filtered out
    struct sqlResult *sr;
    char **row;
    char query[256];
    int id, maxId;
    if (!hDbExists(database))
        errAbort("Invalid database '%s'", database);
    int idOffset = encodeExpIdOffset();
    maxId = encodeExpIdMax(connExp);
    conn = hAllocConn(database);
    safef(query, sizeof(query), "select distinct(%s) from %s where %s='%s'",
                    MDB_VAL, MDB_DEFAULT_NAME, MDB_VAR, MDB_VAR_DCC_ACCESSION);
    sr = sqlGetResult(conn, query);
    AllocArray(ids, maxId + 1); // ids start with 1
    while ((row = sqlNextRow(sr)) != NULL)
        {
        id = sqlUnsigned(row[0] + idOffset);
        if (id <= maxId) 
            ids[id] = 1;
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    }
struct encodeExp *exp = NULL, *exps = encodeExpLoadAllFromTable(connExp, ENCODE_EXP_TABLE);
dyStringPrintf(output, "[\n");
while ((exp = slPopHead(&exps)) != NULL)
    {
    // filter out experiments not in the selected database
    if (database && ids[exp->ix] == 0)
            continue;
    encodeExpJson(output, exp);
    dyStringAppend(output,",\n");
    }
output->string[dyStringLen(output)-2] = '\n';
output->string[dyStringLen(output)-1] = ']';
dyStringPrintf(output, "\n");
sqlDisconnect(&connExp);
}

int main(int argc, char *argv[])
/* Process command line */
{
long enteredMainTime = clock1000();
struct dyString *output = newDyString(10000);

cgiSpoof(&argc, argv);
pushWarnHandler(apiWarnAbortHandler);
pushAbortHandler(apiWarnAbortHandler);

char *cmd = cgiString("cmd");
char *jsonp = cgiOptionalString("jsonp");
if (!strcmp(cmd, "cv"))
    {
    cvResource(output);
    }
else if (!strcmp(cmd, "experiments"))
    {
    experimentsResource(output);
    }
else
    {
    warn("unknown cmd: %s",cmd);
    errAbort("Unsupported 'cmd' parameter");
    }

apiOut(dyStringContents(output), jsonp);
cgiExitTime("hgEncodeApi", enteredMainTime);
return 0;
}

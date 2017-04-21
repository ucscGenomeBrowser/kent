/* cdwRenameDataset - Rename a dataset, updating cdwDataset, cdwSubmitDir.url, and cdwSubmit.url 
 * tables.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdwLib.h"

boolean clDry = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwRenameDataset - Rename a dataset, updating cdwDataset, cdwSubmitDir.url, and cdwSubmit.url\n"
  "tables.  You'll still need to update meta.txt and resubmit it, and rename the wrangle dir.\n"
  "usage:\n"
  "   cdwRenameDataset oldName newName\n"
  "options:\n"
  "   -dry - don't actually modify database.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {NULL, 0},
};

static void update(struct sqlConnection *conn, char *query)
/* If clDry permits update database, else just print. */
{
if (clDry)
    verbose(1, "%s\n", query);
else
    sqlUpdate(conn, query);
}

static void mustStrSwapOne(char *string, int sz, char *oldStr, char *newStr)
/* Swap in a single string or die trying */
{
int subCount = strSwapStrs(string, sz, oldStr, newStr);
if (subCount != 1)
    errAbort("%d %s found in %s in mustStrSwapOne, must occur once", subCount, oldStr, string);
}

void cdwRenameDataset(char *oldName, char *newName)
/* cdwRenameDataset - Rename a dataset, updating cdwDataset, cdwSubmitDir.url, and cdwSubmit.url 
 * tables. */
{
/* Update entry in cdwDataset */
struct sqlConnection *conn = cdwConnectReadWrite();
char query[3*PATH_LEN];
sqlSafef(query, sizeof(query), 
    "update cdwDataset set name='%s' where name ='%s'", newName, oldName);
update(conn, query);

/* Figure out submitDir that ends with the dataset name */
sqlSafef(query, sizeof(query), "select count(*) from cdwSubmitDir where url like '/%%%s'", oldName);
int dirCount = sqlQuickNum(conn, query);
if (dirCount != 1)
    errAbort("Got %d cdwSubmitDir URLs ending in /%s, expecting 1", dirCount, oldName);
sqlSafef(query, sizeof(query), "select id from cdwSubmitDir where url like '/%%%s'", oldName);
int submitDirId = sqlQuickNum(conn, query);

/* Update cdwSubmitDir.url with substituted url. */
sqlSafef(query, sizeof(query), "select url from cdwSubmitDir where id=%d", submitDirId);
char *oldSubmitUrl = sqlQuickString(conn, query);
char newUrl[2*PATH_LEN];
safef(newUrl, sizeof(newUrl), "%s", oldSubmitUrl);
mustStrSwapOne(newUrl, sizeof(newUrl), oldName, newName);
sqlSafef(query, sizeof(query), "update cdwSubmitDir set url='%s' where id=%d", newUrl, submitDirId);
update(conn, query);

/* Update cdwSubmit.url with substituted url wherever it matches submitDir. */
struct sqlConnection *conn2 = cdwConnectReadWrite();
sqlSafef(query, sizeof(query), "select id,url from cdwSubmit where submitDirId=%d", submitDirId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *id = row[0];
    char *oldUrl = row[1];
    safef(newUrl, sizeof(newUrl), "%s", oldUrl);
    mustStrSwapOne(newUrl, sizeof(newUrl), oldName, newName);
    sqlSafef(query, sizeof(query), "update cdwSubmit set url='%s' where id=%s", newUrl, id);
    update(conn2, query);
    }
sqlFreeResult(&sr);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clDry = optionExists("dry");
cdwRenameDataset(argv[1],argv[2]);
return 0;
}

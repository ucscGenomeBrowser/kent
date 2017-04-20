/* cdwRenameDataset - Rename a dataset, updating cdwDataset, cdwSubmitDir.url, and cdwSubmit.url tables.. */
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
  "cdwRenameDataset - Rename a dataset, updating cdwDataset, cdwSubmitDir.url, and cdwSubmit.url tables.\n"
  "You'll still need to update meta.txt and resubmit it\n"
  "usage:\n"
  "   cdwRenameDataset oldName newName\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {NULL, 0},
};

void update(struct sqlConnection *conn, char *query)
/* If clDry permits update database, else just print. */
{
if (clDry)
    verbose(1, "%s\n", query);
else
    sqlUpdate(conn, query);
}

void cdwRenameDataset(char *oldName, char *newName)
/* cdwRenameDataset - Rename a dataset, updating cdwDataset, cdwSubmitDir.url, and cdwSubmit.url tables.. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
char query[3*PATH_LEN];
sqlSafef(query, sizeof(query), "update cdwDataset set name='%s' where name ='%s'", newName, oldName);
update(conn, query);
sqlSafef(query, sizeof(query), "select count(*) from cdwSubmitDir where url like '/%%%s'", oldName);
int dirCount = sqlQuickNum(conn, query);
if (dirCount != 1)
    errAbort("Got %d cdwSubmitDir URLs ending in /%s, expecting 1", dirCount, oldName);
sqlSafef(query, sizeof(query), "select id from cdwSubmitDir where url like '/%%%s'", oldName);
int submitDirId = sqlQuickNum(conn, query);
uglyf("submitDirId = %d\n", submitDirId);

sqlSafef(query, sizeof(query), "select url from cdwSubmitDir where id=%d", submitDirId);
char *oldSubmitUrl = sqlQuickString(conn, query);
uglyf("oldSubmitUrl = %s\n", oldSubmitUrl);

char newUrl[2*PATH_LEN];
safef(newUrl, sizeof(newUrl), "%s", oldSubmitUrl);
int subCount = strSwapStrs(newUrl, sizeof(newUrl), oldName, newName);
if (subCount != 1)
    errAbort("Expecting to make 1 substitution, made %d", subCount);
uglyf("newSubmitUrl = %s, subCount=%d\n", newUrl, subCount);
sqlSafef(query, sizeof(query), "update cdwSubmitDir set url='%s' where id=%d", newUrl, submitDirId);
update(conn, query);

struct sqlConnection *conn2 = cdwConnectReadWrite();
sqlSafef(query, sizeof(query), "select id,url from cdwSubmit where submitDirId=%d", submitDirId);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *id = row[0];
    char *oldUrl = row[1];
    safef(newUrl, sizeof(newUrl), "%s", oldUrl);
    int subCount = strSwapStrs(newUrl, sizeof(newUrl), oldName, newName);
    if (subCount != 1)
	errAbort("Expecting to make 1 substitution, made %d", subCount);
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

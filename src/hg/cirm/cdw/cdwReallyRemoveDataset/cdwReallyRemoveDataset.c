/* cdwReallyRemoveDataset - Remove all files from a dataset and in general all traces of it.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdw.h"
#include "cdwLib.h"

boolean really;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwReallyRemoveDataset - Remove all files from a dataset and in general all traces of it.\n"
  "usage:\n"
  "   cdwReallyRemoveDataset submitDir\n"
  "options:\n"
  "   -really - Needs to be set for anything to happen,  otherwise will just print file names.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"really", OPTION_BOOLEAN},
   {NULL, 0},
};

void cdwReallyRemoveDataset(char *submitDir)
/* cdwReallyRemoveDataset - Remove all files from a dataset and in general all traces of it.. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
char query[512];
sqlSafef(query, sizeof(query), "select id from cdwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId <= 0)
    errAbort("%s not found", submitDir);

sqlSafef(query, sizeof(query), "select id from cdwFile where submitDirId=%d", submitDirId);
struct slInt *fileIdList = sqlQuickNumList(conn, query);
verbose(1, "%d files in %s\n", slCount(fileIdList), submitDir);
struct slInt *el;
for (el = fileIdList; el != NULL; el = el->next)
    cdwReallyRemoveFile(conn, submitDir, el->val, really);

sqlSafef(query, sizeof(query), "delete from cdwSubmit where submitDirId = %d", submitDirId);
if (really)
    sqlUpdate(conn, query);


sqlSafef(query, sizeof(query), "delete from cdwSubmitDir where id = %d", submitDirId);
if (really)
    sqlUpdate(conn, query);

verbose(1, "Please remove dataset from cdwDataset table and run cdwMakeFileTags as well\n");
if (!really)
    verbose(1, "-really was not specified. Dry run only.\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
really = optionExists("really");
cdwReallyRemoveDataset(argv[1]);
return 0;
}

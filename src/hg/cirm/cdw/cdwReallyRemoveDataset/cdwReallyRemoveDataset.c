/* cdwReallyRemoveDataset - Remove all files from a dataset and in general all traces of it.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdw.h"
#include "cdwLib.h"

boolean really;
boolean unSymlinkOnly = FALSE;  // if unSymlinkOnly == TRUE do not delete rows in tables cdwSumbit and cdwSumbitDir below.

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwReallyRemoveDataset - Remove all files from a dataset and in general all traces of it.\n"
  "      - It also restores the original symlinks from wrangle directories by replacing them with copies so the wrangle files are not lost.\n"
  "      - This means your space usage will double as symlinks get restored to real files. Make sure you have enough space on the storage device.\n"
  "      - Use mosh and screen to ensure no interruptions.\n"
  "      - It we be slow as it copies big files back so be patient.\n"
  "        The recovered files in wrangle directories will be owned by the curent user.\n"
  "      - DO NOT HIT CONTROL-C!\n"
  "usage:\n"
  "   cdwReallyRemoveDataset submitDir\n"
  "options:\n"
  "   -really - Needs to be set for anything to happen,  otherwise will just print file names.\n"
  "   -unSymlinkOnly - turn symlinks back into actual files in wrangle or valData without removing the files from cdw. Requires twice the space for each dataset.\n"
  "\n"
  "\n"
  "IMPORTANT NOTE - submitDir can be an empty string in which case it applies to all the valData with a blank submitDir.\n"
  "           Never use this with -really unless you are abandoning the installation removing cdwRootDir /data/cirm/cdw/ entirely\n"
  "            and thus need to recover valData so it will not be left with dangling symlinks.\n"

  "                   submitDir can be \"allDatasetsUnSymlink\" in which case it applies to all datasets currently unsymlinking only.\n"
  "             Never use this with -really unless you are abandoning the installation removing cdwRootDir /data/cirm/cdw/ entirely\n"

  "             Please be careful with this command. Ask for help if you have questions.\n"
 );
 
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"really", OPTION_BOOLEAN},
   {"unSymlinkOnly", OPTION_BOOLEAN},
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
    cdwReallyRemoveFile(conn, submitDir, el->val, unSymlinkOnly, really); 

sqlSafef(query, sizeof(query), "delete from cdwSubmit where submitDirId = %d", submitDirId);
if (really && !unSymlinkOnly)
    sqlUpdate(conn, query);


sqlSafef(query, sizeof(query), "delete from cdwSubmitDir where id = %d", submitDirId);
if (really && !unSymlinkOnly)
    sqlUpdate(conn, query);
if (!unSymlinkOnly)
    verbose(1, "Please remove dataset from cdwDataset table and run cdwMakeFileTags as well\n");

if (!really)
    verbose(1, "-really was not specified. Dry run only.\n");

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
really = optionExists("really");
unSymlinkOnly = optionExists("unSymlinkOnly");
if (sameString(argv[1], "allDatasetsUnSymlink"))
    {
    if (sameString(getenv("HOST"), "cirm-01"))
	errAbort("Operation not allowed on cirm-01.");

    unSymlinkOnly = TRUE; // required for additional safety and helps with repeatability since the command can resumed or re-run safely.

    struct sqlConnection *conn = cdwConnectReadWrite();
    struct sqlResult *sr;
    char **row;
    char query[512];
    sqlSafef(query, sizeof(query), "select url from cdwSubmitDir");
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	char *submitDir = row[0];
	verbose(1, "\n\nProcessing submitDir=%s\n", submitDir);
	cdwReallyRemoveDataset(submitDir);
	}
    sqlFreeResult(&sr);
    sqlDisconnect(&conn);
    }
else
    cdwReallyRemoveDataset(argv[1]);
return 0;
}

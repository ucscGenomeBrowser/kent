/* cdwRenameFiles - Rename files submitted name.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "fieldedTable.h"
#include "portable.h"
#include "cdwLib.h"

boolean clDry = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwRenameFiles - Rename files' submitted name both database, manifest, and file system.\n"
  "usage:\n"
  "   cdwRenameFiles rename.tab manifest.txt\n"
  "where:\n"
  "   rename.tab is a two column tab or space separated files with old file name in\n"
  "              one column and current file name in the other\n"
  "   manifest.txt is the manifest file with the old name in it. It will be rewritten with new\n"
  "options:\n"
  "   -dry - only do test run, don't alter database or file\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {NULL, 0},
};

void cdwRenameFiles(char *renameFile, char *maniFile)
/* cdwRenameFiles - Rename files submitted name. */
{
/* Figure out submission directory */
struct sqlConnection *conn = cdwConnectReadWrite();
char *submitDir = getCurrentDir();
char query[3*PATH_LEN];
sqlSafef(query, sizeof(query), "select id from cdwSubmitDir where url='%s'", submitDir);
int submitDirId = sqlQuickNum(conn, query);
if (submitDirId == 0)
    errAbort("%s not in cdwSubmit, run program from submission directory please\n", submitDir);

char *requiredFields[] = {"file",} ;
struct fieldedTable *maniTable = fieldedTableFromTabFile(maniFile, maniFile, requiredFields, 
    ArraySize(requiredFields));
int fileIx = fieldedTableMustFindFieldIx(maniTable, "file");
struct hash *maniHash = fieldedTableUniqueIndex(maniTable, "file");

/* Load in rename file and do one pass to make sure everything exists and is kosher */
struct slPair *mv, *mvList = slPairTwoColumnFile(renameFile);
verbose(1, "Got %d files to rename in %s\n", slCount(mvList), renameFile);
struct hash *dirHash = hashNew(0);
for (mv = mvList; mv != NULL; mv = mv->next)
    {
    char *oldName = mv->name;
    char *newName = mv->val;
    char newDir[PATH_LEN];
    splitPath(newName, newDir, NULL, NULL);
    if (!isEmpty(newDir))
       {
       if (!hashLookup(dirHash, newDir))
           {
	   if (!clDry)
	       makeDir(newDir);
	   hashAdd(dirHash, newDir, NULL);
	   }
       }
    verbose(2, "checking %s->%s\n", oldName, newName);
    if (sameString(oldName, newName))
        errAbort("Can't rename %s to itself", oldName);
    struct fieldedRow *fr = hashFindVal(maniHash, oldName);
    sqlSafef(query, sizeof(query), 
	"select count(*) from cdwFile where submitFileName='%s' and submitDirId=%d",
	oldName, submitDirId);
    int count = sqlQuickNum(conn, query);
    if (count == 0)
        errAbort("%s not found in database in %s", oldName, submitDir);
    if (fr == NULL)
        errAbort("%s not found in %s", oldName, maniFile);
    if (!fileExists(oldName))
        errAbort("%s file does not exist.", oldName);
    }

for (mv = mvList; mv != NULL; mv = mv->next)
    {
    char *oldName = mv->name;
    char *newName = mv->val;
    verbose(2, "doing %s->%s\n", oldName, newName);
    struct fieldedRow *fr = hashFindVal(maniHash, oldName);
    fr->row[fileIx] = newName;
    sqlSafef(query, sizeof(query), 
	"update cdwFile set submitFileName='%s' where submitFileName='%s' and submitDirId=%d",
	newName, oldName, submitDirId);
    if (!clDry)
	{
        sqlUpdate(conn, query);
	mustRename(oldName, newName);
	}
    else
        verbose(1, "%s\n", query);
    }
if (!clDry)
    fieldedTableToTabFile(maniTable, maniFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clDry = optionExists("dry");
cdwRenameFiles(argv[1], argv[2]);
return 0;
}

#include "common.h"
#include "sqlDeleter.h"
#include "jksql.h"
#include "gbVerb.h"
#include "localmem.h"
#include "gbFileOps.h"
#include "sqlUpdater.h"

static char const rcsid[] = "$Id: sqlDeleter.c,v 1.1 2003/06/03 01:27:47 markd Exp $";

#define DIRECT_MAX 50000  /* maximum number to directly delete */

static char* GB_DELETE_TMP = "gbDelete_tmp";
static char* createGbDeleteTmp = 
"CREATE TABLE gbDelete_tmp ("
"   acc varchar(20) not null primary key,"
"   unique(acc)"
")"; 


struct sqlDeleter* sqlDeleterNew(char* tmpDir, boolean verbEnabled)
/* create a new deleter object.  If tmpdir is NULL, this will aways
 * do direct deletes, rather than falling back to the join/copy on a
 * large number of deleted. */
{
struct sqlDeleter* sd;
AllocVar(sd);
sd->lm = lmInit(32*1024);
sd->verbose = verbEnabled;
if (tmpDir != NULL)
    {
    strcpy(sd->tmpDir, tmpDir);
    gbMakeDirs(tmpDir);
    }
sd->directMax = DIRECT_MAX;
return sd;
}

void sqlDeleterFree(struct sqlDeleter** sdPtr)
/* Free an object */
{
struct sqlDeleter* sd = *sdPtr;
if (sd != NULL)
    {
    sqlUpdaterFree(&sd->accLoader);
    lmCleanup(&sd->lm);
    freez(sdPtr);
    }
}

static void initLoader(struct sqlDeleter* sd)
/* setup loader and copy access to tab file */
{
struct slName* acc;
sd->accLoader = sqlUpdaterNew(GB_DELETE_TMP, sd->tmpDir, sd->verbose, NULL);

for (acc = sd->accs; acc != NULL; acc = acc->next)
    sqlUpdaterAddRow(sd->accLoader, "%s", acc->name);
sd->useDeleteJoin = TRUE;
sd->accs = NULL;  /* mark as not in use (list in localmem) */
}

void sqlDeleterAddAcc(struct sqlDeleter* sd, char* acc)
/* Add an accession to list to to delete. */
{
if (sd->deletesDone)
    errAbort("sqlDeleter: can't add accessions after a delete has been done");
/* always use direct if not tmp dir */
if ((sd->accCount < sd->directMax) || (sd->tmpDir[0] == '\0'))
    {
    struct slName* accRec = lmAlloc(sd->lm, sizeof(struct slName)+strlen(acc));
    strcpy(accRec->name, acc);
    slAddHead(&sd->accs, accRec);
    }
else
    {
    if (sd->accLoader == NULL)
        initLoader(sd);
    sqlUpdaterAddRow(sd->accLoader, "%s", acc);
    }
sd->accCount++;
}

static void deleteDirect(struct sqlDeleter* sd, struct sqlConnection *conn,
                         char* table, char* column)
/* delete by directly doing sql commands */
{
char query[256];
struct slName* acc;

for (acc = sd->accs; acc != NULL; acc = acc->next)
    {
    safef(query, sizeof(query), "DELETE QUICK FROM %s WHERE %s = '%s'",
          table, column, acc->name);
    sqlUpdate(conn, query);
    }
}

static void dupTableDef(struct sqlConnection *conn, char* table,
                        char* newTable)
/* create a duplicate of a table with the right indices. */
{
char query[2048], *createArgs;
struct sqlResult* sr;
char** row;

/* Returns two columns of table name and command */
safef(query, sizeof(query), "SHOW CREATE TABLE %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], table))
        break;
    }
if (row == NULL)
    errAbort("can't get create table command for: %s", table);
createArgs = strchr(row[1], '(');
if (createArgs == NULL)
    errAbort("don't understand returned create table command: %s", row[1]);
safef(query, sizeof(query), "CREATE TABLE %s %s", newTable, createArgs);
sqlFreeResult(&sr);
sqlUpdate(conn, query);
}

static void deleteJoin(struct sqlDeleter* sd, struct sqlConnection *conn,
                       char* table, char* column)
/* delete by creating a new table with a join */
{
char query[512], newTmpTable[64], oldTmpTable[64];
if (sd->accLoader != NULL)
    {
    /* build table, free to indicate it's completed */
    assert(!sd->deletesDone);
    sqlRemakeTable(conn, GB_DELETE_TMP, createGbDeleteTmp);
    sqlUpdaterCommit(sd->accLoader, conn);
    sqlUpdaterFree(&sd->accLoader);
    }
sd->deletesDone = TRUE;

/* remove existing tmp tables */
safef(newTmpTable, sizeof(newTmpTable), "%s_new_tmp", table);
safef(oldTmpTable, sizeof(oldTmpTable), "%s_old_tmp", table);
sqlDropTable(conn, newTmpTable);
sqlDropTable(conn, oldTmpTable);

dupTableDef(conn, table, newTmpTable);

/* do join into new table of entries not in accession table */
safef(query, sizeof(query),
      "INSERT INTO %s SELECT %s.* FROM %s LEFT JOIN %s "
      "ON (%s.%s = %s.acc) WHERE %s.acc IS NULL",
      newTmpTable, table, table, GB_DELETE_TMP, table, column,
      GB_DELETE_TMP, GB_DELETE_TMP);
sqlUpdate(conn, query);

/* Now swap the table into place */
safef(query, sizeof(query), "RENAME TABLE %s TO %s, %s TO %s",
      table, oldTmpTable, newTmpTable, table);
sqlUpdate(conn, query);
sqlDropTable(conn, oldTmpTable);
}

void sqlDeleterDel(struct sqlDeleter* sd, struct sqlConnection *conn,
                   char* table, char* column)
/* Delete row where column is in list. */
{
if ((sd->accCount > 0) && sqlTableExists(conn, table))
    {
    if (sd->verbose)
        gbVerbMsg(verbose, "deleting %d keys from %s", sd->accCount, table);
    if (sd->useDeleteJoin)
        deleteJoin(sd, conn, table, column);
    else
        deleteDirect(sd, conn, table, column);
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */



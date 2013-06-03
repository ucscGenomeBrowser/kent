#include "common.h"
#include "sqlDeleter.h"
#include "jksql.h"
#include "gbVerb.h"
#include "localmem.h"
#include "gbFileOps.h"
#include "sqlUpdater.h"
#include "gbSql.h"


/* FIXME: the point where copying the table is more efficient varies with
 * the size of the table.  It turned out that the 50000 row cutoff was
 * way to small for EST tables.  So for now, try just disabling this ugly
 * hack.
 */
#undef COPY_TO_DELETE_HACK

#ifdef COPY_TO_DELETE_HACK
#define DIRECT_MAX 50000  /* maximum number to directly delete */

static char* GB_DELETE_TMP = "gbDelete_tmp";
static char* createGbDeleteTmp = 
"NOSQLINJ CREATE TABLE gbDelete_tmp ("
"   acc varchar(20) not null primary key,"
"   unique(acc)"
")"; 
#endif

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
#ifdef COPY_TO_DELETE_HACK
sd->directMax = DIRECT_MAX;
#endif
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

#ifdef COPY_TO_DELETE_HACK
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
#endif

void sqlDeleterAddAcc(struct sqlDeleter* sd, char* acc)
/* Add an accession to list to to delete. */
{
if (sd->deletesDone)
    errAbort("sqlDeleter: can't add accessions after a delete has been done");
#ifdef COPY_TO_DELETE_HACK
/* always use direct if no tmp dir */
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
#else
{
struct slName* accRec = lmAlloc(sd->lm, sizeof(struct slName)+strlen(acc));
strcpy(accRec->name, acc);
slAddHead(&sd->accs, accRec);
}
#endif
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
    sqlSafef(query, sizeof(query), "DELETE QUICK FROM %s WHERE %s = '%s'",
          table, column, acc->name);
    sqlUpdate(conn, query);
    }
}

#ifdef COPY_TO_DELETE_HACK
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

gbSqlDupTableDef(conn, table, newTmpTable);

/* do join into new table of entries not in accession table */
sqlSafef(query, sizeof(query),
      "INSERT INTO %s SELECT %s.* FROM %s LEFT JOIN %s "
      "ON (%s.%s = %s.acc) WHERE %s.acc IS NULL",
      newTmpTable, table, table, GB_DELETE_TMP, table, column,
      GB_DELETE_TMP, GB_DELETE_TMP);
sqlUpdate(conn, query);

/* Now swap the table into place */
sqlSafef(query, sizeof(query), "RENAME TABLE %s TO %s, %s TO %s",
      table, oldTmpTable, newTmpTable, table);
sqlUpdate(conn, query);
sqlDropTable(conn, oldTmpTable);
}
#endif

void sqlDeleterDel(struct sqlDeleter* sd, struct sqlConnection *conn,
                   char* table, char* column)
/* Delete row where column is in list. */
{
if ((sd->accCount > 0) && sqlTableExists(conn, table))
    {
    if (sd->verbose)
        gbVerbMsg(gbVerbose, "deleting %d keys from %s", sd->accCount, table);
#ifdef COPY_TO_DELETE_HACK
    if (sd->useDeleteJoin)
        deleteJoin(sd, conn, table, column);
    else
#endif
        deleteDirect(sd, conn, table, column);
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */



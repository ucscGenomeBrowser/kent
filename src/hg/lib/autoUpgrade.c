/* autoUpgrade.c -- if possible, add a new column to an existing table.  If it fails,
 * try again every few minutes in case permissions are granted.
 * Originally written by Galt in cartDb.c for hgsid security improvement, copied here
 * and parameterized a bit by Angie for hgSession/wikiLink security improvement. */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "errCatch.h"
#include "jksql.h"
#include "obscure.h"
#include "portable.h"
#include "autoUpgrade.h"

// How often (in seconds) to wait before checking again
#define CHECKINTERVAL (3 * 60)

#define AUTOUPGRPATHSIZE 512

static void makeResultName(char *tableName, char *columnName, char *path)
/* Make trash file path for corresponding autoupgrade result file */
{
safef(path, AUTOUPGRPATHSIZE, "../trash/AUTO_UPGRADE_RESULT_%s_%s", tableName, columnName);
}

static boolean checkAutoUpgradeTableResultTimeIsOld(char *tableName, char *columnName)
/* Has enough time passed since the last upgrade check?
 * The idea is to only check once every few minutes
 * rather than each time the CGI runs. */
{
char path[AUTOUPGRPATHSIZE];
makeResultName(tableName, columnName, path);
if (!fileExists(path))
    return TRUE;  // If there is no result yet we should test and make one
time_t now = time(NULL);
time_t fMT = fileModTime(path);
double diff = difftime(now, fMT);
if (diff > CHECKINTERVAL)
    return TRUE;  // The result is old we should test it again in case situation changed
return FALSE;
}

static void writeAutoUpgradeTableResult(char *tableName, char *columnName, char *result)
/* Write table upgrade result */
{
char path[AUTOUPGRPATHSIZE];
makeResultName(tableName, columnName, path);
writeGulp(path, result, strlen(result));
}

void autoUpgradeTableAddColumn(struct sqlConnection *conn, char *tableName, char *columnName,
                               char *type, boolean notNull, char *defaultVal)
/* Try to upgrade the table by adding column in a safe way handling success, failures
 * and retries with multiple CGIs running.
 * type must be a valid SQL type string like "varchar(255)", "longblob", "tinyint" etc.
 * If notNull is TRUE then 'NOT NULL' will be added to the column definition.
 * defaultVal must be a valid SQL expression (quoted if necessary) for type, for example
 * "''" for a string type, "0.0" for float, or "NULL" if notNull is FALSE. */
{
boolean testAgain = checkAutoUpgradeTableResultTimeIsOld(tableName, columnName);
if (testAgain)
    {
    // Get the advisory lock for this table
    // This prevents multiple CGI processes from trying to upgrade simultaneously
    char lockName[512];
    safef(lockName, sizeof lockName, "AUTO_UPGRADE_%s_%s", tableName, columnName);
    sqlGetLock(conn, lockName);

    char result[4096];
    result[0] = '\0';
    // Make sure that the table has not been already upgraded by some earlier process.
    // We do not want to upgrade more than once.
    if (sqlFieldIndex(conn, tableName, columnName) == -1)
        {
        // Put a catch around the table upgrade attempt,
        // both to allow us to continue if it fails,
        // and to catch the error message and save it in the results file
        char query[1024];
        // Use NOSQLINJ here instead of using sqlSafef because sqlSafef aborts on strings
        // with parentheses like type "varchar(255)".  Caller is trusted to be internal code
        // not using external input.
        safef(query, sizeof query,
              NOSQLINJ "alter table %s add column %s %s %s default %s",
              tableName, columnName, type, (notNull ? "NOT NULL" : ""), defaultVal);
        struct errCatch *errCatch = errCatchNew();
        if (errCatchStart(errCatch))
            {
            sqlUpdate(conn, query);
            }
        errCatchEnd(errCatch);
        if (errCatch->gotError)
            {
            safef(result, sizeof result, "AUTOUPGRADE FAILED\n%s", errCatch->message->string);
            }
        else
            {
            safef(result, sizeof result, "OK\n");
            }
        errCatchFree(&errCatch);
        }
    writeAutoUpgradeTableResult(tableName, columnName, result);

    // Release the advisory lock for this table
    sqlReleaseLock(conn, lockName);
    }
}

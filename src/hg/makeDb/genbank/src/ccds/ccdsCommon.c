/* ccdsCommon - common functions used by CCDS programs */
#include "common.h"
#include "ccdsCommon.h"
#include "jksql.h"

void ccdsGetTblFileNames(char *tblFile, char *table, char *tabFile)
/* Get table and tab file name from a single input name.  If specified names
 * looks like a file name, use it as-is, otherwise generate a file name.
 * Output buffers should be PATH_LEN bytes long.  Either output argument maybe
 * be NULL. */
{
if ((strchr(tblFile, '/') != NULL) || (strchr(tblFile, '.') != NULL))
    {
    if (tabFile != NULL)
        safef(tabFile, PATH_LEN, "%s", tblFile);
    if (table != NULL)
        splitPath(tblFile, NULL, table, NULL);
    }
else
    {
    if (tabFile != NULL)
        {
        char *tmpDir = getenv("TMPDIR");
        if (tmpDir != NULL)
            safef(tabFile, PATH_LEN, "%s/%s.tab", tmpDir, tblFile);
        else
            safef(tabFile, PATH_LEN, "%s.tab", tblFile);
        }
    if (table != NULL)
        safef(table, PATH_LEN, "%s", tblFile);
    }
}

void ccdsRenameTable(struct sqlConnection *conn, char *oldTable, char *newTable)
/* rename a database table */
{
char query[2048];
safef(query, sizeof(query), "RENAME TABLE %s TO %s", oldTable, newTable);
sqlDropTable(conn, newTable);
sqlUpdate(conn, query);
}

struct hash *ccdsStatusValLoad(struct sqlConnection *conn)
/* load values from the imported ccdsStatusVals table.  Table hashes
 * status name to uid.  Names are loaded both as-is and lower-case */
{
struct hash *statusVals = hashNew(0);
struct sqlResult *sr = sqlGetResult(conn, 
                                    "SELECT ccds_status_val_uid, ccds_status FROM CcdsStatusVals");
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int uid = sqlSigned(row[0]);
    char *stat = row[1];
    hashAddInt(statusVals, stat, uid);
    tolowers(stat);
    hashAddInt(statusVals, stat, uid);
    }
sqlFreeResult(&sr);
return statusVals;
}

void ccdsStatusValCheck(struct hash *statusVals, char *stat)
/* check if a status value is valid, in a case-insensitive manner */
{
char statLower[64];
boolean isValid = (strlen(stat) < sizeof(statLower));
if (isValid)
    {
    strcpy(statLower, stat);
    tolowers(statLower);
    isValid = (hashLookup(statusVals, statLower) != NULL);
    }
if (!isValid)
    errAbort("unknown CCDS status: \"%s\"", stat);

}

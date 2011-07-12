/* makeTableList - create/recreate tableList. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "hgConfig.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeTableList - create/recreate tableList tables (cache of SHOW TABLES)\n"
  "usage:\n"
  "   makeTableList [assemblies]\n"
  "options:\n"
  "   -all               recreate tableList for all assemblies"
  );
}

static struct optionSpec options[] = {
    {"all", OPTION_BOOLEAN},
   {NULL, 0},
};

static void makeTableListConn(char *tableListName, struct sqlConnection *conn)
{
// recreate tableList in given db
// written so that tableList always exists (i.e. we use a temporary file and RENAME TABLE (which is hopefully atomic))

char *tmpDir = getenv("TMPDIR");
if(tmpDir == NULL)
    tmpDir = "/tmp";
char *tmp = cloneString(rTempName(tmpDir, "makeTableList", ".txt"));
struct sqlResult *sr;
char **row;
char buf[1024];
char tmpTable[512];
FILE *fd = mustOpen(tmp, "w");

sr = sqlGetResult(conn, "SHOW TABLES");
while ((row = sqlNextRow(sr)) != NULL)
    fprintf(fd, "%s\n", row[0]);
sqlFreeResult(&sr);
carefulClose(&fd);

// now load show tables data into a temporary table
safef(tmpTable, sizeof(tmpTable), "tableList%ld", (long) getpid());
sqlDropTable(conn, tmpTable);
safef(buf, sizeof(buf), "CREATE TABLE %s (name varchar(255) not null, PRIMARY KEY(name))", tmpTable);
sqlUpdate(conn, buf);
safef(buf, sizeof(buf), "LOAD DATA LOCAL INFILE '%s' INTO TABLE %s", tmp, tmpTable);
sqlUpdate(conn, buf);

if(sqlTableExists(conn, tableListName))
    {
    sqlDropTable(conn, "tableListOld");
    safef(buf, sizeof(buf), "RENAME TABLE tableList to tableListOld, %s TO %s", tmpTable, tableListName);
    sqlUpdate(conn, buf);
    sqlDropTable(conn, "tableListOld");
    }
else
    {
    safef(buf, sizeof(buf), "RENAME TABLE %s TO %s", tmpTable, tableListName);
    sqlUpdate(conn, buf);
    }
unlink(tmp);
}

void makeTableList(char *argv[], int argc, boolean all, char *tableListName)
/* makeTableList - create/recreate tableList. */
{
struct sqlResult *sr;
char **row;
struct slName *dbs = NULL;

if(all)
    {
    struct sqlConnection *conn = hConnectCentral();
    sr = sqlGetResult(conn, "select name from dbDb");
    while ((row = sqlNextRow(sr)) != NULL)
        slNameAddHead(&dbs, row[0]);
    sqlFreeResult(&sr);
    hDisconnectCentral(&conn);
    }
else
    {
    int i;
    for (i = 1; i < argc; i++)
        slNameAddHead(&dbs, argv[i]);
    }

for (; dbs != NULL; dbs = dbs->next)
    {
    // make sure assembly exists (on hgwdev some entries in hgcentraltest.dbDb do not exist).
    if(!all || sqlDatabaseExists(dbs->name))
        {
        struct sqlConnection *conn = sqlConnect(dbs->name);
        verbose(2, "db: %s\n", dbs->name);
        makeTableListConn(tableListName, conn);
        sqlDisconnect(&conn);
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
boolean all = optionExists("all");
if (argc < 2 && !all)
    usage();
char *tableListName = cfgOptionDefault("showTableCache", "tableList");
makeTableList(argv, argc, all, tableListName);
return 0;
}

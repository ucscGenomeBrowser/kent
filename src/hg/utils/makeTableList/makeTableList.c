/* makeTableList - create/recreate tableList. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"

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

static void makeTableListConn(struct sqlConnection *conn)
{
// recreate tableList in given db
// written so that tableList always exists (i.e. we use a temporary file and rename (which is hopefully atomic))

char *tmp = cloneString(rTempName("/tmp", "makeTableList", ".txt"));
struct sqlResult *sr;
char **row;
char buf[1024];
char tmpTable[512];
FILE *fd = fopen(tmp, "w");
if(fd == NULL)
    errAbort("Couldn't create tmp file");

sr = sqlGetResult(conn, "SHOW TABLES");
while ((row = sqlNextRow(sr)) != NULL)
    fprintf(fd, "%s\n", row[0]);
sqlFreeResult(&sr);
fclose(fd);

// now load show tables data into a temporary table
safef(tmpTable, sizeof(tmpTable), "tableList%ld", (long) getpid());
safef(buf, sizeof(buf), "CREATE TABLE %s (name varchar(255) not null, PRIMARY KEY(name))", tmpTable);
sqlUpdate(conn, buf);
safef(buf, sizeof(buf), "LOAD DATA LOCAL INFILE '%s' INTO TABLE %s", tmp, tmpTable);
sqlUpdate(conn, buf);

if(sqlTableExists(conn, "tableList"))
    {
    safef(buf, sizeof(buf), "RENAME TABLE tableList to tableListOld, %s TO tableList", tmpTable);
    sqlUpdate(conn, buf);
    sqlUpdate(conn, "DROP TABLE tableListOld");
    }
else
    {
    safef(buf, sizeof(buf), "RENAME TABLE %s TO tableList", tmpTable);
    sqlUpdate(conn, buf);
    }
unlink(tmp);
}

void makeTableList(char *argv[], int argc, boolean all)
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
        makeTableListConn(conn);
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
makeTableList(argv, argc, all);
return 0;
}

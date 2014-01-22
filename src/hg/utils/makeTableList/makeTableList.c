/* makeTableList - create/recreate tableList. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "hgConfig.h"
#include "jksql.h"

static char *host = NULL;
static char *user = NULL;
static char *password = NULL;
static char *toProf = NULL;
static char *bigFiles = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeTableList - create/recreate tableList tables (cache of SHOW TABLES)\n"
  "usage:\n"
  "   makeTableList [assemblies]\n"
  "options:\n"
  "   -host               show tables: mysql host\n"
  "   -password           show tables: mysql password\n"
  "   -user               show tables: mysql user\n"
  "   -toProf             optional: mysql profile to write table list to\n"
  "   -all                recreate tableList for all assemblies\n"
  "   -bigFiles <fname>   create file with tuples (db, track, name of bigfile)\n"
  );
}

static struct optionSpec options[] = {
    {"all", OPTION_BOOLEAN},
    {"host", OPTION_STRING},
    {"password", OPTION_STRING},
    {"user", OPTION_STRING},
    {"toProf", OPTION_STRING},
    {"bigFiles", OPTION_STRING},
   {NULL, 0},
};

static void makeTableListConn(char *tableListName, struct sqlConnection *conn, struct sqlConnection *targetConn)
{
// recreate tableList in given db
// written so that tableList always exists (i.e. we use a temporary file and RENAME TABLE (which is hopefully atomic))
// targetConn can be NULL

char *tmpDir = getenv("TMPDIR");
if(tmpDir == NULL)
    tmpDir = "/tmp";
char *tmp = cloneString(rTempName(tmpDir, "makeTableList", ".txt"));
struct sqlResult *sr;
char **row;
char buf[1024];
char tmpTable[512];
FILE *fd = mustOpen(tmp, "w");
FILE *bigFd = NULL;
if (bigFiles)
    bigFd = mustOpen(bigFiles, "w");

// iterate over all tables and execute "describe" for them
// write to tab sep file, prefixed with table name
printf("running DESCRIBE on all tables. Depending on the latency to the mysql server, this can take a while...\n");
struct slName *allTables =  sqlListTables(conn);
struct slName *tbl;
char *sep;
for (tbl = allTables;  tbl != NULL;  tbl = tbl->next)
    {
    char query[256];
    sqlSafef(query, sizeof(query), "describe %s", tbl->name);
    sr = sqlGetResult(conn, query);
    int numCols = sqlCountColumns(sr);
    int numRows = 0;
    bool hasFileCol = FALSE;
    while ((row = sqlNextRow(sr)) != NULL)
        {
        if (sameString(row[0], "fileName"))
            hasFileCol = TRUE;
        fprintf(fd,"%s\t",tbl->name);
        sep= "";
        int c;
        for (c=0;c<numCols;++c)
            {
            char* str = row[c];
            if (str==NULL)
                str = "";
            fprintf(fd,"%s%s",sep,str);
            sep = "\t";
            }
        fprintf(fd,"\n");
        numRows++;
        }
    sqlFreeResult(&sr);

    // write fileName column to big file index
    if (bigFd!=NULL && hasFileCol && numRows==1)
        {
        //char *db = sqlGetDatabase(conn);
        sqlSafef(query, sizeof(query), "select * from %s", tbl->name);
        sr = sqlGetResult(conn, query);
        while ((row = sqlNextRow(sr)) != NULL)
            fprintf(bigFd,"%s\t%s\n",tbl->name, row[0]);
        }
    }

carefulClose(&fd);
if (bigFd)
    {
    printf("big file names written to %s\n", bigFiles);
    carefulClose(&bigFd);
    }
//printf("table data written to %s\n", tmp);

if (targetConn!=NULL)
    {
    sqlDisconnect(&conn);
    conn = targetConn;
    verbose(2, "using target connection \n");
    }

// now load show tables data into a temporary table
safef(tmpTable, sizeof(tmpTable), "tableList%ld", (long) getpid());
sqlDropTable(conn, tmpTable);
// Field    | Type             | Null | Key | Default | Extra
sqlSafef(buf, sizeof(buf), "CREATE TABLE %s (tableName varchar(255) not null, field varchar(255), "
    "type varchar(255), nullAllowed varchar(255), isKey varchar(255), hasDefault varchar(255) NULL, extra varchar(255),"
    "index tableIdx(tableName))", tmpTable);
sqlUpdate(conn, buf);
sqlSafef(buf, sizeof(buf), "LOAD DATA LOCAL INFILE '%s' INTO TABLE %s", tmp, tmpTable);
sqlUpdate(conn, buf);

if(sqlTableExists(conn, tableListName))
    {
    sqlDropTable(conn, "tableListOld");
    sqlSafef(buf, sizeof(buf), "RENAME TABLE %s to tableListOld, %s TO %s", tableListName, tmpTable, tableListName);
    sqlUpdate(conn, buf);
    sqlDropTable(conn, "tableListOld");
    }
else
    {
    sqlSafef(buf, sizeof(buf), "RENAME TABLE %s TO %s", tmpTable, tableListName);
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
    sr = sqlGetResult(conn, "NOSQLINJ select name from dbDb");
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
        struct sqlConnection *conn;
        if(host)
            conn = sqlConnectRemote(host, user, password, dbs->name);
        else
            conn = sqlConnect(dbs->name);
        verbose(2, "db: %s\n", dbs->name);

        struct sqlConnection *targetConn = NULL;
        if (isNotEmpty(toProf))
            {
            targetConn = sqlConnectProfile(toProf, dbs->name);
            verbose(2, "target db profile: %s\n", toProf);
            }
            
        makeTableListConn(tableListName, conn, targetConn);
        sqlDisconnect(&conn);
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
boolean all = optionExists("all");
host = optionVal("host", NULL);
user = optionVal("user", NULL);
password = optionVal("password", NULL);
toProf = optionVal("toProf", NULL);
bigFiles = optionVal("bigFiles", NULL);
if (argc < 2 && !all)
    usage();
char *tableListName = cfgOptionDefault("showTableCache", "tableList");
makeTableList(argv, argc, all, tableListName);
return 0;
}

/* makeTableList - create/recreate tableList. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
static char *toHost = NULL;
static char *toUser = NULL;
static char *toPassword = NULL;
static char *hgcentral = NULL;
boolean doBigFiles = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeTableList - create/recreate tableList tables (cache of SHOW TABLES and DESCRIBE)\n"
  "usage:\n"
  "   makeTableList [assemblies]\n"
  "options:\n"
  "   -host               show tables: mysql host\n"
  "   -user               show tables: mysql user\n"
  "   -password           show tables: mysql password\n"
  "   -toProf             optional: mysql profile to write table list to (target server)\n"
  "   -toHost             alternative to toProf: mysql target host\n"
  "   -toUser             alternative to toProf: mysql target user\n"
  "   -toPassword         alternative to toProf: mysql target pwd\n"
  "   -hgcentral          specify an alternative hgcentral db name when using -all\n"
  "   -all                recreate tableList for all active assemblies in hg.conf's hgcentral\n"
  "   -bigFiles           create table with tuples (track, name of bigfile)\n");
}

static struct optionSpec options[] = {
    {"all", OPTION_BOOLEAN},
    {"host", OPTION_STRING},
    {"password", OPTION_STRING},
    {"user", OPTION_STRING},
    {"toProf", OPTION_STRING},
    {"toHost", OPTION_STRING},
    {"toPassword", OPTION_STRING},
    {"toUser", OPTION_STRING},
    {"hgcentral", OPTION_STRING},
    {"bigFiles", OPTION_BOOLEAN},
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

char *bigFileTmp = NULL;
FILE *bigFd = NULL;
if (doBigFiles)
    {
    bigFileTmp = cloneString(rTempName(tmpDir, "bigFiles", ".txt"));
    bigFd = mustOpen(bigFileTmp, "w");
    }

struct sqlResult *sr;
char **row;
char buf[1024];
char tmpTable[512];
FILE *fd = mustOpen(tmp, "w");
// iterate over all tables and execute "describe" for them
// write to tab sep file, prefixed with table name
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

    // only output bigfile name if we have found a "fileName" column and the table has 1 row
    if (bigFd && hasFileCol && numRows==1)
        {
        sqlSafef(query, sizeof(query), "select * from %s", tbl->name);
        sr = sqlGetResult(conn, query);
        while ((row = sqlNextRow(sr)) != NULL)
            fprintf(bigFd,"%s\t%s\n",tbl->name, row[0]);
        sqlFreeResult(&sr);
        }
    }

carefulClose(&fd);
carefulClose(&bigFd);

if (targetConn!=NULL)
    {
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

if (bigFileTmp)
    {
    // load bigFiles directly into a table
    sqlDropTable(conn, "bigFiles");
    sqlSafef(buf, sizeof(buf), "CREATE TABLE bigFiles (tableName varchar(255) not null, fileName varchar(255), "
        "index tableIdx(tableName))");
    sqlUpdate(conn, buf);
    sqlSafef(buf, sizeof(buf), "LOAD DATA LOCAL INFILE '%s' INTO TABLE bigFiles", bigFileTmp);
    sqlUpdate(conn, buf);
    unlink(bigFileTmp);
    }

}

void makeTableList(char *argv[], int argc, boolean all, char *tableListName)
/* makeTableList - create/recreate tableList. */
{
struct sqlResult *sr;
char **row;
struct slName *dbs = NULL;
printf("This tool is running DESCRIBE on all tables. \n"
    "Depending on the latency to the mysql server, this can take a while.\n");

if(all)
    {
    struct sqlConnection *conn;
    if (hgcentral != NULL)
        conn = sqlConnectRemote(host, user, password, hgcentral);
    else
        conn = hConnectCentral();

    sr = sqlGetResult(conn, NOSQLINJ "select name from dbDb where active=1");
    while ((row = sqlNextRow(sr)) != NULL)
        {
        printf("Found db %s\n", row[0]);
        slNameAddHead(&dbs, cloneString(row[0]));
        }

    sqlFreeResult(&sr);

    slNameAddHead(&dbs, cloneString("hgFixed"));
    slNameAddHead(&dbs, cloneString("go"));
    slNameAddHead(&dbs, cloneString("uniProt"));
    slNameAddHead(&dbs, cloneString("visiGene"));
    slNameAddHead(&dbs, cloneString("proteome"));

    }
else
    {
    int i;
    for (i = 1; i < argc; i++)
        slNameAddHead(&dbs, argv[i]);
    }


struct sqlConnection *conn;

// just check that the username/password actually works
if (host!=NULL)
    {
    printf("checking mysql credentials from command line\n");
    conn = sqlConnectRemote(host, user, password, NULL);
    sqlDisconnect(&conn);
    }

for (; dbs != NULL; dbs = dbs->next)
    {
    // open connect to source db
    printf("DB: %s\n", dbs->name);
    if(host)
        conn = sqlMayConnectRemote(host, user, password, dbs->name);
    else
        conn = sqlMayConnect(dbs->name);
    verbose(2, "source db: %s\n", dbs->name);
    // make sure assembly exists (on hgwdev some entries in hgcentraltest.dbDb do not exist).
    if(conn==NULL)
        {
        printf("db does not exist on source mysql server\n");
        continue;
        }

    struct sqlConnection *targetConn = conn;
    // open connection to target db
    if (isNotEmpty(toProf))
        {
        targetConn = sqlMayConnectProfile(toProf, dbs->name);
        verbose(2, "target db profile: %s\n", toProf);
        }
    else if (isNotEmpty(toHost) && isNotEmpty(toUser) && isNotEmpty(toPassword))
        {
        targetConn = sqlMayConnectRemote(toHost, toUser, toPassword, dbs->name);
        verbose(2, "target db host, user, password: %s, %s, %s\n", toHost, toUser, toPassword);
        }
            
    // make sure assembly exists (on hgwdev some entries in hgcentraltest.dbDb do not exist).
    if(targetConn==NULL)
        {
        printf("db does not exist on target\n");
        continue;
        }

    makeTableListConn(tableListName, conn, targetConn);
    sqlDisconnect(&conn);
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
toHost = optionVal("toHost", NULL);
toUser = optionVal("toUser", NULL);
toPassword = optionVal("toPassword", NULL);
hgcentral = optionVal("hgcentral", NULL);
doBigFiles = optionExists("bigFiles");
if (argc < 2 && !all)
    usage();
char *tableListName = cfgOptionDefault("showTableCache", "tableList");
makeTableList(argv, argc, all, tableListName);
return 0;
}

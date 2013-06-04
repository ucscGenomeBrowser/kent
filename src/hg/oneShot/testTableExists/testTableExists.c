/* testTableExists - Experiments with ways to test for table existence ... */

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "options.h"
#include "jksql.h"
#include "hgConfig.h"
#include "obscure.h"
#include "portable.h"


char *database = NULL;
char *host     = NULL;
char *user     = NULL;
char *password = NULL;

struct sqlConnection *conn = NULL;

char *existingMyIsamDb = NULL;
char *existingMyIsamTable = NULL;
char *existingInnodbDb = NULL;
char *existingInnodbTable = NULL;
char *nonExistingTableDb = NULL;
char *nonExistingTable = NULL;

int numReps=1000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testTableExists - Experiments with various ways to test for table existence.\n"
  "usage:\n"
  "   testTableExists loginProfile existingMyIsamTable existingInnodbTable nonExistingTable\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void testSqlTableExists(struct sqlConnection *sc, char *sql, char *table, int *resultRows, unsigned int *errorNo)
/* Use sql to test if a table exists. Return number of rows in resultset and whether it had an error. */
{
char query[256];
struct sqlResult *sr;
int count=0;
sqlSafef(query, sizeof(query), sql, table);
sr=sqlGetResultExt(sc,query, errorNo, NULL);
if (sr)
    {
    while (sqlNextRow(sr))
	count++;
    sqlFreeResult(&sr);
    }
*resultRows=count;
}

void useDb(char *db)
/* Use db changes default db. */
{
char query[256];
sqlSafef(query, sizeof(query), "use %s", db);
sqlUpdate(conn,query);
}


char *getCfgOption(char *config, char *setting)
/* get setting for specified config */
{
char temp[256];
safef(temp, sizeof(temp), "%s.%s", config, setting);
char *value = cfgOption(temp);
if (!value)
    errAbort("setting %s not found!",temp);
return value;
}

void testTableExistsRepeatedly(char *sql, int kind)
/* testTableExistsRepeatedly - Experiments with ways to test for table existence ... */
{
int i=0;

int rows=0;
unsigned int errorNo=0;

//verbose(1, "got here 2 %s\n", existingMyIsamDb); //debug

verbose(1, "\nQuery: [%s]\n", sql); 

useDb(existingMyIsamDb);

//verbose(1, "got here 3\n"); //debug

long time = clock1000();

for(i=0; i<numReps; ++i)
    {
    //verbose(1, "got here 3.1 [%s] %s\n", sql, existingMyIsamTable); //debug
    testSqlTableExists(conn, sql, existingMyIsamTable, &rows, &errorNo);
    //verbose(1, "got here 3.2  kind=%d rows=%d\n", kind, rows); fflush(stderr); //debug
    if ((kind==0 && errorNo!=0) || (kind==1 && rows==0))
	errAbort("Unexpected error: table should exist: %s", existingMyIsamTable);
    //verbose(1, "got here 3.3\n"); //debug
    }
verbose(1, "MyISAM time: %.2f seconds\n", (clock1000() - time) / 1000.0);

//verbose(1, "got here 4\n"); //debug

if (!sameString(existingInnodbDb, "NULL"))
{
useDb(existingInnodbDb);
time = clock1000();
for(i=0; i<numReps; ++i)
    {
    testSqlTableExists(conn, sql, existingInnodbTable, &rows, &errorNo);
    if ((kind==0 && errorNo!=0) || (kind==1 && rows==0))
	errAbort("Unexpected error: innodb table should exist: %s", existingInnodbTable);
    }
verbose(1, "InnoDb time: %.2f seconds\n", (clock1000() - time) / 1000.0);
}
//verbose(1, "got here 5\n"); //debug

useDb(nonExistingTableDb);
time = clock1000();
for(i=0; i<numReps; ++i)
    {
    testSqlTableExists(conn, sql, nonExistingTable, &rows, &errorNo);
    if ((kind==0 && errorNo==0) || (kind==1 && rows!=0))
	errAbort("Unexpected success: table not existing should not exist: %s", nonExistingTable);
    }
verbose(1, "notExist time: %.2f seconds\n", (clock1000() - time) / 1000.0);


//verbose(1, "got here 5\n"); //debug
}

void testTableExists(char *config)
/* testTableExists - Experiments with ways to test for table existence ... */
{

/* get connection info */
database = "hg18";
host     = getCfgOption(config, "host"    );
user     = getCfgOption(config, "user"    );
password = getCfgOption(config, "password");

verbose(1, "running on %s\n", getenv("HOST"));

verbose(1, "connecting to host %s\n", host);

conn = sqlConnectRemote(host, user, password, database);

verbose(1, "sqlVersion=%s\n", sqlVersion(conn));

verbose(1, "numReps=%d\n", numReps);

verbose(1, "MyISAM exists: %s.%s\n", existingMyIsamDb, existingMyIsamTable); 
verbose(1, "InnoDb exists: %s.%s\n", existingInnodbDb, existingInnodbTable); 
verbose(1, "does not exist: %s.%s\n", nonExistingTableDb, nonExistingTable); 

//verbose(1, "got here 1\n");

// kinds:
// 0 means not exists when hadError==TRUE
// 1 means not exists when rowCount==0

//good 
testTableExistsRepeatedly("select count(*) from %s", 0);

//good 
testTableExistsRepeatedly("describe %s", 0);

//testTableExistsRepeatedly("CHECK TABLE %s FAST QUICK;");
  //need to grep for "doesn't exist" in all output fields.

//good 
testTableExistsRepeatedly("SELECT 1 FROM %s LIMIT 0", 0);

//good 
//testTableExistsRepeatedly("show tables like '%s'", 1);
verbose(1, "\nskipping this too slow: show tables like '%%s'\n");
  // need to check if output rowcount is 1 or 0

//testTableExistsRepeatedly("SELECT COUNT(*) FROM information_schema.tables  WHERE table_schema = 'hg18' AND table_name = '%s'");
  // need to check if output rowcount is 1 or 0
  // need to patch in the bloody database too
  // only works on mysql 5

//good 
testTableExistsRepeatedly("show columns from %s", 0);

//flush table DOES NOT WORK!: it doesn't return any failure at all: testTableExistsRepeatedly("flush table %s", 0);

//testTableExistsRepeatedly("analyze table %s");
  //need to grep for "doesn't exist" in all output fields.

//good 
testTableExistsRepeatedly("show index from %s", 0);

//good 
testTableExistsRepeatedly("explain SELECT 1 FROM %s LIMIT 0", 0);

//good 
//testTableExistsRepeatedly("show table status like '%s'", 1);
verbose(1, "\nskipping this too slow: show table status like '%%s'\n");
  // need to check if output rowcount is 0
 
//verbose(1, "got here 99\n");

sqlDisconnect(&conn);

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 8)
    usage();

existingMyIsamDb    = argv[2];
existingMyIsamTable = argv[3];

existingInnodbDb    = argv[4];
existingInnodbTable = argv[5];

nonExistingTableDb  = argv[6];
nonExistingTable    = argv[7];

testTableExists(argv[1]);

return 0;
}

/* safePush - Push database tables from one machine to another. This is a 
 * little more careful than mypush.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "jksql.h"
#include "hash.h"
#include "options.h"
#include "hgConfig.h"


/* Command line flags - all default to false. */
boolean oldOk, chrom, prefix, allTables, test;

char *dev32 = "hgwdevold";	/* Old dev machine. */
char *dev64 = "hgwdev";		/* New dev machine. */

char *mysqlDataDir = "/var/lib/mysql";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "toDev64 - A program that copies data from the old hgwdev database to the\n"
  "new hgwdev database.\n"
  "usage:\n"
  "   toDev64 database table\n"
  "options:\n"
  "   -chrom - if set will copy tables split by chromosome\n"
  "   -prefix - if set will copy all tables that start with 'table'\n"
  "   -oldOk - if set will let you overwrite a new table on the dest with\n"
  "            an older source table\n"
  "   -allTables - if set will copy all tables in database. The table\n"
  "                table parameter should be 'all' in this case\n"
  "   -test - if set it not will actually do the copy. (It just\n"
  "             prints the commands it will execute.)\n"
  );
}

char *user, *password;	/* Database user and password. */

static struct optionSpec options[] = {
   {"oldOk", OPTION_BOOLEAN},
   {"chrom", OPTION_BOOLEAN},
   {"prefix", OPTION_BOOLEAN},
   {"allTables", OPTION_BOOLEAN},
   {"test", OPTION_BOOLEAN},
   {NULL, 0},
};

boolean isAllGoodChars(char *s)
/* Return TRUE if all characters are alphanumeric or _ */
{
char c;
while ((c = *s++) != 0)
    if (!isalnum(c) && c != '_')
         return FALSE;
return TRUE;
}

void forceAllGoodChars(char *s)
/* Make sure all chars are good or abort */
{
if (!isAllGoodChars(s))
    errAbort("Illegal characters in %s, only letters, numbers, and _ allowed",
    	s);
}

void execAndCheck(char *command)
/* Do a system on command, and abort with error message if
 * it fails */
{
puts(command);
if (!test)
    {
    int err = system(command);
    if (err != 0)
        errAbort("system call failed error code %d", err);
    }
}

boolean is5(char *host)
/* Return TRUE if we seem to be version 5.0 of mysql or later */
{
struct sqlConnection *conn = sqlConnectRemote(host, user, password, NULL);
int version = sqlMajorVersion(conn);
sqlDisconnect(&conn);
verbose(2, "%s is version %d\n", host, version);
return (version >= 5);
}

void flushTables(char *host)
/* Issue mysqladmin flush command. */
{
char command[2048];
safef(command, sizeof(command),
	"mysqladmin --user=hguser --password=hguserstuff flush-tables");
execAndCheck(command);
}

void pushAllTables(char *sourceHost, char *destHost, char *database)
/* Do the big rsync on database from sourceHost to destHost */
{
char command[2048];

if (sqlDatabaseExists(database))
    errAbort("Database %s already exists", database);
safef(command, sizeof(command),
	"rsync -avr --progress --rsh=rsh %s:%s/%s %s/",
	sourceHost, mysqlDataDir, database, mysqlDataDir);
execAndCheck(command);
}

struct hash *tableTimeHash(char *host, boolean is5, char *database, char *tablePattern)
/* Get hash of tables and the time they were last updated. */
{
struct hash *hash = hashNew(0);
struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);
char query[512];
struct sqlResult *sr;
char **row;
sqlSafef(query, sizeof(query), "show table status like '%s'", tablePattern);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *table = row[0];
    char *time = (is5 ? row[12] : row[11]);
    verbose(3, "%s:%s.%s %s\n", host, database, table, time);
    hashAdd(hash, table, cloneString(time));
    }
sqlDisconnect(&conn);
return hash;
}

void pushTable(char *sourceHost, struct hash *sourceTimes,
	       char *destHost, struct hash *destTimes,
	       char *database, char *table, boolean missingTableOk)
/* Push a single table */
{
char command[2048];
if (!oldOk)
    {
    char *sTime = hashFindVal(sourceTimes, table);
    char *dTime = hashFindVal(destTimes, table);
    if (sTime == NULL)
         {
	 if (missingTableOk)
	     return;
	 else
	     errAbort("Table %s missing in %s:%s", table, sourceHost, database);
	 }
    if (dTime != NULL)
         {
	 int s = sqlDateToUnixTime(sTime);
	 int d = sqlDateToUnixTime(dTime);
	 verbose(2, "%s.%s source time %s (%d), dest time %s (%d)\n", 
	 	database, table, sTime, s, dTime, d);
	 if (d > s)
	     errAbort("%s.%s is newer on %s than on %s", database, table,
	     	destHost, sourceHost);
	 if (d == s)
	     {
	     warn("%s.%s has same timestamp on %s and %s. Skipping...",
	         database, table, destHost, sourceHost);
	     return;
	     }
	 }
    }
safef(command, sizeof(command),
    "rsync -av --progress --rsh=rsh %s:%s/%s/%s.* %s/%s/",
    sourceHost, mysqlDataDir, database, table, mysqlDataDir, database);
execAndCheck(command);
}

struct slName *tablesLike(char *host, char *database, char *pattern)
/* Get list of tables matching SQL wildcard pattern */
{
struct slName *list = NULL;
struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);
char query[512];
struct sqlResult *sr;
char **row;
sqlSafef(query, sizeof(query), "show tables like '%s'", pattern);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    slNameAddHead(&list, row[0]);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
slReverse(&list);
return list;
}

void pushSomeTables(char *sourceHost, boolean sourceIs5,
	char *destHost, boolean destIs5, char *database, char *table,
	boolean missingTableOk)
/* Push selected tables in database. */
{
struct hash *sourceTimes = NULL, *destTimes = NULL;
struct slName *tableList = NULL, *tableEl;
char pattern[512];
if (chrom)
    {
    if (prefix)
	safef(pattern, sizeof(pattern), "chr%%\\_%s%%", table);
    else
	safef(pattern, sizeof(pattern), "chr%%\\_%s", table);
    tableList = tablesLike(sourceHost, database, pattern);
    }
else if (prefix)
    {
    safef(pattern, sizeof(pattern), "%s%%", table);
    tableList = tablesLike(sourceHost, database, pattern);
    }
else
    {
    safef(pattern, sizeof(pattern), "%s", table);
    tableList = slNameNew(table);
    }
if (!oldOk)
    {
    sourceTimes = tableTimeHash(sourceHost, sourceIs5, database, pattern);
    destTimes = tableTimeHash(destHost, destIs5, database, pattern);
    }
for (tableEl = tableList; tableEl != NULL; tableEl = tableEl->next)
    pushTable(sourceHost, sourceTimes, destHost, destTimes, database, 
    	tableEl->name, missingTableOk);
}

void pushDatabase(char *sourceHost, boolean sourceIs5,
	char *destHost, boolean destIs5, char *database, char *table,
	boolean missingTableOk)
/* Push table from single database.  Possibly push all tables. */
{
if (allTables)
    pushAllTables(sourceHost, destHost, database);
else
    pushSomeTables(sourceHost, sourceIs5, destHost, destIs5, database,
    	table, missingTableOk);
}

void pushToHost(char *sourceHost, 
	char *database, char *table, char *destHost)
/* Do push of database and table from source to one host */
{
boolean sourceIs5 = is5(sourceHost);
boolean destIs5 = is5(destHost);

if (sourceIs5 || destIs5)
    errAbort("Sorry, can't yet handle mySQL 5");
if (sourceIs5 && !destIs5)
    errAbort("%s is running Mysql pre-5, and %s is running MySQL post-5, sorry",
    	destHost, sourceHost);
forceAllGoodChars(sourceHost);
forceAllGoodChars(destHost);
forceAllGoodChars(database);
forceAllGoodChars(table);
pushDatabase(sourceHost, sourceIs5, destHost, destIs5, database, table,
    TRUE);
flushTables(destHost);
}


void toDev64(char *database, char *table)
/* Push from old hgwdev to new hgwdev */
{
char *envHost = getenv("HOST");

/* Various error checks */
if (envHost == NULL)
    errAbort("Couldn't find HOST environment variable, sorry.");
if (!sameString(envHost, dev64))
    errAbort("You can only run this machine from %s", dev64);
if (allTables && !sameString(table, "all"))
    errAbort("Expecting 'all' for table with -allTables");
if (sameWord(table, "mysql"))
    errAbort("Please contact cluster-admin to update 'mysql' database");
if (prefix && strlen(table) < 3)
    errAbort("You can only use -prefix if have at least 3 letters in table name.");
pushToHost(dev32, database, table, dev64);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
oldOk = optionExists("oldOk");
chrom = optionExists("chrom");
prefix = optionExists("prefix");
allTables = optionExists("allTables");
test = optionExists("test");
user = "hguser";
password = "hguserstuff";

toDev64(argv[1], argv[2]);
return 0;
}

/* safePush - Push database tables from one machine to another. This is a 
 * little more careful than mypush.. */
#include "common.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "jksql.h"
#include "hash.h"
#include "options.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: safePush.c,v 1.8 2008/09/03 19:21:32 markd Exp $";

/* Command line flags - all default to false. */
boolean oldOk, chrom, prefix, allTables, allDatabases, test;

/* Command line string options - null by default. */
char *list, *singleOp;

/* List of all machines on round robin. */
char *roundRobin[] = 
	{"hgw1", "hgw2", "hgw3", "hgw4", "hgw5", "hgw6", "hgw7", "hgw8",};

char *alpha = "hgwdev";		/* Alpha test machine. */
char *beta = "hgwbeta";		/* Beta test machine. */

char *mysqlDataDir = "/var/lib/mysql";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "safePush - Push database tables from one machine to another. This is a\n"
  "little more careful than mypush.  It should be run on the machine that is\n"
  "the source of the data\n"
  "usage:\n"
  "   safePush database table destType\n"
  "Where destType is either:\n"
  "     beta - to push from hgwdev to hgbeta\n"
  "     hgw1 - to push from hgwbeta to hgw1\n"
  "     rr - to push from hgwbeta to the round robin\n"
  "     list - to push from hgwbeta to the list of machines, used with -list\n"
  "     single - to push from current machine to any machine, used with -single\n"
  "options:\n"
  "   -oldOk - if set will let you overwrite a new table on the dest with\n"
  "            an older source table\n"
  "   -chrom - if set will copy tables split by chromosome\n"
  "   -prefix - if set will treat table as a prefix, and copy all tables\n"
  "             starting with that prefix\n"
  "   -allTables - if set will copy all tables in database. The table\n"
  "                table parameter should be 'all' in this case\n"
  "   -allDatabases - if set will copy table in all databases. The database\n"
  "                parameter should be 'all in this case\n"
  "   -test - just print the commands that this would do, don't execute them.\n"
  "   -list=fileName - specify file name containing list of dest machines\n"
  "   -single=host - specify single destination machine"
  );
}

char *user, *password;	/* Database user and password. */

static struct optionSpec options[] = {
   {"oldOk", OPTION_BOOLEAN},
   {"chrom", OPTION_BOOLEAN},
   {"prefix", OPTION_BOOLEAN},
   {"allTables", OPTION_BOOLEAN},
   {"allDatabases", OPTION_BOOLEAN},
   {"test", OPTION_BOOLEAN},
   {"list", OPTION_STRING},
   {"single", OPTION_STRING},
   {NULL, 0},
};

struct hash *getMysqlVars(char *host)
/* Return hash full of MYSQL variables. */
{
struct hash *hash = hashNew(0);
struct sqlConnection *conn = sqlConnectRemote(host, user, password, "mysql");
struct sqlResult *sr = sqlGetResult(conn, "show variables");
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    hashAdd(hash, row[0], cloneString(row[1]));
sqlFreeResult(&sr);
sqlDisconnect(&conn);
verbose(2, "Got %d mysqlVars from %s\n", hash->elCount, host);
return hash;
}

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

boolean is5(struct hash *varHash)
/* Return TRUE if we seem to be version 5.0 of mysql or later */
{
char *ver = hashMustFindVal(varHash, "version");
int version = atoi(ver);
verbose(2, "Version %s\n", ver);
return (version >= 5);
}

void flushTables(char *host)
/* Issue mysqladmin flush command. */
{
char command[2048];
safef(command, sizeof(command),
	"rsh %s mysqladmin --user=hguser --password=hguserstuff flush-tables",
	host);
execAndCheck(command);
}

void pushAllTables(char *sourceHost, char *destHost, char *database)
/* Do the big rsync on database from sourceHost to destHost */
{
char command[2048];
safef(command, sizeof(command),
	"rsync -avr --progress --rsh=rsh %s/%s %s:%s/",
	mysqlDataDir, database, destHost, mysqlDataDir);
execAndCheck(command);
}

struct hash *tableTimeHash(char *host, boolean is5, char *database)
/* Get hash of tables and the time they were last updated. */
{
struct hash *hash = hashNew(0);
struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);
struct sqlResult *sr = sqlGetResult(conn, "show table status");
char **row;
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
	 }
    }
safef(command, sizeof(command),
    "rsync -av --progress --rsh=rsh %s/%s/%s.* %s:/%s/%s/",
    mysqlDataDir, database, table, destHost, mysqlDataDir, database);
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
safef(query, sizeof(query), "show tables like '%s'", pattern);
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
if (!oldOk)
    {
    sourceTimes = tableTimeHash(sourceHost, sourceIs5, database);
    destTimes = tableTimeHash(destHost, destIs5, database);
    }
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
    tableList = slNameNew(table);
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

void pushToHost(char *sourceHost, struct hash *sourceVars,
	char *database, char *table, char *destHost)
/* Do push of database and table from source to one host */
{
struct hash *destVars = getMysqlVars(destHost);
boolean sourceIs5 = is5(sourceVars);
boolean destIs5 = is5(destVars);

if (sourceIs5 || destIs5)
    errAbort("Sorry, can't yet handle mySQL 5");
if (sourceIs5 && !destIs5)
    errAbort("%s is running Mysql pre-5, and %s is running MySQL post-5, sorry",
    	destHost, sourceHost);
forceAllGoodChars(sourceHost);
forceAllGoodChars(destHost);
forceAllGoodChars(database);
forceAllGoodChars(table);
if (allDatabases)
    {
    struct sqlConnection *conn = 
    	sqlConnectRemote(sourceHost, user, password, "mysql");
    struct sqlResult *sr = sqlGetResult(conn, "show databases");
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *db = row[0];
	if (!sameWord(db, "mysql"))
	    pushDatabase(sourceHost, sourceIs5, destHost, destIs5, db,
	    	table, FALSE);
	}
    sqlDisconnect(&conn);
    }
else
    pushDatabase(sourceHost, sourceIs5, destHost, destIs5, database, table,
    	TRUE);
flushTables(destHost);
}

void pushToHosts(char *sourceHost, char *database, char *table,
	int destCount, char **destHosts)
/* Push database and table from source to all dest hosts. */
{
struct hash *sourceVars = getMysqlVars(sourceHost);
int i;
for (i=0; i<destCount; ++i)
    pushToHost(sourceHost, sourceVars, database, table, destHosts[i]);
}

void safePush(char *database, char *table, char *destType)
/* Do a bunch more additional error checking on the command line. 
 * Figure out what is the source machine. Figure out list of dest
 * machines. */
{
char *sourceHost = NULL, *destHost = NULL;
char **destHosts = &destHost;
int destCount = 1;
char *envHost = getenv("HOST");


/* Various error checks */
if (envHost == NULL)
    errAbort("Couldn't find HOST environment variable, sorry.");
if (allTables && !sameString(table, "all"))
    errAbort("Expecting 'all' for table with -allTables");
if (allDatabases && !sameString(database, "all"))
    errAbort("Expecting 'all' for database with -allDatabases");
if (allTables && allDatabases)
    errAbort("Please contact cluster-admin to do allTables and allDatabases");
if (sameWord(table, "mysql"))
    errAbort("Please contact cluster-admin to update 'mysql' database");
if (prefix && strlen(table) < 3)
    errAbort("You can only use -prefix if have at least 3 letters in table name.");

/* Figure out sourceHost, destHost from destType */
if (sameString(destType, "beta"))
    {
    sourceHost = alpha;
    destHost = beta;
    }
else if (sameString(destType, "hgw1"))
    {
    sourceHost = beta;
    destHost = "hgw1";
    }
else if (sameString(destType, "rr"))
    {
    sourceHost = beta;
    destHosts = roundRobin;
    destCount = ArraySize(roundRobin);
    }
else if (sameString(destType, "list"))
    {
    char *retBuf;
    sourceHost = beta;
    if (list == NULL)
        errAbort("Must have -list option with destType list.");
    readAllWords(list, &destHosts, &destCount, &retBuf);
    }
else if (sameString(destType, "single"))
    {
    sourceHost = envHost;
    if (singleOp == NULL)
        errAbort("Must have -single option with destType single.");
    destHost = singleOp;
    }
else
    errAbort("Unrecognized destType. Run program with no parameters for usage.");
if (!sameString(sourceHost, envHost))
    errAbort("Please run this command from %s", sourceHost);

pushToHosts(sourceHost, database, table, destCount, destHosts);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
oldOk = optionExists("oldOk");
chrom = optionExists("chrom");
prefix = optionExists("prefix");
allTables = optionExists("allTables");
allDatabases = optionExists("allDatabases");
test = optionExists("test");
list = optionVal("list", NULL);
singleOp = optionVal("single", NULL);
user = cfgOptionEnv("HGDB_USER", "db.user");
password = cfgOptionEnv("HGDB_PASSWORD", "db.password");

safePush(argv[1], argv[2], argv[3]);
return 0;
}

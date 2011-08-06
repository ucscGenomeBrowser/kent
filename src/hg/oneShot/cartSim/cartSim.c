/* cartSim - Simulate cart usage by genome browser users.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "portable.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

int userCount = 10;
int randSeed = 0;
int cgiDelay = 100;
int hitDelay = 100;
int iterations = 100;
double newRatio = 0.1;
boolean innodb = FALSE;
char *engine = "MyISAM";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cartSim - Simulate cart usage by genome browser users.\n"
  "usage:\n"
  "   cartSim host user password database\n"
  "options:\n"
  "   -randSeed=N random number generator seed.  Defaults to pid\n"
  "   -cgiDelay=N number of milliseconds to pretend cgi is crunching. Default is %d\n"
  "   -hitDelay=N number of milliseconds to delay between hits. Default is %d\n"
  "   -iterations=N number of iterations to hit cart.  Default is %d\n"
  "   -newRatio=0.N proportion of hits that are from new users. Default is %g\n"
  "   -create=N create database if it doesn't exist with given number of dummy records\n"
  "   -clone=source clone existing database\n"
  "   -engine=MyISAM|InnoDB - default %s.\n"
  "   -cleanup=N clean up database by getting rid of all but most recent N elements\n"
  "   -verbose=N level of diagnostic output verbosity.  Default is 1.\n"
  , cgiDelay, hitDelay, iterations, newRatio, engine
  );
}

static struct optionSpec options[] = {
   {"userCount", OPTION_INT},
   {"randSeed", OPTION_INT},
   {"cgiDelay", OPTION_INT},
   {"hitDelay", OPTION_INT},
   {"iterations", OPTION_INT},
   {"newRatio", OPTION_DOUBLE},
   {"create", OPTION_INT},
   {"clone", OPTION_STRING},
   {"engine", OPTION_STRING},
   {"cleanup", OPTION_INT},
   {NULL, 0},
};

/* Some data to help make up fake cart contents.  You get prefixVar=val random combinations */
char *prefixes[] = {"pre", "post", "ex", ""};
char *vars[] = {"avocado", "bean", "almond", "peach", "pea", "oat", "artichoke", "lettuce",
   "apple", "beet", "pumpkin", "potato", "strawberry", "raspberry", "sage", "wheat"};
char *vals[] = { "owl", "hawk", "dove", "robin", "bluebird", "raven", "crow", "sparrow",
    "swallow", "falcon", "pigeon", "finch", "thrush", "woodpecker", "starling", };


#define fakePrefix "FAKE "
#define cartNumFields 6

struct dyString *fakeCart(int size)
/* Return a this=that string with so many elements */
{
struct dyString *dy = dyStringNew(0);
int i;
dyStringAppend(dy, fakePrefix);
for (i=0; i<size; ++i)
    {
    if (i != 0)
       dyStringAppendC(dy, '&');
    dyStringPrintf(dy, "%s%s=%s%d",
    	prefixes[rand()%ArraySize(prefixes)],
	vars[rand()%ArraySize(vars)],
	vals[rand()%ArraySize(vals)],
	rand()%10);
    }
return dy;
}

#define userTable "userDb"

void checkNotRealCartTable(struct sqlConnection *conn, char *database, char *table)
/* Abort unless either table doesn't exist, or table exists and is in fake prefix format. */
{
if (!sqlTableExists(conn, table))
    return;
int contentsFieldIx = sqlFieldIndex(conn, table, "contents");
if (contentsFieldIx < 0)
    errAbort("%s.%s doesn't have a contents field", database, table);
char query[256];
safef(query, sizeof(query), "select contents from %s limit 1", table);
char *firstContents = sqlQuickString(conn, query);
if (firstContents != NULL && !startsWith(fakePrefix, firstContents))
    errAbort("%s.%s.contents doesn't start with '%s'", database, table, firstContents);
freez(&firstContents);
}

void checkFakeCartTable(struct sqlConnection *conn, char *database, char *table)
/* Abort unless either table doesn't exist, or table exists and is in fake prefix format. */
{
if (!sqlTableExists(conn, table))
    return;
int contentsFieldIx = sqlFieldIndex(conn, table, "contents");
if (contentsFieldIx < 0)
    errAbort("%s.%s doesn't have a contents field", database, table);
char query[256];
safef(query, sizeof(query), "select contents from %s limit 1", table);
char *firstContents = sqlQuickString(conn, query);
if (firstContents != NULL && !startsWith(fakePrefix, firstContents))
    errAbort("no goot test database %s: %s.contents doesn't start with '%s'", 
    	database, table, firstContents);
freez(&firstContents);
}

void checkNotRealDatabase(char *host, char *user, char *password, char *database)
/* Make sure that database does not contain real looking user table. */
{
struct sqlConnection *conn = sqlMayConnectRemote(host, user, password, database);
if (conn != NULL)
    {
    checkNotRealCartTable(conn, database, userTable);
    sqlDisconnect(&conn);
    }
}

void checkEmptyOrFakeDatabase(char *host, char *user, char *password, char *database)
/* Make sure that either database doesn't exist, or that it does exist and
 * has fake tables. */
{
struct sqlConnection *conn = sqlMayConnectRemote(host, user, password, database);
if (conn != NULL)
    {
    checkFakeCartTable(conn, database, userTable);
    sqlDisconnect(&conn);
    }
}

boolean databaseExists(char *host, char *user, char *password, char *database)
/* Return TRUE if database exists. */
{
struct sqlConnection *conn = sqlMayConnectRemote(host, user, password, database);
if (conn == NULL)
    return FALSE;
sqlDisconnect(&conn);
return TRUE;
}

void createEmptyDatabase(char *host, char *user, char *password, char *database)
/* Create a new database with no tables. */
{
struct sqlConnection *conn = sqlConnectRemote(host, user, password, NULL);
char query[512];
safef(query, sizeof(query), "create database %s", database);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
}

void dropUserTable(char *host, char *user, char *password, char *database)
/* Drop database if it exists. */
{
if (databaseExists(host, user, password, database))
    {
    struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);
    if (sqlTableExists(conn, userTable))
	{
	char query[512];
	safef(query, sizeof(query), "drop table %s", userTable);
	sqlUpdate(conn, query);
	}
    sqlDisconnect(&conn);
    }
}

void createNewFakeDatabase(char *host, char *user, char *password, char *database)
/* Create a fake database with empty fake useDb table. */
{
dropUserTable(host, user, password, database);
if (!databaseExists(host, user, password, database))
    createEmptyDatabase(host, user, password, database);
struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);
char query[1024];
safef(query, sizeof(query), 
"CREATE TABLE %s  (\n"
"    id integer unsigned not null auto_increment,	# Cart ID\n"
"    contents longblob not null,	# Contents - encoded variables\n"
"    reserved tinyint not null,	# always 0\n"
"    firstUse DATETIME not null,	# First time this was used\n"
"    lastUse DATETIME not null,	# Last time this was used\n"
"    useCount int not null,	# Number of times used\n"
"              #Indices\n"
"    PRIMARY KEY(id)\n"
") ENGINE = %s\n", userTable, engine);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
}


void createFakeEntry(struct sqlConnection *conn, int size)
/* Create a fake entry of the given size. */
{
struct dyString *contents = fakeCart(size);
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "INSERT %s VALUES(0,'%s',0,now(),now(),0)", userTable, contents->string);
sqlUpdate(conn, query->string);
dyStringClear(query);
dyStringFree(&contents);
}

int randomFakeSize()
/* Return a fake size of cart entry */
{
if (rand()%3 == 0)
    {
    double a = 0.0001 * (rand()%10000 + 1);
    return (int)(a*a*a*a*a*a*a*10000) + 10;
    }
else
    return rand()%20 + 1;
}

void createFakeEntries(char *host, char *user, char *password, char *database, int count)
/* create count new fake entries in database. */
{
struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);
int i;
for (i=0; i<count; ++i)
    createFakeEntry(conn, randomFakeSize());
sqlDisconnect(&conn);
}

void fakeCloneOldTable(struct sqlConnection *oldConn, struct sqlConnection *newConn, char *table)
/* Clone cart table in newConn from oldConn. Add fake prefix to
 * contents field to help mark it as fake. */
{
char query[256];
safef(query, sizeof(query), "select * from %s", table);
struct sqlResult *sr = sqlGetResult(oldConn, query);
char **row;
FILE *f = hgCreateTabFile(NULL, table);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int i;
    for (i=0; i<cartNumFields; ++i)
        {
	if (i != 0)
	    fprintf(f, "\t");
	if (i == 1)
	    fprintf(f, "%s", fakePrefix);
	fprintf(f, "%s", row[i]);
	}
    fprintf(f, "\n");
    }
hgLoadTabFile(newConn, NULL, table, &f);
hgUnlinkTabFile(NULL, table);
}

void cloneOldDatabase(char *host, char *user, char *password, char *newDatabase, char *oldDatabase)
/* Write out old database and read in new one. */
{
struct sqlConnection *oldConn = sqlConnectRemote(host, user, password, oldDatabase);
struct sqlConnection *newConn = sqlConnectRemote(host, user, password, newDatabase);
fakeCloneOldTable(oldConn, newConn, userTable);
}

int *getSomeInts(struct sqlConnection *conn, char *table, char *field, int limit)
/* Return an array of ints from field in table. */
{
int *result, i;
char query[512];
safef(query, sizeof(query), "select %s from %s limit %d", field, table, limit);
struct sqlResult *sr = sqlGetResult(conn, query);
AllocArray(result, limit);
for (i=0; i<limit; ++i)
    {
    char **row = sqlNextRow(sr);
    if (row == NULL)
        errAbort("Less than %d rows in %s", limit, table);
    result[i] = sqlSigned(row[0]);
    }
sqlFreeResult(&sr);
return result;
}

void updateOne(struct sqlConnection *conn, char *table, char *contents, int id, int useCount)
/* Update one of cart tables with new contents. */
{
struct dyString *dy = dyStringNew(0);
dyStringPrintf(dy, "UPDATE %s SET contents='", table);
dyStringAppend(dy, contents);
dyStringPrintf(dy, "',lastUse=now(),useCount=%d ", useCount+1);
dyStringPrintf(dy, " where id=%u", id);
sqlUpdate(conn, dy->string);
dyStringFree(&dy);
}

int dummyQuery(struct sqlConnection *conn, char *table, int id, char **retContents)
/* Ask database for useCount and contents. Just return useCount, fill in *retContents  */
{
char *contents = "";
char query[256];
safef(query, sizeof(query), "select useCount,contents from %s where id=%d", table, id);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
int useCount = 0;
if (row != NULL)
    {
    contents = row[1];
    useCount = sqlUnsigned(row[0]);
    }
*retContents = cloneString(contents);
sqlFreeResult(&sr);
return useCount;
}

int dummyInsert(struct sqlConnection *conn, char *table)
/* Insert new row into cart table and return ID */
{
char query[256];
safef(query, sizeof(query), "INSERT %s VALUES(0,\"\",0,now(),now(),0)",
      table);
sqlUpdate(conn, query);
return sqlLastAutoId(conn);
}

boolean randomBitFromProb(double prob)
/* Where prob is between 0 and 1,  return TRUE with at given probability,
 * FALSE otherwise. */
{
return (rand() % 1000000 <= prob*1000000);
}

void cartSimulate(char *host, char *user, char *password, char *database)
/* Simulate action of various UCSC Genome Browser CGIs on cart. */
{
/* Figure out size of tables. */
struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);
int userDbSize = sqlQuickNum(conn, "select count(*) from userDb");
if (userDbSize == 0)
    errAbort("%s.%s table is empty", database, userTable);
int maxSampleSize = 1024*1024;
int sampleSize = min(userDbSize, maxSampleSize);
verbose(2, "# userDb has %d rows, sampling %d\n"
	, userDbSize, sampleSize);

/* Get sample of user id's. */
int *userIds = getSomeInts(conn, "userDb", "id", sampleSize);

/* Get userCount random indexes. */
int *randomIxArray, ix;
AllocArray(randomIxArray, userCount);
verbose(2, "random user ix:\n");
for (ix=0; ix<userCount; ++ix)
    {
    randomIxArray[ix] = rand() % sampleSize;
    verbose(2, "%d ", randomIxArray[ix]);
    }
verbose(2, "\n");

sqlDisconnect(&conn);

int iteration = 0;
for (;;)
    {
    for (ix = 0; ix < userCount; ++ix)
	{
	int randomIx = rand()%sampleSize;
	boolean doNew = randomBitFromProb(newRatio);
	long startTime = clock1000();
	struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);
	long connectTime = clock1000();
	struct dyString *contents = fakeCart(randomFakeSize());

	char *userContents = NULL;
	int userId = userIds[randomIx];
	if (doNew)
	    userId = userIds[randomIx] = dummyInsert(conn, userTable);
	int userUseCount = dummyQuery(conn, userTable, userId, &userContents);
	long userReadTime = clock1000();

	sleep1000(cgiDelay);
	long cgiSleepTime = clock1000();

	updateOne(conn, userTable, contents->string, userId, userUseCount);
	long userWriteTime = clock1000();

	sqlDisconnect(&conn);
	long disconnectTime = clock1000();

	printf("%ld total, %ld oldSize, %ld newSize, %ld connect, %ld userRead, %ld userWrite, %ld disconnect\n",
		disconnectTime - startTime - (cgiSleepTime - userReadTime),
		(long) strlen(userContents),
		(long)contents->stringSize,
		connectTime - startTime,
		userReadTime - connectTime,
		userWriteTime - cgiSleepTime,
		disconnectTime - userWriteTime );

	dyStringFree(&contents);
	freez(&userContents);

	sleep1000(hitDelay);
	if (++iteration >= iterations)
	    return;
	}
    }

errAbort("cartSimulate(%s %s %s %s) not implemented", host, user, password, database);
}


void cleanupTable(char *host, char *user, char *password, char *database, char *table, int target)
/* Trim table to target most recent items.  */
{
uglyTime(NULL);
struct sqlConnection *conn = sqlConnectRemote(host, user, password, database);
struct dyString *query = dyStringNew(0);
dyStringPrintf(query, "select count(*) from %s", table);
int initialCount = sqlQuickNum(conn, query->string);
uglyTime("%d initial vs %d target", initialCount, target);
if (target < initialCount)
    {
    /* Query database for id's ordered by age */

    dyStringClear(query);
    dyStringPrintf(query, "select id,now()-lastUse age from %s order by age", table);
    struct sqlResult *sr = sqlGetResult(conn, query->string);

    /* Build up new query that'll delete old things. */
    dyStringClear(query);
    dyStringPrintf(query, "delete from %s where id in (", table);
    int i=0;
    boolean addComma = FALSE;
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (++i > target)
	   {
	   if (addComma)
	       dyStringAppendC(query, ',');
	   else
	       addComma = TRUE;
	   dyStringPrintf(query, "'%s'", row[0]);
	   }
	}
    dyStringPrintf(query, ")");
    sqlFreeResult(&sr);
    uglyTime("made delete query %d chars", query->stringSize);

    /* Excute deletion */
    sqlUpdate(conn, query->string);
    uglyTime("deleted");
    }
sqlDisconnect(&conn);
}

void cartSim(char *host, char *user, char *password, char *database)
/* cartSim - Simulate cart usage by genome browser users. */
{
char *create = optionVal("create", NULL);
char *clone = optionVal("clone", NULL);
char *cleanup = optionVal("cleanup", NULL);
if (create != NULL)
    {
    checkNotRealDatabase(host, user, password, database);
    checkEmptyOrFakeDatabase(host, user, password, database);
    createNewFakeDatabase(host, user, password, database);
    createFakeEntries(host, user, password, database, sqlUnsigned(create));
    }
else if (clone != NULL)
    {
    checkNotRealDatabase(host, user, password, database);
    checkEmptyOrFakeDatabase(host, user, password, database);
    createNewFakeDatabase(host, user, password, database);
    cloneOldDatabase(host, user, password, database, clone);
    }
else if (cleanup != NULL)
    {
    cleanupTable(host, user, password, database, userTable, sqlUnsigned(cleanup));
    }
else
    {
    cartSimulate(host, user, password, database);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
userCount = optionInt("userCount", userCount);
randSeed = optionInt("randSeed", getpid());
srand(randSeed);
cgiDelay = optionInt("cgiDelay", cgiDelay);
hitDelay = optionInt("hitDelay", hitDelay);
iterations = optionInt("iterations", iterations);
newRatio = optionDouble("newRatio", newRatio);
engine = optionVal("engine", engine);
cartSim(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

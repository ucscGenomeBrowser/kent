/* Program to test -dbLink functions of autoSql. Assumes that
   there is a working database available and that you have the
   correct passwords in ~/.hg.conf and write permission to the
   database. */
#include "common.h"
#include "jksql.h"
#include "linefile.h"
#include "dystring.h"
#include "newTest.h"
#include "hgConfig.h"
#include "hdb.h"

char *testTableName = "autoTest"; /* table name that newTest.as will produce. */
int numPassed = 0;  /* How many tests we have passed. */
int numFailed = 0;  /* How many tests we have failed. */

void usage()
{
errAbort("dbLinkTest - Test -dbLink functions of autoSql. Assumes that\n"
	 "there is a working database available and that you have the\n"
	 "correct passwords in ~/.hg.conf and write permission to the\n"
	 "database.\n"
	 "usage:\n\t"
	 "dbLinkTest");
}

struct sqlConnection *dbConnect()
/* Get a connection do database specified in ~/.hg.conf */
{
struct sqlConnection *conn = NULL;
char *hdbHost 	= cfgOption("db.host");
char *hdbUser 	= cfgOption("db.user");
char *hdbPassword = cfgOption("db.password");
char *hdbDatabase = cfgOption("db.database");
if(hdbDatabase == NULL)
    hdbDatabase = hDefaultDb();
if(hdbHost == NULL || hdbUser == NULL || hdbPassword == NULL)
    errAbort("Cannot read in connection setting from configuration file.");
conn = sqlConnectRemote(hdbHost, hdbUser, hdbPassword, hdbDatabase);
return conn;
}

void setupTable(struct sqlConnection *conn) 
/* Set up the autoTest table for testing. Call dropTable() later
 to remove table. */
{
struct lineFile *lf = lineFileOpen("newTest.sql", TRUE);
char *update = NULL;
char *noNull = NULL, *noNullClone = NULL;
char *line = NULL;
struct dyString *ds = newDyString(256);
int size = 0;
if(sqlTableExists(conn, testTableName))
    errAbort("dbLinkTest.c::setupTable() - Table autoTest already exists. Can't create another.");
while(lineFileNextReal(lf, &line)) 
    {
    char *tmp = strstr(line, "not null");
    if(tmp != NULL)
	{
	*tmp = ',';
	tmp++;
	*tmp = '\0';
	}
    subChar(line, '\t', ' ');
    dyStringPrintf(ds, "%s", line);
    }
update = replaceChars(ds->string, "PRIMARY KEY", "UNIQUE");
sqlUpdate(conn, update);
freez(&update);
dyStringFree(&ds);
lineFileClose(&lf);
}

void dropTable(struct sqlConnection *conn)
/* Drop the test table that was created in setupTable(). */
{
char update[256];
snprintf(update, sizeof(update), "drop table %s", testTableName);
sqlUpdate(conn, update);
}

int countWcDiff(char *fileName)
/** Count how many lines counted are reported by 'wc'. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
int lineSize;
int numLineDiff=0;
char *row[3];
lineFileNeedNext(lf, &line, &lineSize);
line = trimSpaces(line);
chopString(line, " ", row, 3);
numLineDiff=atoi(row[0]);
lineFileClose(&lf);
return numLineDiff;
}

boolean testingFileIdentical(char *file1, char *file2)
/** Check to see if two files are the same. */
{
struct dyString *command = newDyString(2048);
char *resultsFile = "_testingFileIdentical.tmp";
int result = 0;
boolean identical = FALSE;
int numDiff = 0;
dyStringPrintf(command,  "diff -b %s %s | wc > %s", file1, file2,resultsFile );
result = system(command->string);
if(result != 0)
    errAbort("testing::testingFileIdentical() - Failed running diff on %s and %s", file1, file2);
numDiff = countWcDiff(resultsFile);
if(numDiff ==0)
    identical = TRUE;
freeDyString(&command);
remove(resultsFile);
return identical;
}

struct autoTest *newAutoTestSample()
/* Create an autotest structure and return it. */
{
struct autoTest *at = NULL;
AllocVar(at);
at->id=1;
snprintf(at->shortName, sizeof(at->shortName), "shortName");
at->longName = cloneString("autoSql autoTest longName");
at->aliases[0] = cloneString("autoSql autoTest aliases");
at->aliases[1] = cloneString("autoSql autoTest aliases");
at->aliases[2] = cloneString("autoSql autoTest aliases");
at->ptCount = 3;
AllocArray(at->pts, 3);
at->pts[0] = 0;
at->pts[1] = 1;
at->pts[2] = 2;
at->difCount = 3;
AllocArray(at->difs, 3);
at->difs[0] = '0';
at->difs[1] = '1';
at->difs[2] = '2';
at->valCount = 3;
AllocArray(at->vals, at->valCount);
at->vals[0] = cloneString("autoSql autoTest vals");
at->vals[1] = cloneString("autoSql autoTest vals");
at->vals[2] = cloneString("autoSql autoTest vals");
return at;
}

void testInsertion(struct sqlConnection *conn, boolean escaped)
/* Test saveToDb() and loadByQuery() functions. */
{
struct autoTest *at=NULL, *atFromDb=NULL;
char query[256];
char *tFile = "_testAutoTest.test";
char *tDbFile = "_testAutoTestFromDb.test";
FILE *out = NULL;
FILE *outDb = NULL;
boolean result = FALSE;

if(escaped)
    fprintf(stderr, "testEscapedInsertion() - ");
else
    fprintf(stderr, "testRegularInsertion() - ");
fflush(stderr);

/* Load into db and then read back out. */
at = newAutoTestSample();
if(escaped)
    {
    freez(&at->longName);
    at->longName = cloneString("autoSql's autoTest longName");
    at->id++;
    autoTestSaveToDbEscaped(conn, at, testTableName, 1024);
    }
else 
    autoTestSaveToDb(conn, at, testTableName, 1024);
snprintf(query, sizeof(query), "select * from %s where id = %d", testTableName, at->id);
atFromDb = autoTestLoadByQuery(conn, query);

/* Tab out results. */
if(fileExists(tFile))
    errAbort("Can't create file %s. Already exists", tFile);
out = mustOpen(tFile, "w");
autoTestTabOut(at, out);
if(fileExists(tDbFile))
    errAbort("Can't create file %s. Already exists", tDbFile);
outDb = mustOpen(tDbFile, "w");
autoTestTabOut(atFromDb, outDb);

carefulClose(&out);
carefulClose(&outDb);

/* Compare files. */
result = testingFileIdentical(tFile, tDbFile);
if(result)
    {
    warn("PASSED.");
    numPassed++;
    }
else
    {
    warn("FAILED.");
    numFailed++;
    }

/* Cleanup. */
remove(tFile);
remove(tDbFile);
autoTestFree(&at);
autoTestFree(&atFromDb);
}

void doTests()
/* Run our set of tests that use functions exploited by -dbLink flag. */
{
struct sqlConnection *conn = dbConnect();
setupTable(conn);
testInsertion(conn, FALSE);
testInsertion(conn, TRUE);
dropTable(conn);
if(numFailed == 0)
    warn("dbLinkTest: PASSED. All %d tests passed", numPassed);
else
    warn("dbLinkTest: FAILURES. %d tests failed", numFailed);
sqlDisconnect(&conn);
}
    
int main(int argc, char *argv[])
{
if(argc != 1)
    usage();
doTests();
return 0;
}

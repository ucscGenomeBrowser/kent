/* dbTrash - drop tables from a database older than specified N hours. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: dbTrash.c,v 1.17 2009/01/15 20:29:01 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dbTrash - drop tables from a database older than specified N hours\n"
  "usage:\n"
  "   dbTrash -age=N [-drop] [-historyToo] [-db=<DB>] [-verbose=N]\n"
  "options:\n"
  "   -age=N - number of hours old to qualify for drop.  N can be a float.\n"
  "   -drop - actually drop the tables, default is merely to display tables.\n"
  "   -db=<DB> - Specify a database to work with, default is "
	CUSTOM_TRASH ".\n"
  "   -historyToo - also consider the table called 'history' for deletion.\n"
  "               - default is to leave 'history' alone no matter how old.\n"
  "               - this applies to the table 'metaInfo' also.\n"
  "   -extFile    - check extFile for lines that reference files\n"
  "               - no longer in trash\n"
  "   -extDel     - delete lines in extFile that fail file check\n"
  "               - otherwise just verbose(2) lines that would be deleted\n"
  "   -topDir     - directory name to prepend to file names in extFile\n"
  "               - default is /usr/local/apache/trash\n"
  "               - file names in extFile are typically: \"../trash/ct/...\"\n"
  "   -tableStatus  - use 'show table status' to get size data, very inefficient\n"
  "   -delLostTable - delete tables that exist but are missing from metaInfo\n"
  "                 - this operation can be even slower than -tableStatus\n"
  "                 - if there are many tables to check.\n"
  "   -verbose=N - 2 == show arguments, dates, and dropped tables,\n"
  "              - 3 == show date information for all tables."
  );
}

static struct optionSpec options[] = {
    {"age", OPTION_FLOAT},
    {"extFile", OPTION_BOOLEAN},
    {"extDel", OPTION_BOOLEAN},
    {"drop", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"topDir", OPTION_STRING},
    {"tableStatus", OPTION_BOOLEAN},
    {"delLostTables", OPTION_BOOLEAN},
    {"historyToo", OPTION_BOOLEAN},
    {NULL, 0},
};

static double ageHours = 0.0;	/*	must be specified	*/
static boolean drop = FALSE;		/*	optional	*/
static char *db = CUSTOM_TRASH;		/*	optional	*/
static boolean historyToo = FALSE;	/*	optional	*/

static time_t timeNow = 0;
static time_t dropTime = 0;

static boolean extFileCheck = FALSE;
static boolean extDel = FALSE;
static boolean tableStatus = FALSE;
static boolean delLostTable = FALSE;
static char *topDir = "/usr/local/apache/trash";

void checkExtFile(struct sqlConnection *conn)
/* check extFile table for files that have been removed */
{
char query[256];
struct sqlResult *sr;
char **row;
char buffer[4 * 1024];
char *name = buffer;
struct slName *list = NULL;

if (! sqlTableExists(conn, CT_EXTFILE))
    {
    verbose(2,"WARNING: -extFile option specified, extFile table does not exist\n");
    verbose(2,"at this time (Jan 2009), the extFile table is unused.\n");
    return;
    }

safef(query,sizeof(query),"select id,path from %s",CT_EXTFILE);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (topDir != NULL)
	safef(buffer, sizeof buffer, "%s/%s",topDir, row[1]);
    else
	name = row[1];

    if (!fileExists(name))
	{
	struct slName *new = newSlName(row[0]);
	slAddHead(&list, new);
	}
    }
sqlFreeResult(&sr);

struct slName *one;
for(one = list; one; one = one->next)
    {
    safef(query,sizeof(query),"delete from %s where id='%s'",
	CT_EXTFILE, one->name);
    if (extDel)
	sqlUpdate(conn, query);
    verbose(2,"%s\n",query);
    }
slFreeList(&list);
}

// Macro some common code used twice below

#define STATUS_INIT \
sr = sqlGetResult(conn, query); \
nameIx = sqlFieldColumn(sr, "Name"); \
createTimeIx = sqlFieldColumn(sr, "Create_time"); \
updateTimeIx = sqlFieldColumn(sr, "Update_time"); \
dataLengthIx = sqlFieldColumn(sr, "Data_length"); \
indexLengthIx = sqlFieldColumn(sr, "Index_length");

/*	Update_time is sometimes NULL on MySQL 5
 *	so if it fails, then check the Create_time
 */

#define SCAN_STATUS \
struct tm tm; \
time_t timep = 0; \
timeIxUsed = updateTimeIx; \
if ((row[updateTimeIx] != NULL) && \
	(sscanf(row[updateTimeIx], "%4d-%2d-%2d %2d:%2d:%2d", \
	    &(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday), \
		&(tm.tm_hour), &(tm.tm_min), &(tm.tm_sec)) != 6) ) \
    { \
    timeIxUsed = createTimeIx; \
    if (sscanf(row[createTimeIx], "%4d-%2d-%2d %2d:%2d:%2d", \
	&(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday), \
	    &(tm.tm_hour), &(tm.tm_min), &(tm.tm_sec)) != 6) \
	{ \
	verbose(2,"%s %s %s\n", \
	    row[createTimeIx],row[updateTimeIx],row[nameIx]); \
	errAbort("could not parse date %s or %s on table %s\n", \
	    row[createTimeIx], row[updateTimeIx], row[nameIx]); \
	} \
    } \
tm.tm_year -= 1900; \
tm.tm_mon -= 1; \
tm.tm_isdst = -1; \
timep = mktime(&tm);

void dbTrash(char *db)
/* dbTrash - drop tables from a database older than specified N hours. */
{
char query[256];
struct sqlResult *sr;
char **row;
int updateTimeIx;
int createTimeIx;
int dataLengthIx;
int indexLengthIx;
int nameIx;
int timeIxUsed;
unsigned long long totalSize = 0;
// expiredTableNames: table exists and is in metaInfo and subject to age limits
struct slName *expiredTableNames = NULL;
struct slName *lostTables = NULL;	// tables existing but not in metaInfo
unsigned long long lostTableCount = 0;
struct hash *expiredHash = newHash(10); // as determined by metaInfo
struct hash *notExpiredHash = newHash(10);
struct sqlConnection *conn = sqlConnect(db);

if (extFileCheck)
    checkExtFile(conn);

time_t ageSeconds = (time_t)(ageHours * 3600);	/*	age in seconds	*/
safef(query,sizeof(query),"select name,UNIX_TIMESTAMP(lastUse) from %s WHERE "
    "lastUse < DATE_SUB(NOW(), INTERVAL %ld SECOND);", CT_META_INFO,ageSeconds);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAddInt(expiredHash, row[0], sqlSigned(row[1]));
sqlFreeResult(&sr);
safef(query,sizeof(query),"select name,UNIX_TIMESTAMP(lastUse) from %s WHERE "
    "lastUse >= DATE_SUB(NOW(), INTERVAL %ld SECOND);",CT_META_INFO,ageSeconds);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    hashAddInt(notExpiredHash, row[0], sqlSigned(row[1]));
sqlFreeResult(&sr);

if (tableStatus)  // show table status is very expensive, use only when asked
    {
    /*	run through the table status business to get table size information */
    safef(query,sizeof(query),"show table status");
    STATUS_INIT;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	/* if not doing history too, and this is the history table, next row */
	if ((!historyToo) && (sameWord(row[nameIx],"history")))
	    continue;
	/* also skip the metaInfo table */
	if ((!historyToo) && (sameWord(row[nameIx],CT_META_INFO)))
	    continue;
	/* don't delete the extFile table  */
	if (sameWord(row[nameIx],CT_EXTFILE))
	    continue;

	SCAN_STATUS;

	if (hashLookup(expiredHash,row[nameIx]))
	    {
	    slNameAddHead(&expiredTableNames, row[nameIx]);
	    verbose(3,"%s %ld drop %s\n",row[timeIxUsed], (unsigned long)timep,
		    row[nameIx]);
	    /*	 If sizes are non-NULL, add them up	*/
	    if ( ((char *)NULL != row[dataLengthIx]) &&
		    ((char *)NULL != row[indexLengthIx]) )
		totalSize += sqlLongLong(row[dataLengthIx])
		    + sqlLongLong(row[indexLengthIx]);
	    hashRemove(expiredHash, row[nameIx]);
	    }
	else
	    {
	    if (hashLookup(notExpiredHash,row[nameIx]))
		verbose(3,"%s %ld OK %s\n",row[timeIxUsed], (unsigned long)timep,
		    row[nameIx]);
	    else
		{	/* table exists, but not in metaInfo, is it old enough ? */
		if (timep < dropTime)
		    {
		    slNameAddHead(&expiredTableNames, row[nameIx]);
		    verbose(2,"%s %ld dropt %s lost table\n",
			row[timeIxUsed], (unsigned long)timep, row[nameIx]);
		    /*       If sizes are non-NULL, add them up     */
		    if ( ((char *)NULL != row[dataLengthIx]) &&
			((char *)NULL != row[indexLengthIx]) )
			    totalSize += sqlLongLong(row[dataLengthIx])
				+ sqlLongLong(row[indexLengthIx]);
		    }
		else
		    verbose(3,"%s %ld OKt %s\n",row[timeIxUsed],
			(unsigned long)timep, row[nameIx]);
		}
	    }
	}
    sqlFreeResult(&sr);
    }
else
    {	// simple 'show tables' is more efficient than 'show table status'
    safef(query,sizeof(query),"show tables");
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (hashLookup(expiredHash,row[0]))
	    {
	    slNameAddHead(&expiredTableNames, row[0]);
	    time_t lastUse = (time_t)hashIntVal(expiredHash,row[0]);
	    struct tm *lastUseTm = localtime(&lastUse);
	    verbose(3,"%4d-%02d-%02d %02d:%02d:%02d %ld drop %s\n",
		lastUseTm->tm_year+1900, lastUseTm->tm_mon+1,
		lastUseTm->tm_mday, lastUseTm->tm_hour, lastUseTm->tm_min,
		lastUseTm->tm_sec, (unsigned long)lastUse,row[0]);
	    hashRemove(expiredHash, row[0]);
	    }
	else if (hashLookup(notExpiredHash,row[0]))
	    {
	    time_t lastUse = (time_t)hashIntVal(notExpiredHash,row[0]);
	    struct tm *lastUseTm = localtime(&lastUse);
	    verbose(3,"%4d-%02d-%02d %02d:%02d:%02d %ld OK %s\n",
		lastUseTm->tm_year+1900, lastUseTm->tm_mon+1,
		lastUseTm->tm_mday, lastUseTm->tm_hour, lastUseTm->tm_min,
		lastUseTm->tm_sec, (unsigned long)lastUse,row[0]);
	    }
	else
	    {
	    struct slName *el = slNameNew(row[0]);
	    slAddHead(&lostTables, el);
	    }
        }
    sqlFreeResult(&sr);
    lostTableCount = slCount(lostTables);
    // If tables exist, but not in metaInfo, check their age to expire them.
    // It turns out even this show table status is slow too, so, only
    // run thru it if asked to eliminate lost tables.  It is better to
    // do this operation with the stand-alone perl script on the customTrash
    // database machine.
    if (delLostTable && lostTables)
	{
	struct slName *el;
	for (el = lostTables; el != NULL; el = el->next)
	    {
	    if (sameWord(el->name,"history"))
		continue;
	    if (sameWord(el->name,CT_META_INFO))
		continue;
	    if (sameWord(el->name,CT_EXTFILE))
		continue;
	    boolean oneTableOnly = FALSE; // protect against multiple tables
	    /*	get table time information to see if it is expired */
	    safef(query,sizeof(query),"show table status like '%s'", el->name);
	    STATUS_INIT;

	    while ((row = sqlNextRow(sr)) != NULL)
		{
		if (oneTableOnly)
		    errAbort("ERROR: query: '%s' returned more than one table "
				"name\n", query);
		else
		    oneTableOnly = TRUE;
		if (differentWord(row[nameIx], el->name))
		    errAbort("ERROR: query: '%s' did not return table name '%s' != '%s'\n", query, el->name, row[nameIx]);

		SCAN_STATUS;

		if (timep < dropTime)
		    {
		    slNameAddHead(&expiredTableNames, row[nameIx]);
		    verbose(2,"%s %ld dropt %s lost table\n",
			row[timeIxUsed], (unsigned long)timep, row[nameIx]);
		    }
		else
		    verbose(3,"%s %ld OKt %s\n",
			row[timeIxUsed], (unsigned long)timep, row[nameIx]);
		}
	    sqlFreeResult(&sr);
	    }
	}
    }

/*	perhaps the table was already dropped, but not from the metaInfo */
struct hashEl *elList = hashElListHash(expiredHash);
struct hashEl *el;
for (el = elList; el != NULL; el = el->next)
    {
    verbose(2,"%s exists in %s only\n", el->name, CT_META_INFO);
    if (drop)
	ctTouchLastUse(conn, el->name, FALSE); /* removes metaInfo row */
    }

if (drop)
    {
    char comment[256];
    if (expiredTableNames)
	{
	struct slName *el;
	int droppedCount = 0;
	/* customTrash DB user permissions do not have permissions to
 	 * drop tables.  Must use standard special user that has all
 	 * permissions.  If we are not using the standard user at this
 	 * point, then switch to it.
	 */
	if (sameWord(db,CUSTOM_TRASH))
	    {
	    sqlDisconnect(&conn);
	    conn = sqlConnect(db);
	    }
	for (el = expiredTableNames; el != NULL; el = el->next)
	    {
	    verbose(2,"# drop %s\n", el->name);
	    sqlDropTable(conn, el->name);
	    ctTouchLastUse(conn, el->name, FALSE); /* removes metaInfo row */
	    ++droppedCount;
	    }
	/* add a comment to the history table and finish up connection */
	if (tableStatus)
	    safef(comment, sizeof(comment), "Dropped %d tables with "
		"total size %llu, %llu lost tables",
		    droppedCount, totalSize, lostTableCount);
	else
	    safef(comment, sizeof(comment),
		"Dropped %d tables, no size info, %llu lost tables",
		    droppedCount, lostTableCount);
	verbose(2,"# %s\n", comment);
	hgHistoryComment(conn, "%s", comment);
	}
    else
	{
	safef(comment, sizeof(comment),
	    "Dropped no tables, none expired, %llu lost tables",
		lostTableCount);
	verbose(2,"# %s\n", comment);
	}
    }
else
    {
    char comment[256];
    if (expiredTableNames)
	{
	int droppedCount = slCount(expiredTableNames);
	if (tableStatus)
	    safef(comment, sizeof(comment), "Would have dropped %d tables with "
		"total size %llu, %llu lost tables",
		    droppedCount, totalSize, lostTableCount);
	else
	    safef(comment, sizeof(comment),
		"Would have dropped %d tables, no size info, %llu lost tables",
		    droppedCount, lostTableCount);
	verbose(2,"# %s\n", comment);
	}
    else
	{
	safef(comment, sizeof(comment),
	    "Would have dropped no tables, none expired, %llu lost tables",
		lostTableCount);
	verbose(2,"# %s\n", comment);
	}
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct tm *tm;

if (argc < 2)
    usage();
optionInit(&argc, argv, options);

if (!optionExists("age"))
    {
    verbose(1,"ERROR: must specify an age argument, e.g.: -age=8\n");
    usage();
    }
if (time(&timeNow) < 1)
    errAbort("can not obtain current time via time() function\n");
tm = localtime(&timeNow);
ageHours = optionFloat("age", 0.0);
time_t ageSeconds = (time_t)(ageHours * 3600);	/*	age in seconds	*/
dropTime = timeNow - ageSeconds;
if (ageHours > 0.0)
    {
    verbose(2,"#	specified age = %f hours = %ld seconds\n", ageHours,
	(unsigned long)ageSeconds);
    verbose(2,"#	current time: %d-%02d-%02d %02d:%02d:%02d %ld\n",
	1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec, (unsigned long)timeNow);
    tm = localtime(&dropTime);
    verbose(2,"#	   drop time: %d-%02d-%02d %02d:%02d:%02d %ld\n",
	1900+tm->tm_year, 1+tm->tm_mon, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec, (unsigned long)dropTime);
    }
else
    {
    verbose(1,"ERROR: specified age %.f must be greater than 0.0\n", ageHours);
    usage();
    }
drop = optionExists("drop");
historyToo = optionExists("historyToo");
db = optionVal("db",db);
extFileCheck = optionExists("extFile");
extDel = optionExists("extDel");
tableStatus = optionExists("tableStatus");
topDir = optionVal("topDir", topDir);
verbose(2,"#	drop requested: %s\n", drop ? "TRUE" : "FALSE");
verbose(2,"#	    historyToo: %s\n", historyToo ? "TRUE" : "FALSE");
verbose(2,"#	       extFile: %s\n", extFileCheck ? "TRUE" : "FALSE");
verbose(2,"#	        extDel: %s\n", extDel ? "TRUE" : "FALSE");
verbose(2,"#	   tableStatus: %s\n", tableStatus ? "TRUE" : "FALSE");
verbose(2,"#	        topDir: %s\n", topDir);
verbose(2,"#	database: %s\n", db);

dbTrash(db);
if (verboseLevel() > 1)
    printVmPeak();
return 0;
}

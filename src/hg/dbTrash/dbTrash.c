/* dbTrash - drop tables from a database older than specified N hours. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: dbTrash.c,v 1.3 2006/05/22 22:41:47 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dbTrash - drop tables from a database older than specified N hours\n"
  "usage:\n"
  "   dbTrash -age=N [-drop] [-db=<DB>]\n"
  "options:\n"
  "   -age=N - number of hours old to qualify for drop.  N can be a float.\n"
  "   -drop - actually drop the tables, default is merely to display tables.\n"
  "   -db=<DB> - Specify a database to work with, default is customTrash.\n"
  "   -historyToo - also consider the table called 'history' for deletion.\n"
  "               - default is to leave 'history' alone no matter how old.\n"
  "   -verbose=N - 2 == show arguments, dates, and dropped tables,\n"
  "              - 3 == show date information for all tables."
  );
}

static struct optionSpec options[] = {
    {"age", OPTION_FLOAT},
    {"drop", OPTION_BOOLEAN},
    {"drop", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {"historyToo", OPTION_BOOLEAN},
    {NULL, 0},
};

static double age = 0.0;	/*	must be specified	*/
static boolean drop = FALSE;		/*	optional	*/
static char *db = CUSTOM_TRASH;		/*	optional	*/
static boolean historyToo = FALSE;	/*	optional	*/

static time_t timeNow = 0;
static time_t ageTime = 0;
static time_t dropTime = 0;

void dbTrash(char *db)
/* dbTrash - drop tables from a database older than specified N hours. */
{
struct sqlConnection *conn = sqlCtConn(TRUE);
char query[256];
struct sqlResult *sr;
char **row;
int updateTimeIx;
int createTimeIx;
int nameIx;
int timeIxUsed;
struct slName *tableNames = NULL;	/*	subject to age limits	*/

if (differentWord(db,CUSTOM_TRASH))
    {
    sqlDisconnect(&conn);
    conn = sqlConnect(db);
    }
safef(query,sizeof(query),"show table status");
sr = sqlGetResult(conn, query);
nameIx = sqlFieldColumn(sr, "Name");
createTimeIx = sqlFieldColumn(sr, "Create_time");
updateTimeIx = sqlFieldColumn(sr, "Update_time");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct tm tm;
    time_t timep = 0;

    /* if not doing history too, and this is the history table, next row */
    if ((!historyToo) && (sameWord(row[nameIx],"history")))
	continue;

    /*	Update_time is sometimes NULL on MySQL 5
     *	so if it fails, then check the Create_time
     */
    timeIxUsed = updateTimeIx;
    if (sscanf(row[updateTimeIx], "%4d-%2d-%2d %2d:%2d:%2d",
	&(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday),
	    &(tm.tm_hour), &(tm.tm_min), &(tm.tm_sec)) != 6)
	{
	timeIxUsed = createTimeIx;
	if (sscanf(row[createTimeIx], "%4d-%2d-%2d %2d:%2d:%2d",
	    &(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday),
		&(tm.tm_hour), &(tm.tm_min), &(tm.tm_sec)) != 6)
	    {
	    verbose(2,"%s %s %s\n",
		row[createTimeIx],row[updateTimeIx],row[nameIx]);
	    errAbort("could not parse date %s or %s on table %s\n",
		row[createTimeIx], row[updateTimeIx], row[nameIx]);
	    }
	}
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    timep = mktime(&tm);
    if (timep < dropTime)
	{
	slNameAddHead(&tableNames, row[nameIx]);
	verbose(3,"%s %ld drop %s\n",row[timeIxUsed], (unsigned long)timep,
		row[nameIx]);
	}
    else
	verbose(3,"%s %ld   OK %s\n",row[timeIxUsed], (unsigned long)timep,
		row[nameIx]);
    }
sqlDisconnect(&conn);

if (drop)
    {
    if (tableNames)
	{
	struct slName *el;
	conn = sqlConnect(db);
	for (el = tableNames; el != NULL; el = el->next)
	    {
	    verbose(2,"# drop %s\n", el->name);
	    sqlDropTable(conn, el->name);
	    }
	sqlDisconnect(&conn);
	}
    }
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
age = optionFloat("age", 0.0);
ageTime = (time_t)(age * 60 * 60);	/*	age in seconds	*/
dropTime = timeNow - ageTime;
if (age > 0.0)
    {
    verbose(2,"#	specified age = %f hours = %ld seconds\n", age,
	(unsigned long)ageTime);
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
    verbose(1,"ERROR: specified age %.f must be greater than 0.0\n", age);
    usage();
    }
drop = optionExists("drop");
historyToo = optionExists("historyToo");
db = optionVal("db",db);
verbose(2,"#	drop requested: %s\n", drop ? "TRUE" : "FALSE");
verbose(2,"#	    historyToo: %s\n", historyToo ? "TRUE" : "FALSE");
verbose(2,"#	database: %s\n", db);
dbTrash(db);
return 0;
}

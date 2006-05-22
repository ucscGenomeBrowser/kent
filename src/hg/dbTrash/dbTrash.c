/* dbTrash - drop tables from a database older than specified N hours. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

static char const rcsid[] = "$Id: dbTrash.c,v 1.2 2006/05/22 19:14:59 hiram Exp $";

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
  );
}

static struct optionSpec options[] = {
    {"age", OPTION_FLOAT},
    {"drop", OPTION_BOOLEAN},
    {"db", OPTION_STRING},
    {NULL, 0},
};

static double age = 8.0;
static boolean drop = FALSE;
static char *db = (char *)NULL;

static time_t timeNow = 0;
static time_t ageTime = 0;
static time_t dropTime = 0;

void dbTrash(char *XXX)
/* dbTrash - drop tables from a database older than specified N hours. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct tm *tm;

optionInit(&argc, argv, options);
if (argc < 1)
    usage();
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
dbTrash(argv[1]);
return 0;
}

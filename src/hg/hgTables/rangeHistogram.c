/* ranges - display value ranges and histograms on fields. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: rangeHistogram.c,v 1.2 2004/08/28 23:42:15 kent Exp $";

static void printValueHistogram(char *db, char *table, char *field)
/* Print very simple-minded text histogram. */
{
double maxHist = 60;
double scale = -1.0;
boolean firstTime = TRUE;
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
char query[256];

safef(query, sizeof(query),
   "select %s, count(*) as count from %s group by %s order by count desc",
   field, table, field);
sr = sqlGetResult(conn, query);
hTableStart();
hPrintf("<TR>");
hPrintf("<TH>value</TH>");
hPrintf("<TH>count</TH>");
hPrintf("<TH>graph</TH>");
hPrintf("</TR>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    int count = atoi(row[1]);
    int starCount;
    if (scale < 0)
	scale = (maxHist)/count;
    hPrintf("<TR><TD>%s</TD>", name);
    hPrintf("<TD ALIGN=RIGHT>%d</TD>", count);
    hPrintf("<TD>");
    starCount = round(scale*count);
    if (starCount > 0)
	starOut(stdout, starCount);
    else
        hPrintf("&nbsp;");
    hPrintf("</TD></TR>\n");
    }
// hPrintf("</TABLE>");
hTableEnd();
sqlDisconnect(&conn);
}

void doValueHistogram(char *field)
/* Put up value histogram. */
{
char *db = cartString(cart, hgtaDatabase);
char *table = cartString(cart, hgtaHistoTable);
htmlOpen("Value histogram for %s.%s.%s", db, table, field);
printValueHistogram(db, table, field);
htmlClose();
}

static void printValueRange(char *db, char *table, char *field)
/* Print min/max/mean. */
{
double maxHist = 60;
double scale = -1.0;
boolean firstTime = TRUE;
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
char query[256];

safef(query, sizeof(query),
   "select min(%s), max(%s), avg(%s) from %s", field, field, field, table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<B>min:</B> %s <B>max:</B> %s <B>average:</B> %s\n",
    	row[0], row[1], row[2]);
    }
sqlDisconnect(&conn);
}


void doValueRange(char *field)
/* Put up value histogram. */
{
char *db = cartString(cart, hgtaDatabase);
char *table = cartString(cart, hgtaHistoTable);
htmlOpen("Value range for %s.%s.%s", db, table, field);
printValueRange(db, table, field);
htmlClose();
}


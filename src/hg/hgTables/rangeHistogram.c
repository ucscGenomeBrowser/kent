/* ranges - display value ranges and histograms on fields. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hgTables.h"
#include "bedCart.h"
#include "myVariants.h"


static char *myVariantsHistoWhere(char *table)
/* Return a WHERE fragment limiting a My Annotations table, which holds every
 * assembly, to the one being browsed, or NULL for any other table.  Caller
 * frees. */
{
char *dot = strrchr(table, '.');
return myVariantsScopeWhere(database, (dot != NULL) ? dot + 1 : table);
}

static void printValueHistogram(char *db, char *table, char *field)
/* Print very simple-minded text histogram. */
{
double maxHist = 60;
double scale = -1.0;
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char **row;

#define TOO_BIG_FOR_HISTO 500000
boolean tooBig = (sqlTableSize(conn, table) > TOO_BIG_FOR_HISTO);
if (tooBig)
    {
    hPrintf("This table has too many rows for histogram view");
    hFreeConn(&conn);
    return;
    }

struct dyString *query = sqlDyStringCreate("select `%s`, count(*) as count from %s",
    field, table);
char *where = myVariantsHistoWhere(table);
if (where != NULL)
    sqlDyStringPrintf(query, " where %-s", where);
sqlDyStringPrintf(query, " group by `%s` order by count desc", field);
sr = sqlGetResult(conn, dyStringContents(query));
dyStringFree(&query);
freeMem(where);
hTableStart();
hPrintf("<TR>");
hPrintf("<TH>value</TH>");
hPrintf("<TH>count</TH>");
hPrintf("<TH>graph</TH>");
hPrintf("</TR>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = htmlEncode(naForNull(row[0]));
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
    freeMem(name);
    }
// hPrintf("</TABLE>");
hTableEnd();
hFreeConn(&conn);
}

void doValueHistogram(char *field)
/* Put up value histogram. */
{
char *db = cartString(cart, hgtaDatabase);
char *table = cartString(cart, hgtaHistoTable);
if (myVariantsIsHiddenField(table, field))
    errAbort("No histogram available for field %s", field);
htmlOpen("Value histogram for %s.%s.%s", db, table, field);
printValueHistogram(db, table, field);
htmlClose();
}

static void printValueRange(char *db, char *table, char *field)
/* Print min/max/mean. */
{
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char **row;

struct dyString *query = sqlDyStringCreate("select min(`%s`), max(`%s`), avg(`%s`) from %s",
    field, field, field, table);
char *where = myVariantsHistoWhere(table);
if (where != NULL)
    sqlDyStringPrintf(query, " where %-s", where);
sr = sqlGetResult(conn, dyStringContents(query));
dyStringFree(&query);
freeMem(where);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<B>min:</B> %s <B>max:</B> %s <B>average:</B> %s\n",
    	naForNull(row[0]), naForNull(row[1]), naForNull(row[2]));
    }
hFreeConn(&conn);
}


void doValueRange(char *field)
/* Put up value histogram. */
{
char *db = cartString(cart, hgtaDatabase);
char *table = cartString(cart, hgtaHistoTable);
boolean showItemRgb = FALSE;

if (myVariantsIsHiddenField(table, field))
    errAbort("No range available for field %s", field);
showItemRgb=bedItemRgb(findTdbForTable(db, curTrack, table, ctLookupName));
// should we expect itemRgb instead of "reserved"

if (showItemRgb && sameWord(field, "reserved"))
    htmlOpen("Value range for %s.%s.itemRgb", db, table);
else
    htmlOpen("Value range for %s.%s.%s", db, table, field);
printValueRange(db, table, field);
htmlClose();
}


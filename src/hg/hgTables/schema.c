/* schema - display info about database organization. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "obscure.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "trackDb.h"
#include "joiner.h"
#include "tableDescriptions.h"
#include "asParse.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: schema.c,v 1.12 2004/07/18 18:41:10 kent Exp $";


void describeFields(char *db, char *table, 
	struct asObject *asObj, struct sqlConnection *conn)
/* Print out an HTML table showing table fields and types, and optionally 
 * offering histograms for the text/enum fields. */
{
struct sqlResult *sr;
char **row;
#define TOO_BIG_FOR_HISTO 500000
boolean tooBig = (sqlTableSize(conn, table) > TOO_BIG_FOR_HISTO);
char button[64];
char query[256];

safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);

hTableStart();
hPrintf("<TR> <TH>field</TH> <TH>SQL type</TH> ");
if (!tooBig)
    hPrintf("<TH>info</TH> ");
if (asObj != NULL)
    hPrintf("<TH>description</TH> ");
puts("</TR>\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<TR> <TD><TT>%s</TT></TD> <TD><TT>%s</TT></TD>", row[0], row[1]);
    if (!tooBig)
	{
	hPrintf(" <TD>");
	if (isSqlStringType(row[1]) && !sameString(row[1], "longblob"))
	    {
	    hPrintf("<A HREF=\"../cgi-bin/hgTables");
	    hPrintf("?%s", cartSidUrlString(cart));
	    hPrintf("&%s=%s", hgtaDatabase, db);
	    hPrintf("&%s=%s", hgtaTable, table);
	    hPrintf("&%s=%s", hgtaDoValueHistogram, row[0]);
	    hPrintf("\">");
	    hPrintf("values");
	    hPrintf("</A>");
	    }
	else if (isSqlNumType(row[1]))
	    {
	    hPrintf("<A HREF=\"../cgi-bin/hgTables");
	    hPrintf("?%s", cartSidUrlString(cart));
	    hPrintf("&%s=%s", hgtaDatabase, db);
	    hPrintf("&%s=%s", hgtaTable, table);
	    hPrintf("&%s=%s", hgtaDoValueRange, row[0]);
	    hPrintf("\">");
	    hPrintf("range");
	    hPrintf("</A>");
	    }
	else
	    {
	    hPrintf("&nbsp;");
	    }
	hPrintf("</TD>");
	}
    if (asObj != NULL)
        {
	struct asColumn *asCol = asColumnFind(asObj, row[0]);
	hPrintf(" <TD>");
	if (asCol != NULL)
	    hPrintf("%s", asCol->comment);
	else
	    {
	    if (sameString("bin", row[0]))
	       hPrintf("Indexing field to speed chromosome range queries.");
	    else
		hPrintf("&nbsp;");
	    }
	hPrintf("</TD>");
	}
    puts("</TR>");
    }
hTableEnd();
sqlFreeResult(&sr);
}

static void explainCoordSystem()
/* Our coord system is counter-intuitive to users.  Warn them in advance to 
 * reduce the frequency with which they find this "bug" on their own and 
 * we have to explain it on the genome list. */
{
puts("<P><I>Note: all start coordinates in our database are 0-based, not \n"
     "1-based.  See explanation \n"
     "<A HREF=\"http://genome.ucsc.edu/FAQ/FAQtracks#tracks1\">"
     "here</A>.</I></P>");
}


static void printSampleRows(int sampleCount, struct sqlConnection *conn, char *table)
/* Put up sample values. */
{
char query[256];
struct sqlResult *sr;
char **row;
int i, columnCount = 0;

/* Make table with header row containing name of fields. */
safef(query, sizeof(query), "describe %s", table);
sr = sqlGetResult(conn, query);
hTableStart();
hPrintf("<TR>");
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<TH>%s</TH>", row[0]);
    ++columnCount;
    }
hPrintf("</TR>");
sqlFreeResult(&sr);

/* Get some sample fields. */
safef(query, sizeof(query), "select * from %s limit %d", table, sampleCount);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<TR>");
    for (i=0; i<columnCount; ++i)
        hPrintf("<TD>%s</TD>", row[i]);
    hPrintf("</TR>\n");
    }
sqlFreeResult(&sr);
hTableEnd();
explainCoordSystem();
}


static void showSchema(char *db, char *table)
/* Show schema to open html page. */
{
struct sqlConnection *conn = sqlConnect(db);
struct joiner *joiner = joinerRead("all.joiner");
struct joinerPair *jpList, *jp;
struct asObject *asObj = asForTable(conn, table);
char *splitTable = chromTable(conn, table);

hPrintf("<B>Database:</B> %s ", db);
hPrintf("<B>Primary Table:</B> %s", table);
if (!sameString(splitTable, table))
    hPrintf(" (%s)", splitTable);
hPrintf(" <B>Row Count:</B> ");
printLongWithCommas(stdout, sqlTableSize(conn, splitTable));
hPrintf("<BR>\n");
describeFields(db, splitTable, asObj, conn);

jpList = joinerRelate(joiner, db, table);
if (jpList != NULL)
    {
    webNewSection("Connected Tables and Joining Fields");
    for (jp = jpList; jp != NULL; jp = jp->next)
	{
	hPrintSpaces(6);
	hPrintf("%s.", jp->b->database);
	hPrintf("<A HREF=\"../cgi-bin/hgTables?");
	hPrintf("%s&", cartSidUrlString(cart));
	hPrintf("%s=%s&", hgtaDoSchemaDb, jp->b->database);
	hPrintf("%s=%s", hgtaDoSchema, jp->b->table);
	hPrintf("\">");
	hPrintf("%s", jp->b->table);
	hPrintf("</A>");
	hPrintf(".%s (via %s)<BR>\n", 
	    jp->b->field, jp->a->field);
	}
    }

webNewSection("Sample Rows");
printSampleRows(10, conn, splitTable);
}

void doTableSchema(char *db, char *table, struct sqlConnection *conn)
/* Show schema around table. */
{
htmlOpen("Schema for %s", table);
showSchema(db, table);
htmlClose();
}

void doTrackSchema(struct trackDb *track, struct sqlConnection *conn)
/* Show schema around track. */
{
char *table = connectingTableForTrack(track);
htmlOpen("Schema for %s - %s", track->shortLabel, track->longLabel);
showSchema(database, table);
htmlClose();
}


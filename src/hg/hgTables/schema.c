/* schema - display info about database organization. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "trackDb.h"
#include "grp.h"
#include "joiner.h"
#include "tableDescriptions.h"
#include "hgColors.h"
#include "asParse.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: schema.c,v 1.6 2004/07/14 06:33:26 kent Exp $";


boolean isSqlStringType(char *type)
/* Return TRUE if it a a stringish SQL type. */
{
return strstr(type, "char") || strstr(type, "text") || strstr(type, "blob") ;
}

boolean isSqlNumType(char *type)
/* Return TRUE if it is a numerical SQL type. */
{
return strstr(type, "int") || strstr(type, "float") || strstr(type, "double");
}

static struct asColumn *asColumnFind(struct asObject *asObj, char *name)
/* Return named column. */
{
struct asColumn *asCol = NULL;
if (asObj!= NULL)
    {
    for (asCol = asObj->columnList; asCol != NULL; asCol = asCol->next)
        if (sameString(asCol->name, name))
	     break;
    }
return asCol;
}

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

/* For some reason BORDER=1 does not work in our web.c nested table scheme.
 * So use web.c's trick of using an enclosing table to provide a border.   */
puts("<!--outer table is for border purposes-->" "\n"
     "<TABLE BGCOLOR=\"#"HG_COL_BORDER"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>");
puts("<TABLE BORDER=\"1\" BGCOLOR=\""HG_COL_INSIDE"\" CELLSPACING=\"0\">");
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
	if ((isSqlStringType(row[1]) || startsWith("enum", row[1])) &&
	    ! sameString(row[1], "longblob"))
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
	    hPrintf("&nbsp;");
	hPrintf("</TD>");
	}
    puts("</TR>");
    }
puts("</TABLE>");
puts("</TR></TD></TABLE>");
sqlFreeResult(&sr);
}

struct asObject *asForTable(struct sqlConnection *conn, char *table)
/* Get autoSQL description if any associated with table. */
{
struct asObject *asObj = NULL;
if (sqlTableExists(conn, "tableDescriptions"))
    {
    char query[256];
    char *asText = NULL;

    /* Try split table first. */
    safef(query, sizeof(query), 
    	"select autoSqlDef from tableDescriptions where tableName='chrN_%s'",
	table);
    asText = sqlQuickString(conn, query);

    /* If no result try unsplit table. */
    if (asText == NULL)
	{
	safef(query, sizeof(query), 
	    "select autoSqlDef from tableDescriptions where tableName='%s'",
	    table);
	asText = sqlQuickString(conn, query);
	}
    if (asText != NULL && asText[0] != 0)
	asObj = asParseText(asText);
    freez(&asText);
    }
return asObj;
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
hPrintf("<B>Primary Table:</B> %s<BR>", table);
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
char *table = track->tableName;
table = connectingTableForTrack(track);
htmlOpen("Schema for %s - %s", track->shortLabel, track->longLabel);
showSchema(database, table);
htmlClose();
}


#include "common.h"
#include "linefile.h"
#include "hash.h"
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
#include "hgTables.h"

static char const rcsid[] = "$Id: schema.c,v 1.3 2004/07/14 00:39:31 kent Exp $";

boolean showTableDescriptions(struct sqlConnection *conn, char *table)
/* Display autoSql definition and gbdDescriptions link for table, 
 * if available. */
{
boolean gotInfo = FALSE;
static char *asTableName = "tableDescriptions";

if (sqlTableExists(conn, asTableName))
    {
    struct sqlResult *sr = NULL;
    struct tableDescriptions *asi = NULL;
    char query[512];
    char **row = NULL;
    safef(query, sizeof(query), "select * from %s where tableName = '%s'",
	  asTableName, table);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	asi = tableDescriptionsLoad(row);
	gotInfo = TRUE;
	if (asi->autoSqlDef != NULL && asi->autoSqlDef[0] != 0)
	    {
	    hPrintf("<BR>\n");
	    hPrintf("<B><A HREF=\"http://www.linuxjournal.com/article.php?sid=5949\" TARGET=_BLANK>");
	    hPrintf("AutoSql</A> field definition:</B>");
	    hPrintf("<PRE><TT>");
	    hPrintf(asi->autoSqlDef);
	    hPrintf("</TT></PRE>");
	    }
	if (asi->gbdAnchor != NULL && asi->gbdAnchor[0] != 0)
	    {
	    hPrintf("<A HREF=\"/goldenPath/gbdDescriptions.html#%s\" TARGET=_BLANK>",
		   asi->gbdAnchor);
	    hPrintf("Genome Browser Database Description for %s</A>", table);
	    }
	}
    sqlFreeResult(&sr);
    }
return(gotInfo);
}

static void showSchema(struct cart *cart,
	char *db, char *table, struct sqlConnection *conn)
/* Show schema to open html page. */
{
struct joiner *joiner = joinerRead("all.joiner");
struct hTableInfo *hti = hFindTableInfo(NULL, table);
struct joinerPair *jpList, *jp;
hPrintf("<B>Database:</B> %s ", db);
hPrintf("<B>Primary Table:</B> %s<BR>", table);
showTableDescriptions(conn, table);

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
	hPrintf(".%s (via %s.%s.<B>%s</B> field)<BR>\n", 
	    jp->b->field, jp->a->database, jp->a->table, jp->a->field);
	}
    }
}

void doTableSchema(struct cart *cart,
	char *db, char *table, struct sqlConnection *conn)
/* Show schema around table. */
{
htmlOpen("Schema for %s", table);
showSchema(cart, db, table, conn);
htmlClose();
}

void doTrackSchema(struct cart *cart,
	struct trackDb *track, struct sqlConnection *conn)
/* Show schema around track. */
{
char *table = connectingTableForTrack(track);
htmlOpen("Schema for %s - %s", track->shortLabel, track->longLabel);
showSchema(cart, database, table, conn);
htmlClose();
}


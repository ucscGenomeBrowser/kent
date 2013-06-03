/* useCount - a simple CGI that merely counts its references. */
#include "common.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "hdb.h"


/* table to use for counting in hgcentral */
static char useCount[] = "useCount";

int main(int argc, char *argv[])
{
int count = 0;
cgiSpoof(&argc, argv);

char dateTime[256];
char *remoteAddr = getenv("REMOTE_ADDR");
char *userAgent = getenv("HTTP_USER_AGENT");
char *version = cgiUsualString("version", "unknown");
if (remoteAddr == NULL)
    remoteAddr = "unknown";
if (userAgent == NULL)
    userAgent = "unknown";
/* protect against huge strings coming in from outside */
char safeAgent[255];
snprintf(safeAgent, sizeof(safeAgent), "%s", userAgent);
char safeAddr[255];
snprintf(safeAddr, sizeof(safeAddr), "%s", remoteAddr);
char safeVersion[255];
snprintf(safeVersion, sizeof(safeVersion), "%s", version);

printf("Content-Type:text/html\n\n\n");
printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
printf("<HTML><HEAD>\n");

struct sqlConnection *conn = hConnectCentral();
if (conn)
    {
    char query[1024];
    if (sqlTableExists(conn, useCount))
	{
	sqlSafef(query, sizeof(query), "INSERT %s VALUES(0,\"%s\",\"%s\",now(),\"%s\")",
            useCount, safeAgent, safeAddr, safeVersion);
        sqlUpdate(conn,query);
	count = sqlLastAutoId(conn);
	sqlSafef(query, sizeof(query), "SELECT dateTime FROM %s WHERE count=%d",
	    useCount, count);
	(void) sqlQuickQuery(conn, query, dateTime, sizeof(dateTime));
	}
    else
	{
	printf("ERROR: can not find table '%s'<BR>\n", useCount);
	}
    hDisconnectCentral(&conn);
    }

printf("count: %d, date: %s<BR>\n", count, dateTime);
printf("</HEAD></HTML>\n");
return 0;
}

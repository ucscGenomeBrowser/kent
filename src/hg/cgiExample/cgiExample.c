/* cgiExample - An example cgi program that uses mySQL. */
#include "common.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "jksql.h"
#include "knownInfo.h"


char *lookupName(struct sqlConnection *conn, char *table, unsigned id)
/* Look up name based on numerical id in given table.  Return "n/a" if
 * not found. The return value is overwritten by the next call to this
 * routine. */
{
char query[256];
static char buf[256];
char *result;

sqlSafef(query, sizeof query, "select name from %s where id = %u", table, id);
result = sqlQuickQuery(conn, query, buf, sizeof(buf));
if (result == NULL)
    result = "n/a";
return result;
}

void doMiddle()
/* Print middle parts of web page.  Get database and transcript
 * ID from CGI, and print info about that transcript. */
{
char *transId = cgiString("transId");
char *db = cgiString("db");
struct knownInfo *ki, *kiList = NULL;
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr;
char **row;
char query[256];

/* Get a list of all that have that ID. */
sqlSafef(query, sizeof query, "select * from knownInfo where transId = '%s'", transId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ki = knownInfoLoad(row);
    slAddHead(&kiList, ki);
    }
sqlFreeResult(&sr);
slReverse(&kiList);

/* Print title that says how many match. */
printf("<H2>Transcript %s - %d match</H2>\n", transId, slCount(kiList));

/* Print some info for each match */
for (ki = kiList; ki != NULL; ki = ki->next)
    {
    printf("<B>geneId</B>: %s<BR>\n", ki->geneId);
    printf("<B>geneName</B>: %s<BR>\n", 
    	lookupName(conn, "geneName", ki->geneName));
    /*  ~~~ Todo: fill in other info.  ~~~ */
    }

knownInfoFreeList(&kiList);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (!cgiIsOnWeb())
   {
   warn("This is a CGI script - attempting to fake environment from command line");
   cgiSpoof(&argc, argv);
   }
htmShell("CGI Example", doMiddle, NULL); 
return 0;
}

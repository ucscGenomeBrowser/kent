/* demog - do Demographic section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "hgSubj.h"
#include "hdb.h"
#include "net.h"

static char const rcsid[] = "$Id: demog.c,v 1.1 2006/08/21 22:04:56 fanhsu Exp $";

static boolean demogExists(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Return TRUE if demogAll table exists and it has an entry with the gene symbol */
{
if (sqlTableExists(conn, "gsSubjInfo") == TRUE)
    {
    return(TRUE);
    }
return(FALSE);
}

static void demogPrint(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Print out GAD section. */
{
char *gender, *age, *race;
char *location;

char query[256];
struct sqlResult *sr;
char **row;
char *riskFactor;

printf("<TABLE>");
safef(query, sizeof(query), 
      "select gender, age, race, geography, riskFactor from gsSubjInfo where subjId='%s'", 
      subjId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
    
if (row != NULL) 
    {
    gender     = row[0];
    age        = row[1];
    race       = row[2];
    location   = row[3];
    riskFactor = row[4];
    
    printf("<TR>");
    printf("<TD>");
    printf("<B>gender:</B> %s%s", gender, GSBLANKS);
    printf("</TD>");
    printf("<TD>");
    printf("<B>age:</B> %s%s", age, GSBLANKS);
    printf("</TD>");
    printf("<TD>");
    printf("<B>risk factor:</B> %s%s", riskFactor, GSBLANKS);
    printf("</TD>");
    printf("</TR>");
    
    printf("<TR>");
    printf("<TD>");
    printf("<B>race:</B> %s%s\n", race, GSBLANKS);
    printf("</TD>");
    printf("<TD>");
    printf("<B>location:</B> %s\n", location);
    printf("</TD>");
    printf("</TR>");
    }
printf("</TABLE>");

sqlFreeResult(&sr);
hFreeConn(&conn);

return;
}

struct section *demogSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create demog section. */
{
struct section *section = sectionNew(sectionRa, "demog");
section->exists = demogExists;
section->print = demogPrint;
return section;
}


/* vaccine - do Vacceine and HIV Status section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "hgSubj.h"
#include "hdb.h"
#include "net.h"

static char const rcsid[] = "$Id: vaccine.c,v 1.2 2006/08/30 21:56:22 fanhsu Exp $";

static boolean vaccineExists(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Return TRUE if vaccineAll table exists and it has an entry with the gene symbol */
{
if (sqlTableExists(conn, "gsSubjInfo") == TRUE)
    {
    return(TRUE);
    }
return(FALSE);
}

static void vaccinePrint(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Print out Vaccine section. */
{
char *date1stInject, *immunStatus, *dateInfect;

char query[256];
struct sqlResult *sr;
char **row;

printf("<TABLE>");

safef(query, sizeof(query), "select date1stInject, immunStatus, dateInfect from gsSubjInfo where subjId='%s'", subjId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
    
if (row != NULL) 
    {
    date1stInject   = row[0];
    immunStatus      = row[1];
    dateInfect     = row[2];
    printf("<TR>");
    printf("<TD>");
    printf("<B>Initial Vaccine:</B> %s%s", date1stInject, GSBLANKS);
    printf("</TD>");
    printf("<TD>");
    printf("<B>Vaccine/Placebo:</B> %s%s", immunStatus, GSBLANKS);
    printf("</TD>");
    printf("</TR>");
    
    printf("<TR>");
    printf("<TD>");
    printf("<B>HIV Status:</B> %s%s", "data missing", GSBLANKS);
    printf("</TD>");
    printf("<TD>");
    printf("<B>Estimated Infection Date:</B> %s\n", dateInfect);
    printf("</TD>");
    printf("</TR>");
    }
printf("</TABLE>");
sqlFreeResult(&sr);
hFreeConn(&conn);

return;
}

struct section *vaccineSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create vaccine section. */
{
struct section *section = sectionNew(sectionRa, "vaccine");
section->exists = vaccineExists;
section->print = vaccinePrint;
return section;
}


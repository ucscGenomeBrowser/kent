/* vaccine - do Vacceine and HIV Status section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "gsidSubj.h"
#include "hdb.h"
#include "net.h"

static char const rcsid[] = "$Id: vaccine.c,v 1.11 2008/09/15 20:40:34 fanhsu Exp $";

static boolean vaccineExists(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Return TRUE if vaccineAll table exists and it has an entry with the gene symbol */
{
if (sqlTableExists(conn, "gsidSubjInfo") == TRUE)
    {
    return(TRUE);
    }
return(FALSE);
}

static void vaccinePrint(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Print out Vaccine section. */
{
char *immunStatus;
char *daysInfectF, *daysInfectL;
char *injections;

char query[256];
struct sqlResult *sr;
char **row;
printf("<TABLE>");

safef(query, sizeof(query), 
      "select immunStatus, daysInfectF, daysInfectL, injections from gsidSubjInfo where subjId='%s'", subjId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
    
if (row != NULL) 
    {
    immunStatus  = row[0];
    daysInfectF	 = row[1];
    daysInfectL  = row[2];
    injections   = row[3];

    printf("<TR>");
    printf("<TD>");
    printf("<B>Vaccine/Placebo:</B> %s%s", immunStatus, GSBLANKS);
    printf("</TD>");
    
    printf("<TD>");
    printf("<B>Estimated Study Day of Infection (ESDI)*:</B> %s\n", 
	   daysInfectF);
    printf("</TD>");

    printf("</TR>");
    
    printf("<TR>");
    printf("<TD>");

    /* !!! currently all subjects in the database are infected. */
    /* update this when non-infected subjects addded into DB */

    printf("<B>HIV Status:</B> %s%s", "Infected", GSBLANKS);
    printf("</TD>");

    /* remove the following item per GSID user feedback */
    /*printf("<TD>");
    printf("<B>Days of infection relative to last negative date:</B> %s\n", 
	   daysInfectL);
    printf("</TD>");
    */
    printf("</TR>");
    
    printf("<TR>");
    printf("<TD>");
    printf("<B>Injections:</B> &nbsp;%s%s", injections, GSBLANKS);
    printf("</TD>");
    printf("</TR>");
    }
printf("</TABLE>");
sqlFreeResult(&sr);

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


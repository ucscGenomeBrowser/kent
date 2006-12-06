/* clinical - do Clinical section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "gsidSubj.h"
#include "hdb.h"
#include "net.h"

static char const rcsid[] = "$Id: clinical.c,v 1.2 2006/12/06 18:38:51 fanhsu Exp $";

static boolean clinicalExists(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Return TRUE if clinicalAll table exists and it has an entry with the gene symbol */
{
if (sqlTableExists(conn, "gsClinicRec") == TRUE)
    {
    return(TRUE);
    }
return(FALSE);
}

static void clinicalPrint(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Print out Clinical section. */
{
char bigQuery[2000];

struct sqlResult *sr;
char **row;
char *specimenId, *labCode, *daysCollection, *hivQuan, *cd4Count;

printf("<TABLE BGCOLOR=#222222 CELLSPACING=1 CELLPADDING=3><TR>\n");

printf("<TR>\n");
printf("<TD align=center BGCOLOR=\"#8686D1\"><FONT COLOR=\"#FFFFFF\"><B>Days<BR>since infection</B></FONT></TD>\n");
printf("<TD align=center BGCOLOR=\"#8686D1\"><FONT COLOR=\"#FFFFFF\"><B>HIV-1 RNA<BR>copies/mL</B></FONT></TD>\n");
printf("<TD align=center BGCOLOR=\"#8686D1\"><FONT COLOR=\"#FFFFFF\"><B>CD4<BR>absolute count</B></FONT></TD>\n");
printf("</TR>\n");

/* complex query to ensure date is correctly sorted */
safef(bigQuery, sizeof(bigQuery), 
"select specimenId, labCode, daysCollection, hivQuan, cd4Count from gsidClinicRec where subjId='%s' order by daysCollection",
subjId);

sr = sqlMustGetResult(conn, bigQuery);
row = sqlNextRow(sr);
    
while (row != NULL) 
    {
    specimenId     = row[0];
    labCode        = row[1];
    daysCollection = row[2];
    hivQuan        = row[3];
    cd4Count       = row[4];
    
    printf("<TR>");
    printf("<TD align=right BGCOLOR=\"#D9F8E4\">%s</TD>\n", daysCollection);
    printf("<TD align=right BGCOLOR=\"#D9F8E4\">%s</TD>\n", hivQuan);
    printf("<TD align=right BGCOLOR=\"#D9F8E4\">%s</TD>\n", cd4Count);
    printf("</TR>");
    
    row = sqlNextRow(sr);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printf("</TR></TABLE>");
return;
}

struct section *clinicalSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create clinical section. */
{
struct section *section = sectionNew(sectionRa, "clinical");
section->exists = clinicalExists;
section->print = clinicalPrint;
return section;
}


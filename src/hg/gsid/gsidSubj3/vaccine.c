/* vaccine - do Vacceine and HIV Status section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "gsidSubj3.h"
#include "hdb.h"
#include "net.h"


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

void vxprintf(char *templ, char *string)
/* print "N/A" if the string to be printed is NULL */
{
if (string == NULL) 
    printf(templ, "N/A");
else
    printf(templ, string);
}

static void vaccinePrint(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Print out Vaccine section. */
{
char *immunStatus = NULL;
char *daysInfectF = NULL;
char *daysInfectL = NULL;
char *injections  = NULL;

char *startDate       = NULL;
char *lastSeroNegDay  = NULL;
char *firstSeroPosDay = NULL;
char *firstRNAPosDay  = NULL;
char *ESDBasis	      = NULL;
char *seqDay	      = NULL;

char *art_daei        = NULL;
char *art_sequencing  = NULL;
 
char query[256];
struct sqlResult *sr;
char **row;

printf("<TABLE>");

sqlSafef(query, sizeof(query), 
      "select art_daei, art_sequencing from hgFixed.artDaei where subjId='%s'", 
      subjId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL) 
    {
    art_daei       = cloneString(row[0]);
    art_sequencing = cloneString(row[1]);
    }
sqlFreeResult(&sr);

sqlSafef(query, sizeof(query), 
      "select immunStatus, daysInfectF, daysInfectL, injections from gsidSubjInfo where subjId='%s'", 
      subjId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL) 
    {
    immunStatus  = cloneString(row[0]);
    daysInfectF	 = cloneString(row[1]);
    daysInfectL  = cloneString(row[2]);
    injections   = cloneString(row[3]);
    }
sqlFreeResult(&sr);

sqlSafef(query, sizeof(query), 
      "select startDate,lastSeroNegDay,firstSeroPosDay,firstRNAPosDay,ESDBasis,seqDay from hgFixed.testDates where subjId='%s'", subjId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL) 
    {
    startDate 	    = cloneString(row[0]);
    lastSeroNegDay  = cloneString(row[1]);
    firstSeroPosDay = cloneString(row[2]);
    firstRNAPosDay  = cloneString(row[3]);
    ESDBasis        = cloneString(row[4]);
    seqDay    	    = cloneString(row[5]);
    }
sqlFreeResult(&sr);

printf("<TR>");
printf("<TD>");
printf("<B>Vaccine/Placebo:</B> %s%s", immunStatus, GSBLANKS);
printf("</TD>");
    
printf("<TD>");
vxprintf("<B>ART at Sequencing?</B> %s", art_sequencing);
printf("</TD>");
printf("</TR>");

printf("<TR>");
printf("<TD>");
/* !!! currently all subjects in the database are infected. */
/* update this when non-infected subjects addded into DB */

printf("<B>HIV Status:</B> %s%s", "Infected", GSBLANKS);
printf("</TD>");
    
printf("<TD>");
vxprintf("<B>DAEI for ART Start:</B> %s", art_daei);
printf("</TD>");
printf("</TR>");

/* removed the following item per GSID user feedback */
/*printf("<TD>");
printf("<B>Days of infection relative to last negative date:</B> %s\n", 
       daysInfectL);
printf("</TD>");
*/
    
printf("<TR>");
printf("<TD>");
printf("<B>Injections:</B> &nbsp;%s%s", injections, GSBLANKS);
printf("</TD>");
    
printf("<TD>");

if (sameWord(lastSeroNegDay, "-1"))
    printf("<B>Study Day of Last Serology-Negative HIV Test: </B> %s\n", 
	   "N/A");
else
    vxprintf("<B>Study Day of Last Serology-Negative HIV Test: </B> %s\n", 
	     lastSeroNegDay);
printf("</TD>");
printf("</TR>");

printf("<TR>");
printf("<TD>");
vxprintf("<B>Date of Study Start:</B> %s\n", 
       startDate);
printf("</TD>");

printf("<TD>");
vxprintf("<B>Study Day of First Serology-Positive HIV Test:</B> %s\n", 
       firstSeroPosDay);
printf("</TD>");
printf("</TR>");

printf("<TR>");
printf("<TD>");
vxprintf("<B>Estimated Study Day of Infection (ESDI)*:</B> %s\n", 
	daysInfectF);
printf("</TD>");

printf("<TD>");
vxprintf("<B>Study Day of First RNA-positive HIV Test (NAT):</B> %s\n", firstRNAPosDay);
printf("</TD>");
printf("</TR>");

printf("<TR>");
printf("<TD>");
if (sameWord(seqDay, "-1"))
    printf("<B>Study Day of Sequencing:</B> N/A\n");
else
    vxprintf("<B>Study Day of Sequencing:</B> %s\n", seqDay);
printf("</TD>");

printf("<TD>");
vxprintf("<B>Basis for ESDI:</B> %s\n", 
        ESDBasis);
printf("</TD>");
printf("</TR>");

printf("</TABLE>");
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


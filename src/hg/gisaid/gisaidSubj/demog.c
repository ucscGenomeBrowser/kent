/* demog - do Demographic section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "gisaidSubj.h"
#include "hdb.h"
#include "net.h"


static boolean demogExists(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Return TRUE if demogAll table exists and it has an entry with the gene symbol */
{
if (sqlTableExists(conn, "gisaidSubjInfo") == TRUE)
    {
    return(TRUE);
    }
return(FALSE);
}

static void display1(struct sqlConnection *conn, char *subjId, char* colName)
{
char query[256];
struct sqlResult *sr;
char **row;

sqlSafef(query, sizeof(query), 
      "select %s from gisaidSubjInfo where subjId='%s'", 
      colName, subjId);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
    
if (row != NULL) 
    {
    printf("<B>%s:</B> %s<BR>\n", colName, row[0]);
    fflush(stdout);
    }
sqlFreeResult(&sr);
}

static void demogPrint(struct section *section, 
	struct sqlConnection *conn, char *subjId)
/* Print out GAD section. */
{
printf("<H3>Subject: %s</H3>\n", subjId);
display1(conn, subjId, "EPI_ISOLATE_ID");
display1(conn, subjId, "EPI_SEQUENCE_ID");
//display1(conn, subjId, "subjId");
display1(conn, subjId, "TYPE");
display1(conn, subjId, "PASSAGE");
display1(conn, subjId, "COLLECT_DATE");
display1(conn, subjId, "HOST");
display1(conn, subjId, "LOCATION");
display1(conn, subjId, "NOTES");
display1(conn, subjId, "UPDATE_DATE");
display1(conn, subjId, "ISOLATE_SUBMITTER");
display1(conn, subjId, "SAMPLE_LAB");
display1(conn, subjId, "SEQUENCE_LAB");
display1(conn, subjId, "IV_ANIMAL_VACCIN_PRODUCT");
display1(conn, subjId, "RESIST_TO_ADAMANTANES");
display1(conn, subjId, "RESIST_TO_OSELTAMIVIR");
display1(conn, subjId, "RESIST_TO_ZANAMIVIR");
display1(conn, subjId, "RESIST_TO_PERAMIVIR");
display1(conn, subjId, "RESIST_TO_OTHER");
display1(conn, subjId, "H1N1_SWINE_SET");
display1(conn, subjId, "LAB_CULTURE");
display1(conn, subjId, "IS_COMPLETE");
display1(conn, subjId, "IV_SAMPLE_ID");
display1(conn, subjId, "DATE_SELECTED_FOR_VACCINE");
display1(conn, subjId, "PROVIDER_SAMPLE_ID");
display1(conn, subjId, "ANTIGEN_CHARACTER");
display1(conn, subjId, "PATHOGEN_TEST_INFO");
display1(conn, subjId, "ANTIVIRAL_RESISTANCE");
display1(conn, subjId, "AUTHORS");
display1(conn, subjId, "SEQLAB_SAMPLE_ID");
display1(conn, subjId, "IS_VACCINATED");
display1(conn, subjId, "HUMAN_SPECIMEN_SOURCE");
display1(conn, subjId, "ANIMAL_SPECIMEN_SOURCE");
display1(conn, subjId, "ANIMAL_HEALTH_STATUS");
display1(conn, subjId, "ANIMAL_DOMESTIC_STATUS");
display1(conn, subjId, "SOURCE_NAME");
display1(conn, subjId, "GEOPLACE_NAME");
display1(conn, subjId, "HOST_AGE");
display1(conn, subjId, "HOST_GENDER");
display1(conn, subjId, "ZIP_CODE");
display1(conn, subjId, "PATIENT_STATUS");
display1(conn, subjId, "ANTIVIRAL_TREATMENT");
display1(conn, subjId, "OUTBREAK");
display1(conn, subjId, "VACCINATION_LAST_YEAR");
display1(conn, subjId, "PATHOGENICITY");
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


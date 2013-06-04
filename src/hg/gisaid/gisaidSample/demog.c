/* demog - do Demographic section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "gisaidSample.h"
#include "hdb.h"
#include "net.h"


static boolean demogExists(struct section *section, 
	struct sqlConnection *conn, char *sampleId)
/* Return TRUE if demogAll table exists and it has an entry with the gene symbol */
{
if (sqlTableExists(conn, "gisaidSubjInfo") == TRUE)
    {
    return(TRUE);
    }
return(FALSE);
}

static void display1(struct sqlConnection *conn, char *sampleId, char* colName)
{
char query[256];
struct sqlResult *sr;
char **row;

sqlSafef(query, sizeof(query), 
      "select %s from gisaidSubjInfo where EPI_ISOLATE_ID='%s'", 
      colName, sampleId);
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
	struct sqlConnection *conn, char *sampleId)
/* Print out GAD section. */
{
printf("<H3>Sample (EPI_ISOLATE_ID): %s</H3>\n", sampleId);
//display1(conn, sampleId, "EPI_ISOLATE_ID");
display1(conn, sampleId, "subjId");
display1(conn, sampleId, "EPI_SEQUENCE_ID");
display1(conn, sampleId, "TYPE");
display1(conn, sampleId, "PASSAGE");
display1(conn, sampleId, "COLLECT_DATE");
display1(conn, sampleId, "HOST");
display1(conn, sampleId, "LOCATION");
display1(conn, sampleId, "NOTES");
display1(conn, sampleId, "UPDATE_DATE");
display1(conn, sampleId, "ISOLATE_SUBMITTER");
display1(conn, sampleId, "SAMPLE_LAB");
display1(conn, sampleId, "SEQUENCE_LAB");
display1(conn, sampleId, "IV_ANIMAL_VACCIN_PRODUCT");
display1(conn, sampleId, "RESIST_TO_ADAMANTANES");
display1(conn, sampleId, "RESIST_TO_OSELTAMIVIR");
display1(conn, sampleId, "RESIST_TO_ZANAMIVIR");
display1(conn, sampleId, "RESIST_TO_PERAMIVIR");
display1(conn, sampleId, "RESIST_TO_OTHER");
display1(conn, sampleId, "H1N1_SWINE_SET");
display1(conn, sampleId, "LAB_CULTURE");
display1(conn, sampleId, "IS_COMPLETE");
display1(conn, sampleId, "IV_SAMPLE_ID");
display1(conn, sampleId, "DATE_SELECTED_FOR_VACCINE");
display1(conn, sampleId, "PROVIDER_SAMPLE_ID");
display1(conn, sampleId, "ANTIGEN_CHARACTER");
display1(conn, sampleId, "PATHOGEN_TEST_INFO");
display1(conn, sampleId, "ANTIVIRAL_RESISTANCE");
display1(conn, sampleId, "AUTHORS");
display1(conn, sampleId, "SEQLAB_SAMPLE_ID");
display1(conn, sampleId, "IS_VACCINATED");
display1(conn, sampleId, "HUMAN_SPECIMEN_SOURCE");
display1(conn, sampleId, "ANIMAL_SPECIMEN_SOURCE");
display1(conn, sampleId, "ANIMAL_HEALTH_STATUS");
display1(conn, sampleId, "ANIMAL_DOMESTIC_STATUS");
display1(conn, sampleId, "SOURCE_NAME");
display1(conn, sampleId, "GEOPLACE_NAME");
display1(conn, sampleId, "HOST_AGE");
display1(conn, sampleId, "HOST_GENDER");
display1(conn, sampleId, "ZIP_CODE");
display1(conn, sampleId, "PATIENT_STATUS");
display1(conn, sampleId, "ANTIVIRAL_TREATMENT");
display1(conn, sampleId, "OUTBREAK");
display1(conn, sampleId, "VACCINATION_LAST_YEAR");
display1(conn, sampleId, "PATHOGENICITY");
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


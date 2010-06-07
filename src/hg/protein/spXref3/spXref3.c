/* spXref3 - get xref data of proteins in SWISS-PROT, TrEMBL, TrEMBL-NEW and HUGO
   and create spXref3.tab file to be imported into proteinxxxx database */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spXref3 - get xref data of proteins in SWISS-PROT, TrEMBL, TrEMBL-NEW and HUGO.\n"
  "          Output is placed in file spXref3.tab.\n"
  "usage:\n"
  "   spXref3 xxxx \n"
  "      xxxx is the release date of SWISS-PROT data\n"
  "example: spXref3 070403\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3;
char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;

char *hugoSymbol;
char *hugoDesc;
    
char *chp;
char *proteinDataDate;

char *bioentry_id;
char *biodatabase_id;
char *display_id;
char *accession;
char *entry_version;
char *division;     
char *description;

char cond_str[100]; 
char *ontology_term_id;

char proteinDatabaseName[40];    
FILE *o1;
char empty_str[1] = {""};

if (argc != 2) usage();
proteinDataDate = argv[1];

o1 = fopen("spXref3.tab", "w");

conn = hAllocConn(hDefaultDb());
conn2= hAllocConn(hDefaultDb());
conn3= hAllocConn(hDefaultDb());
   
sprintf(proteinDatabaseName, "biosql%s", proteinDataDate);
sprintf(cond_str, "term_name = 'description'");
ontology_term_id = sqlGetField(proteinDatabaseName, "ontology_term", 
                               "ontology_term_id", cond_str); 

sprintf(proteinDatabaseName, "proteins%s", proteinDataDate);
sprintf(query2,"select * from biosql%s.bioentry;", proteinDataDate);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    bioentry_id 	= row2[0];    
    biodatabase_id 	= row2[1]; 
    display_id		= row2[2];     
    accession		= row2[3];      
    entry_version	= row2[4];  
    division		= row2[5];
			       
    sprintf(query,
	    "select qualifier_value from biosql%s.bioentry_qualifier_value where bioentry_id=%s and ontology_term_id=%s;",
	    proteinDataDate, bioentry_id, ontology_term_id);

    sprintf(cond_str, "swissprot='%s'", accession);
    hugoSymbol = sqlGetField(proteinDatabaseName, "hugo", "symbol", cond_str);
    hugoDesc = sqlGetField(proteinDatabaseName, "hugo", "name", cond_str);

    if (hugoSymbol==NULL) hugoSymbol = empty_str;
    if (hugoDesc==NULL)   hugoDesc   = empty_str;

    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    while (row != NULL)
    	{
    	description = row[0];
	chp = strstr(description, "(");
	if (chp != NULL)
	    {
	    *chp = '\0';
	    }
	fprintf(o1, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
		accession, 
		display_id,  
		division,
		bioentry_id, 
		biodatabase_id,
		description,
		hugoSymbol,
		hugoDesc
	       );
	row = sqlNextRow(sr);
	}
    sqlFreeResult(&sr);
    row2 = sqlNextRow(sr2);
    }
fclose(o1);
sqlFreeResult(&sr2);
hFreeConn(&conn);
hFreeConn(&conn2);
hFreeConn(&conn3);
return(0);
}

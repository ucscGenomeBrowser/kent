/* getallpep gets all pep sequence from biosqlxxxx database */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getallpep - get all pep sequence from biosqlxxxx\n"
  "usage:\n"
  "   getallpep xxxx\n"
  "             xxxx is the release date of biosql database\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3, *conn4;
char query2[256], query3[256], query4[256];
struct sqlResult *sr2, *sr3, *sr4;
char **row2, **row3, **row4;

FILE *o3;
char *chp;

char *proteinDataDate;

int maxlen = {0};
int len;

char *bioentry_id;
char *biodatabase_id;
char *display_id;
char *accession;
char *division;
char *biosequence_str;
  
char *desc, *desc2;
char *genenames = NULL;
char *ontology_term_id;
char *qualifier_value;

if (argc != 2) usage();
proteinDataDate = argv[1];

conn2= hAllocConn();
conn3= hAllocConn();
conn4= hAllocConn();
    
o3 = fopen("allPep.tab", "w");
    
sqlSafef(query3, sizeof query3, "select * from biosql%s.bioentry;", proteinDataDate);

sr3 = sqlMustGetResult(conn3, query3);
row3 = sqlNextRow(sr3);
	      
while (row3 != NULL)
    {
    bioentry_id 	= row3[0];
    biodatabase_id  = row3[1];
    display_id 	= row3[2];
    accession 	= row3[3];
        
    division 	= row3[5];
    	
    sqlSafef(query2, sizeof query2, "select * from biosql%s.biosequence where bioentry_id='%s';", 
	           proteinDataDate, bioentry_id);
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    if (row2 != NULL)
	{
 	biosequence_str = row2[4];
		
	len = strlen(biosequence_str);
	if (maxlen < len) maxlen = len;
	}
		
    sqlSafef(query4, sizeof query4,
	    "select * from biosql%s.bioentry_qualifier_value where bioentry_id='%s';",
	    proteinDataDate, bioentry_id);
    
    genenames="";

    desc  = "";
    desc2 = "";
	
    sr4  = sqlMustGetResult(conn4, query4);
    row4 = sqlNextRow(sr4);
    if (row4 != NULL)
	{
	ontology_term_id= row4[1];
	qualifier_value = row4[2];

	if (strcmp(ontology_term_id, "10") == 0)
	    {
	    desc = qualifier_value;
	    }
	chp = strstr(desc, "(");
	if (chp != NULL)
	    {
	    chp--;
	    *chp = '\0';
	    chp++;
	    desc2 = chp;
	    }
	}
    fprintf(o3, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
       		bioentry_id,
   		biodatabase_id,
    		display_id,
    		accession,
    		division,
		genenames,
		desc,
		desc2,
    		biosequence_str);
    sqlFreeResult(&sr2);
    sqlFreeResult(&sr4);
    row3 = sqlNextRow(sr3);
    }
    
//fprintf(stderr, "Max AA length = %d\n", maxlen);

hFreeConn(&conn2);
hFreeConn(&conn3);
sqlFreeResult(&sr3);
	
fclose(o3);
return(0);
}

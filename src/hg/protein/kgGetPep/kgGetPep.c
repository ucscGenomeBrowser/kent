/* kgGetPep generates FASTA format protein sequence file to be used for Known Genes track build */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgGetPep - generate FASTA format protein sequence file to be used for Known Genes track build.\n"
  "usage:\n"
  "   kgGetPep xxxx\n"
  "            xxxx is the release date of SWISS-PROT database, spxxxx\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
char   query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char   **row2, **row3;

char *accession;
char *displayID;
char *division;
char *extDB;
char *extAC;
    
char *proteinDataDate;

FILE *o3;

FILE *inf;
char line[100];
char *acc;
char *seq_str;
char *bioentryID;
char *databaseID;
int maxlen = {0};
int len;
   
if (argc != 2) usage();
proteinDataDate = argv[1];
 
inf = fopen("mrna.lis", "r");

conn2= hAllocConn(hDefaultDb());
conn3= hAllocConn(hDefaultDb());
    
o3 = fopen("mrnaPep.tab", "w");
while (fgets(line, 100, inf) != NULL)
    {
    line[strlen(line)-1] = '\0';
    acc = &line[1];
    
    sprintf(query3, "select * from proteins%s.spXref2 where extAc='%s' and extDB='EMBL';", 
	    proteinDataDate, acc);

    sr3 = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
	      
    while (row3 != NULL)
	{
   	accession = row3[0];
       	displayID = row3[1];	 
        division  = row3[2];  
	extDB	  = row3[3];     
	extAC	  = row3[4];
        bioentryID= row3[5];
	databaseID= row3[6];
	
    	sprintf(query2,"select val from sp%s.protein where acc='%s';", 
		proteinDataDate, accession);
    	
	sr2 = sqlMustGetResult(conn2, query2);
    	row2 = sqlNextRow(sr2);
    	while (row2 != NULL)
	    {
 	    seq_str = row2[0];
	    	
	    len = strlen(seq_str);
	    if (maxlen < len) maxlen = len;
		
	    printf(">%s\n%s\n", acc, seq_str);
	    fprintf(o3, "%s\t%s\n", acc, seq_str);
		
	    row2 = sqlNextRow(sr2);
	    }
		
        sqlFreeResult(&sr2);
	row3 = sqlNextRow(sr3);
	}
    sqlFreeResult(&sr3);
    }
    
//fprintf(stderr, "Max AA length = %d\n", maxlen);

hFreeConn(&conn2);
hFreeConn(&conn3);
	
fclose(o3);
return(0);
}


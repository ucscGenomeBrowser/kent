/* doRgdGene2 - This is a one shot program used in the RGD Genes build pipeline */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "doRgdGene2 - This program is part of the RGD Genes build pipeline.\n"
  "             It gets most of cols from the rgdGeneTemp table and\n"
  "             get the correct CDS start and end values from the rgdGene2cds table.\n"
  "usage:\n"
  "   doRgdGene2 db\n"
  "      db is the database name\n"
  "example: doRgdGene2 rn\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
 
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;
char *rgdId;
char *database;
int  i;

database = argv[1];
conn2= hAllocConn(database);
conn3= hAllocConn(database);

sqlSafef(query2, sizeof query2, "select * from rgdGeneTemp");
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    rgdId  	= row2[1];
    sqlSafef(query3, sizeof query3, "select rgdId, start, end from rgdGeneCds where rgdId='%s'", rgdId);

    sr3 = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
	      
    if  (row3 != NULL)
	{
   	for (i=1; i<6; i++)
	    {
	    printf("%s\t", row2[i]); 
	    }
	
	/* get correct CDS start and end values */
	printf("%s\t%s\t", row3[1], row3[2]);
   	
	for (i=8; i<10; i++)
	    {
	    printf("%s\t", row2[i]); 
	    }
	printf("%s\n", row2[10]);
	}
    sqlFreeResult(&sr3);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
hFreeConn(&conn3);
    
return(0);
}


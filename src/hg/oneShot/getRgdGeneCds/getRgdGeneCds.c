/* getRgdGeneCds - calculates the min(start)-1 and max(end) of 
   the CDS records of RGD genes raw data. */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getRgdGeneCds - this program calculates the min(start)-1 and max(end) of \n"
  "                the CDS records of RGD genes raw data.\n"
  "usage:\n"
  "   getRgdGeneCds db\n"
  "      db is the database name\n"
  "example: getRgdGeneCds rn4\n");
}

int main(int argc, char *argv[])
{
char *database;

struct sqlConnection *conn2, *conn3;
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;

char *rgdId;

char *cdsStart, *cdsEnd;
if (argc != 2) usage();
database  = argv[1];
   
conn2= hAllocConn(database);
conn3= hAllocConn(database);
	
sqlSafef(query2, sizeof query2, "select distinct rgdId from rgdGeneRaw2gene");
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    rgdId  	= row2[0];

    sqlSafef(query3, sizeof query3, "select min(start)-1, max(end) from rgdGeneRaw2cds where rgdId='%s'", rgdId);

    sr3 = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
	      
    if  (row3 != NULL)
	{
	if ((row3[0] != NULL) && (row3[1] != NULL))
	    {
	    cdsStart  = row3[0];
       	    cdsEnd    = row3[1];	 
	    printf("%s\t%s\t%s\n", rgdId, cdsStart, cdsEnd);
	    }
	}
    sqlFreeResult(&sr3);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
hFreeConn(&conn3);
    
return(0);
}


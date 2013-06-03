/* spXref2 - Create tab delimited data file for spXref2 table of proteins database */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "spXref2 - Create tab delimited data file for spXref2 table of proteins database \n"
  "usage:\n"
  "   spXref2 xxxx\n"
  "      xxxx is the release date of SWISS-PROT data\n"
  "Example: spXref2 070403\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3;
char query[256], query2[256], query3[256];
struct sqlResult *sr, *sr2, *sr3;
char **row, **row2, **row3;
char *r1, *r2, *r3, *r4;

FILE *o1;
char *proteinDataDate;
char *bio_dblink_id;
char *source_bioentry_id;
char *dbxref_id;

char *bioentry_id;
char *biodatabase_id;
char *display_id;
char *accession;
char *entry_version;
char *division;     

char *dbxref_id3;
char *dbname;
char *extAC;

if (argc != 2) usage();

proteinDataDate = argv[1];
o1 = fopen("temp_spXref2.dat", "w");

conn = hAllocConn();
conn2= hAllocConn();
conn3= hAllocConn();

sqlSafef(query2, sizeof query2, "select * from biosql%s.bioentry;", proteinDataDate);
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
			       
    sqlSafef(query, sizeof query, "select * from biosql%s.bioentry_direct_links where source_bioentry_id='%s';",
	    proteinDataDate, bioentry_id);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    while (row != NULL)
    	{
    	bio_dblink_id = row[0];
    	source_bioentry_id = row[1];
   	dbxref_id = row[2];
    
        sqlSafef(query3, sizeof query3,  "select * from biosql%s.dbxref where dbxref_id=%s;",
		proteinDataDate, dbxref_id);
	sr3  = sqlMustGetResult(conn3, query3);
    	row3 = sqlNextRow(sr3);

	dbxref_id3 	= row3[0];
	dbname 		= row3[1];
	extAC 		= row3[2];
			
	fprintf(o1, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", accession, display_id, division,
		    dbname, extAC,bioentry_id,biodatabase_id);
			
    	sqlFreeResult(&sr3);
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

system("cat temp_spXref2.dat | sort |uniq > spXref2.tab");
system("rm temp_spXref2.dat");
return(0);
}


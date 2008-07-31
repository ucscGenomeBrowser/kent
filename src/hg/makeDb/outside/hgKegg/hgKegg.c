/* hgKegg - creates keggPathway.tab and keggMapDesc.tab files for KG links to KEGG Pathway Map */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgKegg - creates keggPathway.tab and keggMapDesc.tab files for KG links to KEGG Pathway Map"
  "usage:\n"
  "   hgKegg xxxx\n"
  "      xxxx is the genome database name\n"
  "example: hgKegg hg16\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3;
char query[256], query2[256], query3[256];
struct sqlResult *sr, *sr2, *sr3;
char **row, **row2, **row3;

char *chp;
FILE *o1, *o2;

char *locusID;	/* LocusLink ID */
char *gbAC;		/* GenBank accession.version */
char *locusID2;	/* LocusLink ID */
char *refAC;	/* Refseq accession.version */
char *dbName; 
char cond_str[200];
char *kgID;
char *mapID;
char *desc;

if (argc != 2) usage();
dbName = argv[1];

conn = hAllocConn(dbName);
conn2= hAllocConn(dbName);
conn3= hAllocConn(dbName);

o1 = fopen("j.dat",  "w");
o2 = fopen("jj.dat", "w");
    
sprintf(query2,"select * from %sTemp.locus2Ref0;", dbName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    locusID2 	= row2[0];
    refAC 	= row2[1];
    
    sprintf(query, "select * from %sTemp.locus2Acc0 where locusID=%s and seqType='m';", 
		   dbName, locusID2);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    while (row != NULL)
    	{
	locusID 	= row[0];
	gbAC 		= row[1];
	
	chp = strstr(gbAC, ".");
	if (chp != NULL) *chp = '\0';
	chp = strstr(refAC, ".");
	if (chp != NULL) *chp = '\0';
    
	sprintf(cond_str, "name='%s'", gbAC);
        kgID = sqlGetField(dbName, "knownGene", "name", cond_str);
	if (kgID != NULL)
	    {
            sprintf(query3, "select * from %sTemp.keggList where locusID = '%s'", dbName, locusID);
            sr3 = sqlGetResult(conn3, query3);
            while ((row3 = sqlNextRow(sr3)) != NULL)
                {
                mapID   = row3[1];
		desc    = row3[2];
		fprintf(o1, "%s\t%s\t%s\n", kgID, locusID, mapID);
		fprintf(o2, "%s\t%s\n", mapID, desc);
		row3 = sqlNextRow(sr3);
                }
            sqlFreeResult(&sr3);
	    }
	row = sqlNextRow(sr);
	}
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

fclose(o1);
fclose(o2);
hFreeConn(&conn);
hFreeConn(&conn2);

system("cat j.dat|sort|uniq >keggPathway.tab");
system("cat jj.dat|sort|uniq >keggMapDesc.tab");
system("rm j.dat");
system("rm jj.dat");
return(0);
}


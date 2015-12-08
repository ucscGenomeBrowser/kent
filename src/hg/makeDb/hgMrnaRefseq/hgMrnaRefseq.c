/* hgMrnaRefseq - creates xref data between mRNAand RefSeq from LocusLink data contained in 2 tables from a temporary DB */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgMrnaRefseq - creates xref data between mRNAand RefSeq from LocusLink data contained in 2 tables from a temporary DB"
  "usage:\n"
  "   hgMrnaRefseq xxxx\n"
  "      xxxx is the genome database name\n"
  "example: hgMrnaRefseq hg16\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2;
char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
    
char *chp;
FILE *o1;

char *gbAC;		/* GenBank accession.version */
char *locusID2;	/* LocusLink ID */
char *refAC;	/* Refseq accession.version */
char *dbName; 

if (argc != 2) usage();
dbName = argv[1];

conn = hAllocConn(dbName);
conn2= hAllocConn(dbName);

o1 = fopen("j.dat", "w");
    
sqlSafef(query2, sizeof query2, "select * from %sTemp.locus2Ref0;", dbName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    locusID2 	= row2[0];
    refAC 	= row2[1];

    sqlSafef(query, sizeof query, "select * from %sTemp.locus2Acc0 where locusID=%s and seqType='m';", dbName, locusID2);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    while (row != NULL)
    	{
	gbAC 		= row[1];

	chp = strstr(gbAC, ".");
	if (chp != NULL) *chp = '\0';
    			
	chp = strstr(refAC, ".");
	if (chp != NULL) *chp = '\0';
    			
	fprintf(o1, "%s\t%s\n", gbAC, refAC);
			
	row = sqlNextRow(sr);
	}
    row2 = sqlNextRow(sr2);
    }
		
fclose(o1);
hFreeConn(&conn);
hFreeConn(&conn2);
sqlFreeResult(&sr2);

mustSystem("cat j.dat|sort|uniq >mrnaRefseq.tab");
printf("mrnaRefseq.tab created.\n");
mustSystem("rm j.dat");
return(0);
}


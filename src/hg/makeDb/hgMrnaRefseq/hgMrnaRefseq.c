/* hgMrnaRefseq - creates xref data between mRNAand RefSeq from LocusLink data contained in 2 tables from a temporary DB */

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
struct sqlConnection *conn, *conn2, *conn3;
char query[256], query2[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
    
char *chp;
FILE *o1;

char *locusID;	/* LocusLink ID */
char *gbAC;		/* GenBank accession.version */
char *giNCBI;	/* NCBI gi for the protein record associated with the CDS */
char *seqType;	/* sequence type m=mRNA g=genomic u=undefined */
char *proteinAC;	/* protein accession.version */
char *taxID;	/* tax id */
    
char *locusID2;	/* LocusLink ID */
char *refAC;	/* Refseq accession.version */
char *giNCBI2;	/* NCBI gi for the protein record associated with the CDS */
char *revStatus;	/* review status */
char *proteinAC2;	/* protein accession.version */
char *taxID2;	/* tax id */
char *dbName; 

if (argc != 2) usage();
dbName = argv[1];

conn = hAllocConn(dbName);
conn2= hAllocConn(dbName);
conn3= hAllocConn(dbName);

o1 = fopen("j.dat", "w");
    
sprintf(query2,"select * from %sTemp.locus2Ref0;", dbName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    locusID2 	= row2[0];
    refAC 	= row2[1];
    giNCBI2 	= row2[2];
    revStatus 	= row2[3];
    proteinAC2 	= row2[4];
    taxID2 	= row2[5];
		
    sprintf(query, "select * from %sTemp.locus2Acc0 where locusID=%s and seqType='m';", dbName, locusID2);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    while (row != NULL)
    	{
	locusID 	= row[0];
	gbAC 		= row[1];
	giNCBI 		= row[2];
	seqType 	= row[3];
	proteinAC 	= row[4];
	taxID 		= row[5];

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

system("cat j.dat|sort|uniq >mrnaRefseq.tab");
printf("mrnaRefseq.tab created.\n");
system("rm j.dat");
return(0);
}


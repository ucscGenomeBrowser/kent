/* hgGetKgUrls - get all URLs for Known Genes details page, to be used by text search indexing */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGetKgUrls - get all URLs for Known Genes details pages \n"
  "usage:\n"
  "   hgGetKgUrls database server\n"
  "      database is the genome database name\n"
  "      server is the hostname of the target Genome Browser server\n"
  "example: hgGetKgUrls hg17 genome.ucsc.edu\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
char *bioDB;
  
char query2[256];
struct sqlResult *sr2;
char **row2;

char *name, *chrom, *strand, *txStart, *txEnd;
char *database;
char *serverName;
char urlStr[1024];

if (argc != 3) usage();
database   = argv[1];
serverName = argv[2];
   
conn2= hAllocConn();
	
sqlSafef(query2, sizeof query2, "select name, chrom, strand, txStart, txEnd from %s.knownGene", database);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    name 	= row2[0];
    chrom 	= row2[1];
    strand	= row2[2];
    txStart 	= row2[3];
    txEnd   	= row2[4];

    sprintf(urlStr, 
            "http://%s/cgi-bin/hgGene?db=%s&hgg_gene=%s&hgg_chrom=%s&hgg_start=%s&hgg_end=%s\"",
    	    serverName, database, name, chrom, txStart, txEnd);
    printf("%s\n", urlStr);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
    
return(0);
}


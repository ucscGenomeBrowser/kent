/* gadPos - generate genomic positions for GAD entries */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gadPos -  generate genomic positions for GAD entries\n"
  "usage:\n"
  "   gadPos gdb outFile\n"
  "      gdb is the genome database\n"
  "      gadPos is the output file name for GAD positions (bed 4 format)\n"
  "example: gadPos hg17 gadPos.tab\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3, *conn4, *conn5;
 
char query[256], query2[256], query3[256], query4[256], query5[256];
struct sqlResult *sr, *sr2, *sr3, *sr4, *sr5;
char **row, **row2, **row3, **row4, **row5;

char *gdb;
char *refSeq;

FILE   *outputFile;
char *geneSymbol, *clusterId;
char *chrom, *chromStart, *chromEnd;
boolean found;

if (argc != 3) usage();
gdb   = argv[1];
   
outputFile = fopen(argv[2], "w");

conn  = hAllocConn(gdb);
conn2 = hAllocConn(gdb);
conn3 = hAllocConn(gdb);
conn4 = hAllocConn(gdb);
conn5 = hAllocConn(gdb);
	
/* loop over all gene symbols in GAD */	
sprintf(query2,"select distinct geneSymbol from %s.gadAll where association='Y'", gdb);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    geneSymbol  = row2[0];
    found = FALSE;    
    /* first look for start/end positions in KG clusters */
    sprintf(query3, 
       "select distinct clusterId from %s.kgXref x, %s.knownIsoforms i where x.geneSymbol='%s' and i.transcript=x.kgId",
       gdb, gdb, geneSymbol);

    sr3 = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);

    if (row3 != NULL)
    	{
	while (row3 != NULL)
	    {
   	    clusterId = row3[0];

    	    sprintf(query4, 
	    	    "select chrom from %s.knownCanonical where clusterId=%s", 
		    gdb, clusterId);
		
    	    sr4 = sqlMustGetResult(conn4, query4);
    	    row4 = sqlNextRow(sr4);
	    chrom = strdup(row4[0]);
            sqlFreeResult(&sr4);
	    
	    /* keep the minimum of start and maximum of end position 
	       of all genes in a cluster as the start/end position */
	       
    	    sprintf(query4, 
    		"select min(txStart), max(txEnd) from %s.knownGene k, %s.knownIsoforms i, %s.knownCanonical c where i.clusterId=%s and i.transcript=k.name and c.clusterId=i.clusterId and k.chrom=c.chrom",
		gdb, gdb, gdb, clusterId);
		
    	    sr4 = sqlMustGetResult(conn4, query4);
    	    row4 = sqlNextRow(sr4);

    	    while (row4 != NULL)
	    	{
		chromStart = row4[0];
		chromEnd   = row4[1];
		found = TRUE;
		fprintf(outputFile, "%s\t%s\t%s\t%s\n", chrom, chromStart, chromEnd, geneSymbol);
	        row4 = sqlNextRow(sr4);
		}
            sqlFreeResult(&sr4);
	    
	    row3 = sqlNextRow(sr3);
	    }
        sqlFreeResult(&sr3);
	}
    
    if (!found)
        {
	/* if no KG cluster can be found for the GAD gene symbol, 
	   try it with RefSeqs */
        sprintf(query, 
        "select distinct r.mrnaAcc from %s.refLink r where r.name='%s'",
        gdb, geneSymbol);

        sr = sqlMustGetResult(conn, query);
        row = sqlNextRow(sr);
	
	if (row !=NULL)
	    {
	    while (row != NULL)
	    	{
   	    	refSeq  = row[0];
    	    	sprintf(query4, 
    			"select chrom, txStart, txEnd from %s.refGene where name='%s'", 
			gdb, refSeq);
		
    	    	sr4 = sqlMustGetResult(conn4, query4);
    	    	row4 = sqlNextRow(sr4);

    	    	while (row4 != NULL)
	    	    {
		    chrom      = row4[0];
		    chromStart = row4[1];
		    chromEnd   = row4[2];
		    found = TRUE;
		    fprintf(outputFile, "%s\t%s\t%s\t%s\n", chrom, chromStart, chromEnd, geneSymbol);
	            row4 = sqlNextRow(sr4);
		    }
            	sqlFreeResult(&sr4);
	        row = sqlNextRow(sr);
		}
            sqlFreeResult(&sr);
	    }
	}

    if (!found)
	    /* if not finding the gene symbol in RefSeq, try it with kgAlias table then */
	    {
	    sprintf(query5, 
    		   "select chrom, txStart, txEnd from %s.knownGene, %s.kgAlias where alias='%s' and name=kgId",
		   gdb, gdb, geneSymbol);
		
    	    sr5 = sqlMustGetResult(conn5, query5);
    	    row5 = sqlNextRow(sr5);

    	    if (row5 != NULL)
		{
		chrom 	   = row5[0];
		chromStart = row5[1];
		chromEnd   = row5[2];
		
		fprintf(outputFile, "%s\t%s\t%s\t%s\n", chrom, chromStart, chromEnd, geneSymbol);
		row5 = sqlNextRow(sr5);
		}
            sqlFreeResult(&sr5);
	    }
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn);
hFreeConn(&conn2);
hFreeConn(&conn3);
hFreeConn(&conn4);
hFreeConn(&conn5);
fclose(outputFile);

return(0);
}


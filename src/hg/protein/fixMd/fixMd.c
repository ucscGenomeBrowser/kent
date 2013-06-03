/* fixMd - fix the problem of seq_contig.md file that the haplotype chromosomes start and end positions refer to original chromosome, change them into referencing the haplotype chrome initial position */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "fixMd - fix the problem of seq_contig.md\n"
  "usage:\n"
  "   fixMd hDb\n"
  "      hDb is the DB where seq_contig resides\n"
  "example: fixMd hg18\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2;
  
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;

char *database;

char *name, *chrom, *strand, *chrStart, *chrEnd, *cdsStart, *cdsEnd,
     *exonCount, *exonStarts, *exonEnds;

int j;
char *oldChrom;
int iStart, iEnd;
int iOldStart=0;

char newStart[200], newEnd[200];

if (argc != 2) usage();
database  = argv[1];
   
conn2= hAllocConn();

oldChrom = strdup("");

sqlSafef(query2, sizeof query2, "select * from %s.seq_contig", database);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    chrom 	= row2[1];
    chrStart 	= row2[2];
    chrEnd   	= row2[3];
    if (strstr(chrom, "hap") != NULL)
    	{
	if (!sameWord(oldChrom, chrom))
	    {
	    /* a new haplotype */
	    for (j=0; j<2; j++)
	    	{
	    	printf("%s\t", row2[j]);
	    	}
		
	    iOldStart = atoi(chrStart);
	    iStart = 1;
	    iEnd   =  atoi(chrEnd) - iOldStart + 1;
		
	    printf("%d\t", iStart);
	    printf("%d\t", iEnd);
	    
	    for (j=4; j<9; j++)
	    	{
	    	printf("%s\t", row2[j]);
	    	}
	    
	    printf("%s\n", row2[9]);
	    oldChrom = strdup(chrom);
	    }
	else
	    {
	    /* a continuation of a haplotype */
	    for (j=0; j<2; j++)
	    	{
	    	printf("%s\t", row2[j]);
	    	}
		
	    iStart = atoi(chrStart) - iOldStart + 1;
	    iEnd   = atoi(chrEnd) - atoi(chrStart) + iStart;
		
	    printf("%d\t", iStart);
	    printf("%d\t", iEnd);
	    
	    for (j=4; j<9; j++)
	    	{
	    	printf("%s\t", row2[j]);
	    	}
	    
	    printf("%s\n", row2[9]);
	    }
	}
    else
    	{
	for (j=0; j<9; j++)
	    {
	    printf("%s\t", row2[j]);
	    }
	printf("%s\n", row2[9]);
	}
    
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
return(0);
}


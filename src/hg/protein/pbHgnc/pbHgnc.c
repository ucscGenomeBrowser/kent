/* pbHgnc - process HGNC data */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pbHgnc - process HGNC data\n"
  "usage:\n"
  "   pbHgnc yymmdd\n"
  "      yymmdd is the release date of SWISS-PROT data\n"
  "example: pbHgnc 051015\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
char *bioDB;
  
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;

char *proteinDataDate;
 
FILE   *o2, *o3;
char *swissID, *pdb;

char *chp;
char *hgncId, *name, *symbol, *refSeqIds, *uniProt;
int j;

char *refseq;

if (argc != 2) usage();
proteinDataDate = argv[1];
   
o2 = fopen("j.dat", "w");
conn2= hAllocConn();
conn3= hAllocConn();
	
sprintf(query2,
	"select hgncId, symbol, name, refSeqIds, uniProt from proteins%s.hgnc where status not like '%cWithdrawn%c'", 
	proteinDataDate, '%', '%');
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    j=0;
    hgncId 	= row2[j];j++;
    symbol 	= row2[j];j++;
    name	= row2[j];j++;
    refSeqIds 	= row2[j];j++;
    uniProt   	= row2[j];j++;
    
    chp = strstr(hgncId, "HGNC:");
    hgncId = chp+5;

    refseq = refSeqIds;
    chp = strstr(refSeqIds, ",");
    if (chp != NULL) 
    	{
	//printf("%s\t%s\t%s\n", symbol, refseq, uniProt);fflush(stdout);
    	*chp = '\0';
        while (chp != NULL)
	    {    
	    fprintf(o2, "%s\t%s\t%s\t%s\t%s\n", symbol, refseq, uniProt, hgncId, name);
	    chp++;
 	    while (*chp == ' ') chp++;
	    refseq = chp;
            chp = strstr(refseq, ",");
	    if (chp != NULL) *chp = '\0';
	    }
	fprintf(o2, "%s\t%s\t%s\t%s\t%s\n", symbol, refseq, uniProt, hgncId, name);
	}
    else
    	{
	fprintf(o2, "%s\t%s\t%s\t%s\t%s\n", symbol, refseq, uniProt, hgncId, name);
	}

    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
fclose(o2);
    
system("cat j.dat |sort -u >hgncXref.tab");
return(0);
}


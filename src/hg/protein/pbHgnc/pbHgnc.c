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
  
char query2[256];
struct sqlResult *sr2;
char **row2;

char *proteinDataDate;
 
FILE   *o2;
char *entrez;

char *chp;
char *hgncId, *name, *symbol, *refSeqIds, *uniProt;
int j;
char *locusType;

char *refseq;
boolean gotRefseq;

if (argc != 2) usage();
proteinDataDate = argv[1];
   
o2 = fopen("j.dat", "w");
conn2= hAllocConn(hDefaultDb());
conn3= hAllocConn(hDefaultDb());

sprintf(query2,
	"select hgncId, symbol, name, refSeqMapped, refSeqIds, uniProt, entrezMapped, locusType from proteins%s.hgnc where status not like '%cWithdrawn%c'", 
	proteinDataDate, '%', '%');
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    j=0;
    hgncId 	= row2[j];j++;
    symbol 	= row2[j];j++;
    name	= row2[j];j++;
    refseq 	= row2[j];j++;
    refSeqIds 	= row2[j];j++;
    uniProt   	= row2[j];j++;
    entrez   	= row2[j];j++;
    locusType  	= row2[j];j++;
    
    chp = strstr(hgncId, "HGNC:");
    hgncId = chp+5;

    gotRefseq = FALSE;

    /* process refSeqMapped first */

    if (!sameWord(refseq, ""))
    	{
    	fprintf(o2, "%s\t%s\t%s\t%s\t%s\t%s\n", symbol, refseq, uniProt, hgncId, entrez, name);
	gotRefseq = TRUE;
	}

    /* process refSeqIds next */

    chp = strstr(refSeqIds, ",");
    if (chp != NULL) 
    	{
    	*chp = '\0';
        while (chp != NULL)
	    {    
	    fprintf(o2, "%s\t%s\t%s\t%s\t%s\t%s\n", symbol, refseq, uniProt, hgncId, entrez, name);
	    chp++;
 	    while (*chp == ' ') chp++;
	    refseq = chp;
            chp = strstr(refseq, ",");
	    if (chp != NULL) *chp = '\0';
	    }
	fprintf(o2, "%s\t%s\t%s\t%s\t%s\t%s\n", symbol, refseq, uniProt, hgncId, entrez, name);
	gotRefseq = TRUE;
	}
    else
    	{
	if (!sameWord(refseq,""))
	    {
	    fprintf(o2, "%s\t%s\t%s\t%s\t%s\t%s\n", symbol, refseq, uniProt, hgncId, entrez, name);
	    }
	else
	    {
	    /* output the record if no RefSeq in either refSeqIds or refSeqMapped */
	    if (!gotRefseq)
	    	{
	    	fprintf(o2, 
		"%s\t%s\t%s\t%s\t%s\t%s\n", symbol, refseq, uniProt, hgncId, entrez, name);
		}
	    }
	}
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
fclose(o2);
    
system("cat j.dat |sort -u >hgncXref.tab");
return(0);
}


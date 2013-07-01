/* kgCheck2 - From the initial gene candidates, */
/* go through various criteria on gene-check results and keep the ones that pass the criteria */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgCheck2 - from gene candidates, go through various criteria and keep the ones that pass the criteria \n"
  "usage:\n"
  "   kgCheck2 tempDb genomeDb candidateTable chkTable outfile\n"
  "      tempDb is the temp KG DB name\n"
  "      genomeDb is the genome DB where refGene and mgcGenes tables are\n"
  "      candidateTable is the gene candidates table in tempDb\n"
  "      checkTable is the table (in tempDb) storing gene-check results\n"
  "      outfile is the output file name\n"
  "example: kgCheck2 kgMm6ATemp mm6 kgCandidate0 geneCheck kgCandidate.tab\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3;
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;
char condStr[255];
char *answer;

char *kgTempDb;
char *outfileName;
FILE *outf;
int  i;
char *chp;
char *acc2;

char *name, *txStart, *txEnd;
char *chrom;
char *acc, *stat;
char *frame, *start, *stop;
char *causes;
char *genomeDb;
char *geneName;
char srcType;
int  alignCnt = 0;

char *candTable, *chkTable;
int  orfStop, cdsGap, cdsSplice, numCdsIntrons;
boolean passed, isRefSeq;
float ranking;

if (argc != 6) usage();
kgTempDb    = argv[1];
genomeDb    = argv[2];
candTable   = argv[3];
chkTable    = argv[4];
outfileName = argv[5];

outf = mustOpen(outfileName, "w");
conn = hAllocConn();
conn2= hAllocConn();
conn3= hAllocConn();

/* go through each protein */
sqlSafef(query2, sizeof(query2), "select * from %s.%s", kgTempDb, candTable);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    name  = row2[0];
    chrom = row2[1];
    txStart = row2[3];
    txEnd   = row2[4];
    
    /* retrieve gene-check results */
    sqlSafef(query3, sizeof(query3), 
          "select * from %s.%s where acc='%s' and chrStart=%s and chrEnd = %s",
          kgTempDb, chkTable, name, txStart, txEnd);
    sr3  = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
	{
	passed = FALSE;
	ranking  = 3;
	
   	acc 	  = row3[0];
   	stat	  = row3[5];
   	frame     = row3[6];
   	start	  = row3[7];
   	stop	  = row3[8];
   	orfStop   = atoi(row3[9]);
   	cdsGap    = atoi(row3[10]);
   	cdsSplice = atoi(row3[12]);
	numCdsIntrons = atoi(row3[18]);
	causes    = row3[21];
	
	sqlSafefFrag(condStr, sizeof(condStr), "name='%s'", acc);
	answer = sqlGetField(genomeDb, "refGene", "name", condStr);
 	if (answer != NULL) 
	    {
	    isRefSeq = TRUE;
	    }
	else
	    {
	    isRefSeq = FALSE;
	    }
	
	ranking = 9;
	/* all genes passed gene-check with status ok are considered good */
	if (sameWord(stat, "ok")) 
	    {
	    passed = TRUE;
	    ranking  = 1;
	    }
	else
	    {
	    /* exclude bad frame, orfStop, noStart, noStop */
	    if (
	     	(!sameWord(frame, "ok")) || 
		(orfStop != 0) ||
		(!sameWord(start, "ok")) ||
		(!sameWord(stop, "ok")) 
	       )
	    	{
		passed = FALSE;
		ranking = 9;
		}
	    else
	        {
		passed = TRUE;
		ranking = 2;
		
		/* reject entries with cdsPlice > 1 */
		if (cdsSplice > 0)
		    {
		    passed = FALSE;
		    /* let this special case pass */
		    if ((numCdsIntrons > 1) && (cdsSplice == 1))
			{
			passed = TRUE;
			}
		    }
		    
	 	/* if cdsGap > 0, degrade it ranking by 1.  If cdsGap is not 
		   a multiple of 3, degrade its ranking further */
		if (cdsGap > 0)
		    {
		    ranking = ranking + 1;
		    if ((cdsGap - (cdsGap/3)*3) != 0) 
		    	{
			ranking = ranking + 1;
			}
		    }
		}
	    }
	    
        /* give RefSeq entries 0.5 advantage in its ranking */  
 	if (isRefSeq)
	    {
	    ranking = ranking - 0.5;
	    }
	else
	    {	
            chp = strstr(acc, "_");
	    if (chp != NULL)
	    	{
		acc2 = chp + 1;
		}
	    else
	        {
		acc2 = acc;
		}
	    safef(condStr, sizeof(condStr), "name='%s'", acc2);
	    
	    /* If it is an MGC gene, give it a 0.3 advantable */
	    answer = sqlGetField(genomeDb, "mgcGenes", "name", condStr);
 	    if (answer != NULL) 
	    	{
	    	ranking = ranking - 0.3;
	    	}
	    }

	/* print out entries, with their rankings, that passed the above criteria */    
	if (passed) 
	    {
	    geneName = strdup(row2[0]);
	    chp = strstr(geneName, "_");
	    if (chp != NULL)
	    	{
		if (strstr(geneName, "NM_") != NULL)
		    {
		    srcType = 'R';	/* src is RefSeq */
		    }
		else
		    {
		    chp++;
		    /* keep the composite name, so that kgGetCds can process correctly */
		    /* geneName = chp; */
		    srcType  = 'U';	/* src is UCSC prot/mrna alignment */
		    }
		}
	    else
	    	{
		srcType = 'G';		/* src is GenBank */
		}
	    alignCnt++;
	    fprintf(outf, "%s\t", geneName);
	    for (i= 1; i<10; i++) fprintf(outf, "%s\t", row2[i]);
	    fprintf(outf, "%c%d\t", srcType, alignCnt);

	    fprintf(outf, "%.2f\n", ranking);
	    }

        row3 = sqlNextRow(sr3);
	}
    sqlFreeResult(&sr3);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(outf);
return(0);
}


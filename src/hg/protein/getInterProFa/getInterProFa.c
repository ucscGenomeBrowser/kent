/* getInterProFa - get AA sequence of InterPro domains for a specific organism */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getInterProFa - get AA sequence of InterPro domains for a specific organism\n"
  "usage:\n"
  "   getInterProFa tableName outFileName\n"
  "      tableName is the InterPro xref table name for the specific organism\n"
  "      outFileName is the output file name\n"
  "example: getInterProFa interProXrefHiv1 interProHiv1.fa\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3, *conn4;
char query2[256], query3[256], query4[256];
struct sqlResult *sr2, *sr3, *sr4;
char **row2, **row3, **row4;

char *aaSeq;
char *accession;
char *desc;

FILE *outFile;
char *outFileName;
char *tableName;

char *interProId;
int  maxLen, len;
char *maxAcc = NULL;
char *start, *end;
char *maxStart=NULL, *maxEnd=NULL, *maxDesc = NULL;

if (argc != 3) usage();
tableName    = argv[1];
outFileName  = argv[2];
   
outFile = mustOpen(outFileName, "w");
conn2 = hAllocConn();
conn3 = hAllocConn();
conn4 = hAllocConn();
	
/* loop over all InterPro entry for the specific InterPro xref table for this organism */
sqlSafef(query2, sizeof query2, "select distinct interProId from proteome.%s", tableName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    interProId  = row2[0];
    
    /* get all start/end positions of this InterPro domain */ 
    sqlSafef(query3, sizeof query3,
    "select accession, start, end, description from proteome.%s where interProId='%s'", 
    tableName, interProId);
    sr3 = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);

    maxLen = 0;
    while (row3 != NULL)
	{
   	accession = row3[0];
       	start     = row3[1];	 
        end       = row3[2];  
	desc      = row3[3];
	len = atoi(end) - atoi(start) + 1;
	
	/* remember the max len, so far */
	if (len > maxLen)
	    {
	    maxLen   = len;
	    maxAcc   = cloneString(accession);
	    maxStart = cloneString(start);
	    maxEnd   = cloneString(end);
	    maxDesc  = cloneString(desc);
	    }
	    
	row3 = sqlNextRow(sr3);
	}
    sqlFreeResult(&sr3);
    
    /* fetch the corresponding AA sequence of the domain having the max length */
    sqlSafef(query4, sizeof query4, "select substring(val, %s, %d) from uniProt.protein where acc='%s'",
    	    maxStart, maxLen, maxAcc);
    sr4 = sqlMustGetResult(conn4, query4);
    row4 = sqlNextRow(sr4);
    if (row4 == NULL)
    	{
	fprintf(stderr, "%s %s missing, exiting ...\n", maxAcc, interProId);
	exit(1);
	}
    else
    	{
	aaSeq = row4[0];
	if (maxLen >= 18)
	    {
	    fprintf(outFile, ">%s %s\n", interProId, maxDesc);
	    fprintf(outFile, "%s\n", aaSeq);fflush(stdout);
	    }
	}
    sqlFreeResult(&sr4);	   
    
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
hFreeConn(&conn3);
hFreeConn(&conn4);

fclose(outFile);
return(0);
}


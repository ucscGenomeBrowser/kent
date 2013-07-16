/* omimCheckAv - validate AV positions against protein sequences */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "spDb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "omimCheckAv - validate AV positions against protein sequences\n"
  "usage:\n"
  "   omimCheckAv gdb outfile\n"
  "      gdb is the geneome DB\n"
  "      outFile is the validated AV positions"
  "example: omimCheckAv hg17 omimAvPosValidated.tab\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3, *conn4;
 
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;

char *accession;
char *extDB;
char *extAC;
char condStr[255];

char *id, *subId, *avStr, *pos;
char *baseAAStr, *subsAAStr;
char baseAA, subsAA;

char *genomeDb;
char *aaSeq;
char ch;

int  aaPos, aaLen;
int nTotal = 0;
int nOK    = 0;
int nBase  = 0;
int nErr   = 0;
int nSubs  = 0;
boolean gotAMatch = FALSE;

FILE   *outf;

if (argc != 3) usage();
genomeDb = argv[1];
   
outf = fopen(argv[2], "w");
conn2= hAllocConn();
conn3= hAllocConn();
conn4= hAllocConn();
	
/* loop thru all recordd in the omimAvPos table */
sqlSafef(query2, sizeof query2, "select * from %s.omimAvPos", genomeDb);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    id 		= row2[0];
    subId 	= row2[1];
    avStr	= row2[2];
    pos 	= row2[3];
    baseAAStr	= row2[4];
    subsAAStr	= row2[5]; 
    
    baseAA	= *baseAAStr;
    subsAA	= *subsAAStr;
    aaPos 	= atoi(pos);
    
    /* find corresponding protein for each OMIM record */
    sqlSafef(query3, sizeof query3,  
        "select distinct accession, extDB, extAC from %s.spXref2 where extAC='%s' and extDB='MIM';",
    	    PROTEOME_DB_NAME, id);

    sr3 = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);

    while (row3 != NULL)
	{
   	accession = row3[0];
	extDB	  = row3[1];     
	extAC	  = row3[2];

	nTotal++;
        
	gotAMatch = FALSE;
	
	/* get protein sequence */
	sqlSafefFrag(condStr, sizeof condStr, "acc='%s'", accession);
	aaSeq = sqlGetField(UNIPROT_DB_NAME, "protein", "val", condStr);
	aaLen = strlen(aaSeq);

	/* check AA (both base and substitition) of the AV entry against 
	   AA in the protein sequence */
	if (aaPos <= aaLen)
	    {
	    ch = *(aaSeq+aaPos-1);
	    if (ch == baseAA)
	    	{
		gotAMatch = TRUE;
		nOK++;
		nBase++;
		}
	    else
	    	{
	    	if (ch == subsAA)
	    	    {
		    gotAMatch = TRUE;
		    nOK++;
		    nSubs++;
		    }
		}
	    
	    if (gotAMatch) 
	    	{
	        fprintf(outf, "%s\t%s\t%s\t%s\n", id, subId, accession, pos);
	    	}
	    else 
	    	{
		nErr++;
		}
	    }
	else
	    {
	    nErr++;
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

fprintf(stderr, "nTotal\t= %6d\n", nTotal);
fprintf(stderr, "nOk\t= %6d\n", nOK);
fprintf(stderr, "nBase\t= %6d\n", nBase);
fprintf(stderr, "nSub\t= %6d\n", nSubs);
fprintf(stderr, "nErr\t= %6d\n", nErr);

return(0);
}


/* gsIdXref - generate data for the gsIdXref table */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gsIdXref - create cross-reference between the GSID subj IDs and their DNA and protein sequence IDs"
  "       generate a list of proteins and a list of protein/mRNA pairs.\n"
  "usage:\n"
  "   gsIdXref hdb outFile\n"
  "      hdb is the genome database\n"
  "      outFile is the output file name\n"
  "example: gsIdXref hiv1 gsIdXref.tab\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
 
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;

char *subjId, *rawId, *aaSeqId;
char *genomeRelease;
char *outFileName;
char *chp;
char *labCode;
char seqClass = 'X';
 
FILE   *outf;

if (argc != 3) usage();
genomeRelease   = argv[1];
outFileName     = argv[2];
   
outf = fopen(outFileName, "w");
conn2= hAllocConn(genomeRelease);
conn3= hAllocConn(genomeRelease);
	
sprintf(query2,"select subjId, labCode from %s.labCodeSubjId", genomeRelease);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    subjId = row2[0];
    labCode = row2[1];

    chp = strstr(subjId, "GSID3");
    if (chp != NULL)
	{
	seqClass = 'T';
	}
    else
    	{
	chp = strstr(subjId, "GSID4");

    	if (chp != NULL)
	    {
	    seqClass = 'U';
	    }
        else
	    {
	    fprintf(stderr, "%s seems not a valid subject ID...\n", subjId);
	    exit(1);
	    }
	}

    /* skip it if labCode is empty */
    if (sameWord(labCode, ""))
	{
	fprintf(stderr, "skipping %s because the associated labCode is empty\n", subjId);
        row2 = sqlNextRow(sr2);
	continue;
	}

    sprintf(query3,"select id from %s.aaSeq where id like 'p1.%s%c' order by id", 
	    genomeRelease, labCode, '%');

    sr3 = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
    if (row3 == NULL)
	{
	fprintf(stderr, "%s has no sequence.\n", subjId);
	}	      
    while (row3 != NULL)
	{
   	aaSeqId  = row3[0];
	rawId = aaSeqId + 3;

	fprintf(outf, "%s\tss%c%s\t%s\n", 
	        subjId, '.', rawId, aaSeqId);
	fflush(outf);
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


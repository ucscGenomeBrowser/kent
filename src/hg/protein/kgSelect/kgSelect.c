/* kgSelect - select the highest scoring mrna (or RefSeq) from the protMrnaScore table */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgSelect - select the highest scoring mrna (or RefSeq) from the protMrnaScore table\n"
  "usage:\n"
  "   kgSelect tempDb outfile\n"
  "      tempDb is the temp DB name\n"
  "      outfile is the output file name\n"
  "example: kgSelect kgMm6ATemp kgTemp2.gp\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3, *conn4;
char query2[256], query3[256], query4[256];
struct sqlResult *sr2, *sr3, *sr4;
char **row2, **row3, **row4;
char *kgTempDb;
char *outfileName;
FILE *outf;
int  alignCnt = 0;
char *score;
int  i;
char protMrnaName[255];
char *chp;
char *protAcc, *mrnaAcc;
char srcType;
char *geneName;

if (argc != 3) usage();
kgTempDb    = argv[1];
outfileName = argv[2];

outf = mustOpen(outfileName, "w");
conn2= hAllocConn();
conn3= hAllocConn();
conn4= hAllocConn();

/* go through each protein */
sqlSafef(query2, sizeof(query2), "select distinct protAcc from %s.protMrnaScore", kgTempDb);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    /* get all prot/mRna pairs in score table with this protein, ordered by score */
    protAcc = row2[0];
    sqlSafef(query3, sizeof(query3), 
            "select mrnaAcc, score from %s.protMrnaScore where protAcc='%s' order by score desc limit 1", 
    	    kgTempDb, protAcc);
    sr3  = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
	      
    /* get the top scoring mRNA or RefSeq */	      
    if (row3 != NULL)
	{
   	mrnaAcc = row3[0];
	score   = row3[1];
	safef(protMrnaName, sizeof(protMrnaName), "%s_%s", protAcc, mrnaAcc);
	sqlSafef(query4, sizeof(query4), 
		"select * from %s.kgCandidate where name='%s' or name='%s'", 
		kgTempDb, mrnaAcc, protMrnaName);
    	sr4  = sqlMustGetResult(conn4, query4);
    	row4 = sqlNextRow(sr4);
	if (row4 != NULL)
	    {
	    geneName = strdup(row4[0]);
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
		    geneName = chp;
		    srcType  = 'U';	/* src is UCSC prot/mrna alignment */
		    }
		}
	    else
	    	{
		srcType = 'G';		/* src is GenBank */
		}
	    alignCnt++;
	    fprintf(outf, "%s\t", geneName);
	    for (i= 1; i<10; i++) fprintf(outf, "%s\t", row4[i]);
	    fprintf(outf, "%s\t%c_%d\n", protAcc, srcType, alignCnt);fflush(stdout);
	    }
        sqlFreeResult(&sr4);
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


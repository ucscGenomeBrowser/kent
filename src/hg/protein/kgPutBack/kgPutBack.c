/* kgPutBack - From the initial gene candidates, */
/* go through various criteria on gene-check results and keep the ones that pass the criteria */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgPutBack - from gene candidates, go through various criteria and keep the ones that pass the criteria \n"
  "usage:\n"
  "   kgPutBack tempDb genomeDb protDb putBackTable outfile\n"
  "      tempDb is the temp KG DB name\n"
  "      genomeDb is the genome DB where refGene and mgcGenes tables are\n"
  "      protDb is the proteinsYYMMDD DB\n"
  "      putBackTable is the put back list table\n"
  "      outfile is the output file name\n"
  "example: kgPutBack kgHg18ATemp hg18 sp060115 kgPutBack2 kgPutBack2.gp\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3;
char query2[256], query3[256], query[256];
struct sqlResult *sr2, *sr3, *sr;
char **row2, **row3, **row;
int  icnt;
char *proteinID;
char *kgId;
char *spDb;
char *kgTempDb;
char *outfileName;
FILE *outf;
int  i;

char *genomeDb;
int  iAlign = 0;

char *putBackTable;

if (argc != 6) usage();
kgTempDb    = argv[1];
genomeDb    = argv[2];
spDb  = argv[3];
putBackTable= argv[4];
outfileName = argv[5];

outf = mustOpen(outfileName, "w");
conn = hAllocConn(genomeDb);
conn2= hAllocConn(genomeDb);
conn3= hAllocConn(genomeDb);

icnt = 1;
/* go through each put back RefSeq */
safef(query2, sizeof(query2), "select distinct acc from %s.%s", kgTempDb, putBackTable);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    kgId = row2[0];
    
    safef(query3, sizeof(query3), 
          "select * from %s.refGene where name='%s'", genomeDb, kgId);
    sr3  = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);
	{
        safef(query, sizeof(query), 
            "select d.val,score from %s.protMrnaScore,%s.displayId d where mrnaAcc='%s' and d.acc=protAcc order by score desc",
	    kgTempDb, spDb, kgId);
    	sr  = sqlMustGetResult(conn, query);
    	row = sqlNextRow(sr);
	if (row != NULL)
	    {
	    proteinID = row[0];	    
	
	    for (i=0; i<10; i++) fprintf(outf, "%s\t", row3[i]);
	    iAlign++;
	    fprintf(outf, "%s\tR%dP\n", proteinID, iAlign);
	    icnt++;
    	    row = sqlNextRow(sr);
	    }
    	else
	    {
	    fprintf(stderr, "No matching protein found for %s.\n", kgId);
	    }
	sqlFreeResult(&sr);
        
	row3 = sqlNextRow(sr3);
	}
    sqlFreeResult(&sr3);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn);
hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(outf);
return(0);
}


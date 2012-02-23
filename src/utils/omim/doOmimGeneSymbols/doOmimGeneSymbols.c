/* doOmimGeneSymbols - This is a one shot program used in the OMIM related subtracks build pipeline */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "doOmimGeneSymbols - This program is part of the OMIM related subtracks build pipeline.\n"
  "usage:\n"
  "   doOmimGeneSymbols db outFileName\n"
  "      db is the database name\n"
  "      outFileName is the filename of the output file\n"
  "example: doOmimGeneSymbols hg19 omimGeneSymbol.tab\n");
}

char *omimId;
FILE   *outf;

int main(int argc, char *argv[])
{
char *database;
char *outFn;

struct sqlConnection *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;

int iCnt;

char *chp1, *chp2;

if (argc != 3) usage();

database = argv[1];
conn2= hAllocConn(database);

outFn   = argv[2];
outf    = mustOpen(outFn, "w");

sprintf(query2,"select omimId, geneSymbol from omimGeneMap");
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    boolean oneRecDone;
    
    omimId = row2[0];

    iCnt = 0;
    chp1 = row2[1];
    oneRecDone = FALSE;
    while (!oneRecDone) 
    	{
	iCnt++;
	chp2 = strstr(chp1,",");
	while (*chp1 == ' ') chp1++;
	if (chp2 != NULL)
	    {
	    *chp2 = '\0';
	    fprintf(outf, "%s\t%s\n", omimId, chp1);
            chp2++;
	    chp1 = chp2;
	    }
	else
	    {
	    fprintf(outf, "%s\t%s\n", omimId, chp1);
	    oneRecDone = TRUE;
	    }
	}
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

fclose(outf);
hFreeConn(&conn2);
return(0);
}


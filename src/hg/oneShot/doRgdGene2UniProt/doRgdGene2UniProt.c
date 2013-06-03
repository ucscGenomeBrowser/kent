/* doRgdGene2ToUniProt - This is a one shot program used in the RGD Genes build pipeline */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "doRgdGene2ToUniProt - This program is part of the RGD Genes build pipeline.\n"
  "             It parses the uniprot_id  col of the gene records of the raw RGD gene data file, GENES_RAT,\n"
  "             and create a xref table between RGD gene ID and UniProt ID.\n"
  "usage:\n"
  "   doRgdGene2ToUniProt db outFileName\n"
  "      db is the database name\n"
  "      outFileName is the filename of the output file\n"
  "example: doRgdGene2ToUniProt rn4 rgdGene2ToUniProt.tab\n");
}

char *rgdId;
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

outFn = argv[2];
outf = mustOpen(outFn, "w");

sqlSafef(query2, sizeof query2, "select gene_rgd_id, uniprot_id from rgdGene2Raw where uniprot_id <> \"\"");
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    boolean oneGeneDone;
    
    rgdId = row2[0];

    iCnt = 0;
    chp1 = row2[1];
    oneGeneDone = FALSE;
    while (!oneGeneDone) 
    	{
	iCnt++;
	chp2 = strstr(chp1,",");
	if (chp2 != NULL)
	    {
	    *chp2 = '\0';
	    fprintf(outf, "RGD:%s\t%s\n", rgdId, chp1);
            chp2++;
	    chp1 = chp2;
	    }
	else
	    {
	    fprintf(outf, "RGD:%s\t%s\n", rgdId, chp1);
	    oneGeneDone = TRUE;
	    }
	}
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

fclose(outf);
hFreeConn(&conn2);
return(0);
}


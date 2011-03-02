/* kgBioCyc0 - collect needed data from BioCyc database */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgBioCyc0 - collect needed data from BioCyc database \n"
  "usage:\n"
  "   kgBioCyc0 bioCycDb gdb\n"
  "      bioCycDb is the BioCyc database\n"
  "      gdb is the genome database\n"
  "      ensDb is the genome database where ensemblXref3 resides\n"
  "example: kgBioCyc0 bioCyc060216 hg18 hg17\n");
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn2, *conn3;
char *pathway;

char *gdb, *bioCycDb, *ensGdb;
int i;
char *geneId = NULL;
char *symbol;

char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;

FILE   *outf;
char *kgId;

if (argc != 4) usage();
bioCycDb = argv[1];
gdb      = argv[2];
ensGdb   = argv[3];
   
outf = fopen("bioCycPathway.tmp", "w");
conn2= hAllocConn(gdb);
conn3= hAllocConn(gdb);
	
sprintf(query2, "select * from %s.pathways;", bioCycDb);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    pathway = row2[0];
    for (i=0; i<63; i++)
    	{
	/* first get kgId of genes with the same gene symbol */
    	sprintf(query3, "select gene_name%d from %s.pathways where UNIQUE_ID='%s'", 
    	    i+1, bioCycDb, pathway);

    	sr3 = sqlMustGetResult(conn3, query3);
    	row3 = sqlNextRow(sr3);
	
	symbol = strdup(row3[0]);
    	sqlFreeResult(&sr3);
	
	if (strcmp(symbol, "") != 0)
	    {
	    sprintf(query3, "select kgId from %s.kgXref where geneSymbol = '%s'", gdb, symbol);

    	    sr3 = sqlMustGetResult(conn3, query3);
    	    row3 = sqlNextRow(sr3);

	    if (row3 != NULL)
	    	{
	    	kgId= row3[0];
	    	if (strcmp(kgId, "") != 0)
	    	    {
	    	    fprintf(outf, "%s\t%s\t%s\n", kgId, symbol, pathway);fflush(stdout);
	    	    }
	    	}
    	    sqlFreeResult(&sr3);
	    }
	/* then get kgIds with the same UniProt accession via Ensembl gene IDs */
	sprintf(query3, "select gene_id%d from %s.pathways where UNIQUE_ID='%s'", 
    	    i+1, bioCycDb, pathway);
    	sr3 = sqlMustGetResult(conn3, query3);
    	row3 = sqlNextRow(sr3);
	
	geneId = NULL;
	if (row3 != NULL) 
	    if (row3[0] != NULL) geneId = cloneString(row3[0]);
    	sqlFreeResult(&sr3);
	if (geneId != NULL) 
	    if (strcmp(geneId, "") != 0)
	    	{
            	safef(query3, sizeof(query3),
                      "select kgId from %s.kgXref k, %s.ensGeneXref e where e.gene_id='%s' and k.spId=e.external_name and external_db like 'UniProt%%'", 
                      gdb, ensGdb, geneId);
    	    	sr3 = sqlMustGetResult(conn3, query3);
    	    	row3 = sqlNextRow(sr3);
	    	if (row3 != NULL)
	    	    {
	    	    kgId= row3[0];
	    	    if (strcmp(kgId, "") != 0)
	    	    	{
	    	    	fprintf(outf, "%s\t%s\t%s\n", kgId, geneId, pathway);fflush(stdout);
	    	    	}
	    	    }
    	    	sqlFreeResult(&sr3);
	    	}
	}
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);
hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(outf);

system("cat bioCycPathway.tmp |sort -u  >bioCycPathway.tab");
system("rm bioCycPathway.tmp");
return(0);
}


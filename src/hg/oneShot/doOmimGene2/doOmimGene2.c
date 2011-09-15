/* doOmimGene2 - This is a one shot program used in the OMIM related subtracks build pipeline */
#include "common.h"
#include "hCommon.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "doOmimGene2 - This program is part of the OMIM related subtracks build pipeline.\n"
  "usage:\n"
  "   doOmimGene2 db outFileName\n"
  "      db is the database name\n"
  "      outFileName is the filename of the output file\n"
  "example: doOmimGene2 hg19 omimGene2.tab\n");
}

int main(int argc, char *argv[])
{
char *database;
char *outFn;
FILE   *outf;

struct sqlConnection *conn1;
struct sqlConnection *conn2;
char query1[256];
char query2[256];
struct sqlResult *sr1;
struct sqlResult *sr2;
char **row1;
char **row2;
char *omimId, *geneId;

if (argc != 3) usage();

database = argv[1];
conn1= hAllocConn(database);
conn2= hAllocConn(database);

outFn   = argv[2];
outf    = mustOpen(outFn, "w");

/* get all OMIM Genes with Gene ID (Entrez/LocusLink) */
//sprintf(query1, "select omimId, geneId from mim2gene where geneId <>'-' ");
sprintf(query1, "select omimId, geneId from mim2gene where geneId <>'-' and entryType='gene' ");
sr1 = sqlMustGetResult(conn1, query1);
row1 = sqlNextRow(sr1);
while (row1 != NULL)
    {
    omimId = row1[0];
    geneId = row1[1];

    /* get corresponding RefSeq data, ordered by length of the gene */
    sprintf(query2,
    // added mrnaAcc as the 2nd ordering condition
    "select r.chrom, r.txStart, r.txEnd, r.name, r.txEnd-r.txStart, mrnaAcc from refGene r, refLink where mrnaAcc=r.name and locusLinkId = %s order by txEnd-txStart desc, mrnaAcc desc", geneId);
    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    
    /* take the top logest one */
    if (row2 != NULL)
    	{
    	int j;
    	for (j=0; j<3; j++)
    	    {
	    fprintf(outf, "%s\t", row2[j]);
	    }
        fprintf(outf, "%s\n", omimId);
	}
    sqlFreeResult(&sr2);
    row1 = sqlNextRow(sr1);
    }
sqlFreeResult(&sr2);

fclose(outf);
hFreeConn(&conn1);
hFreeConn(&conn2);
return(0);
}


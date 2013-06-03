/* hgGetNrOmimGene - Generate omimGene entries related to NR_xxxx RefSeq. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hdb.h"
#include "options.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGetNrOmimGene - Generate omimGene entries related to NR_xxxx RefSeq\n"
  "usage:\n"
  "   hgGetNrOmimGene database outFileName\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void hgGetNrOmimGene(char *database, char *outFileName)
/* hgGetNrOmimGene - Generate omimGene entries related to NR_xxxx RefSeq. */
{
struct sqlConnection *conn2, *conn3;
 
char query2[256], query3[256];
struct sqlResult *sr2, *sr3;
char **row2, **row3;
FILE *outf;

char *chrom, *txStart, *txEnd;
char *omimId;

outf = fopen(outFileName, "w");
conn2= hAllocConn(database);
conn3= hAllocConn(database);
	
/* first get all RefSeq entries that begin with "NR_" and have related OMIM entries */
sqlSafef(query2, sizeof query2, "select g.chrom, g.txStart, g.txEnd, omimId from refGene g, refLink l, omimGene o "
    "where l.mrnaAcc=g.name and g.name like 'NR_%c' and omimId <>0 limit 1000", '%');
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
while (row2 != NULL)
    {
    chrom 	= row2[0];
    txStart 	= row2[1];
    txEnd	= row2[2];
    omimId	= row2[3];

    /* then check if this omimId is already in the omimGene table */
    sqlSafef(query3, query3, "select name from %s.omimGene where name='%s'",
    	    database, omimId);
    sr3 = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);

    /* if not, create a new omimGene entry */
    if (row3 == NULL)
	{
	fprintf(outf,"%s\t%s\t%s\t%s\n", chrom, txStart, txEnd, omimId);
	}
    sqlFreeResult(&sr3);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

hFreeConn(&conn2);
hFreeConn(&conn3);
fclose(outf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hgGetNrOmimGene(argv[1], argv[2]);
return 0;
}

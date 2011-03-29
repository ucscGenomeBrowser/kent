/* regStartSampleEmbl - Make up a EMBL format file (because it's an easy way to do 
 * structured multiline text) with a sample of genes to annotate.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "basicBed.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regStartSampleEmbl - Make up a EMBL format file (because it's an easy way to do structured multiline text) with a sample of genes to annotate.\n"
  "usage:\n"
  "   regStartSampleEmbl db count output.embl\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int getStartOfGeneBefore(struct sqlConnection *conn, struct bed4 *gene)
/* Get the start of the previous non-overlapping gene */
{
char query[256];
safef(query, sizeof(query), 
    "select max(txStart) from knownGene where chrom='%s' and txEnd < %d", 
    gene->chrom, gene->chromStart);
return sqlQuickNum(conn, query);
}

int getEndOfGeneAfter(struct sqlConnection *conn, struct bed4 *gene)
/* Get the end of the next non-overlapping gene */
{
char query[256];
safef(query, sizeof(query), 
    "select min(txEnd) from knownGene where chrom='%s' and txStart > %d", 
    gene->chrom, gene->chromEnd);
return sqlQuickNum(conn, query);
}

void regStartSampleEmbl(char *db, char *countString, char *outFile)
/* regStartSampleEmbl - Make up a EMBL format file (because it's an easy way to do 
 * structured multiline text) with a sample of genes to annotate.. */
{
int count = atoi(countString);
struct sqlConnection *conn = sqlConnect(db);
FILE *f = mustOpen(outFile, "w");

/* Get list of random genes (canonical isoform) into bed4 format. */
struct bed4 *gene, *geneList = NULL;
char query[512];
safef(query, sizeof(query), 
	"select chrom,chromStart,chromEnd,transcript from knownCanonical,kgTxInfo "
	"where knownCanonical.transcript = kgTxInfo.name "
	"and chrom not like '%%hap%%' "
	"and category='coding' order by rand() limit %d", count);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(gene);
    gene->chrom = cloneString(row[0]);
    gene->chromStart = sqlUnsigned(row[1]);
    gene->chromEnd = sqlUnsigned(row[2]);
    gene->name = cloneString(row[3]);
    slAddHead(&geneList, gene);
    }
slReverse(&geneList);

int ix = 0;
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    /* Print basic information on gene. */
    safef(query, sizeof(query), "select geneSymbol,description from kgXref where kgId = '%s'"
    	,gene->name);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row = sqlNextRow(sr);
    fprintf(f, "GENE %s\n", row[0]);
    fprintf(f, "DESC %s\n", row[1]);
    sqlFreeResult(&sr);
    fprintf(f, "UCSC %s\n", gene->name);
    fprintf(f, "NUMB %d\n", ++ix);

    /* Print out number of splicing isoforms. */
    safef(query, sizeof(query), "select clusterId from knownCanonical where transcript='%s'"
    	,gene->name);
    int clusterId = sqlQuickNum(conn, query);
    safef(query, sizeof(query), "select count(*) from knownIsoforms where clusterId=%d"
    	,clusterId);
    int isoformCount = sqlQuickNum(conn, query);
    fprintf(f, "ISOF %d\n", isoformCount);

    /* Get gene neighborhood. */
    safef(query, sizeof(query), "select chrom from knownGene whre name = '%s'", gene->name);
    int start = getStartOfGeneBefore(conn, gene);
    int end = getEndOfGeneAfter(conn, gene);
    fprintf(f, "NBHD %s:%d-%d\n", gene->chrom, start+1, end);
    sqlFreeResult(&sr);

    /* Now print some lines we need to fill in by hand. */
    fprintf(f, "TRANSC \n");
    fprintf(f, "BIPROM \n");
    fprintf(f, "DNAPRO \n");
    fprintf(f, "ME3PRO \n");
    fprintf(f, "ME1PRO \n");
    fprintf(f, "ME1BEF \n");
    fprintf(f, "ME1INT \n");
    fprintf(f, "NOTES \n");

    fprintf(f, "//\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
regStartSampleEmbl(argv[1], argv[2], argv[3]);
return 0;
}

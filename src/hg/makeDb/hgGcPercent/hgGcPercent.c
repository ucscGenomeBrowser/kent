/* hgGcPercent - Calculate GC Percentage in 20kb windows. */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "jksql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGcPercent - Calculate GC Percentage in 20kb windows\n"
  "usage:\n"
  "   hgGcPercent database nibDir\n");
}

char *createTable = 
"#Displays GC percentage in 20Kb blocks for genome\n"
"CREATE TABLE gcPercent (\n"
    "chrom varchar(255) not null,	# Human chromosome number\n"
    "chromStart int unsigned not null,	# Start position in genoSeq\n"
    "chromEnd int unsigned not null,	# End position in genoSeq\n"
    "name varchar(255) not null,	# Constant string GCpct\n"
    "gcPpt int unsigned not null,	# GC percentage for 20Kb block\n"
              "#Indices\n"
    "UNIQUE(chrom(12),chromStart),\n"
    "INDEX(chrom(12), chromEnd)\n"
");\n";

void makeGcTab(char *nibFile, char *chrom, FILE *f)
/* Scan through nib file and write out GC percentage info in
 * 20 kb windows. */
{
int chromSize, start, end, oneSize, winSize = 20000;
int minCount = winSize/4;
int i, count, gcCount, val, ppt;
struct dnaSeq *seq = NULL;
FILE *nf = NULL;
DNA *dna;
int dotMod = 0;

nibOpenVerify(nibFile, &nf, &chromSize);
for (start=0; start<chromSize; start = end)
    {
    if ((++dotMod&127) == 0)
       {
       printf(".");
       fflush(stdout);
       }
    end = start + winSize;
    if (end > chromSize)
        end = chromSize;
    oneSize = end - start;
    seq = nibLdPart(nibFile, nf, chromSize, start, oneSize);
    dna = seq->dna;
    count = gcCount = 0;
    for (i=0; i<oneSize; ++i)
        {
	if ((val = ntVal[dna[i]]) >= 0)
	    {
	    ++count;
	    if (val == G_BASE_VAL || val == C_BASE_VAL)
	        ++gcCount;
	    }
	}
    freeDnaSeq(&seq);
    if (count >= minCount)
	ppt = round(1000.0*(double)gcCount/(double)count);
    else
        ppt = 0;
    fprintf(f, "%s\t%d\t%d\t%s\t%d\n",
	    chrom, start, end, "GC", ppt, gcCount, count);
    }
carefulClose(&nf);
printf("\n");
}

void hgGcPercent(char *database, char *nibDir)
/* hgGcPercent - Calculate GC Percentage in 20kb windows on all chromosomes. */
{
struct fileInfo *nibList = listDirX(nibDir, "*.nib", TRUE), *nibEl;
char dir[256], chrom[128], ext[64];
char *tabFileName = "gcPercent.bed";
FILE *tabFile = mustOpen(tabFileName, "w");
struct sqlConnection *conn;
char query[256];

/* Create tab file with all GC percent data. */
for (nibEl = nibList; nibEl != NULL; nibEl = nibEl->next)
    {
    splitPath(nibEl->name, dir, chrom, ext);
    printf("Processing %s\n", nibEl->name);
    makeGcTab(nibEl->name, chrom, tabFile);
    }
carefulClose(&tabFile);

/* Load that file in database. */
conn = sqlConnect(database);
printf("Loading gcPercent table\n");
sqlMaybeMakeTable(conn, "gcPercent", createTable);
sqlUpdate(conn, "DELETE from gcPercent");
sprintf(query, "LOAD data local infile '%s' into table gcPercent", tabFileName);
sqlUpdate(conn, query);
sqlDisconnect(&conn);

slFreeList(&nibList);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
dnaUtilOpen();
hgGcPercent(argv[1], argv[2]);
return 0;
}

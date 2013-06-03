/* tetDensity - density of tetraodon alignments vs. chromosomes. */
#include "common.h"
#include "hash.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "tetDensity - density of tetraodon alignments vs. chromosomes\n"
  "usage:\n"
  "   tetDensity database\n");
}

char *chromNames[] =
    {
    "chr1",
    "chr2",
    "chr3",
    "chr4",
    "chr5",
    "chr6",
    "chr7",
    "chr8",
    "chr9",
    "chr10",
    "chr11",
    "chr12",
    "chr13",
    "chr14",
    "chr15",
    "chr16",
    "chr17",
    "chr18",
    "chr19",
    "chr20",
    "chr21",
    "chr22",
    "chrX",
    "chrY",
    };

int tetCountInChrom(struct sqlConnection *conn, char *chrom)
/* Number of tets in chromosome. */
{
char query[512];
int tetCount = 0;
int lastPos = -1000000;
struct sqlResult *sr;
char **row;
int chromPos;

sqlSafef(query, sizeof query, "select chromStart from %s_tet_waba", chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromPos = sqlUnsigned(row[0]);
    if (chromPos - lastPos > 1000)
        ++tetCount;
    lastPos = chromPos;
    }
sqlFreeResult(&sr);
return tetCount;
}

int islandCountInChrom(struct sqlConnection *conn, char *chrom)
/* Number of island in chromosome. */
{
char query[512];

sqlSafef(query, sizeof query, "select count(*) from cpgIsland where chrom = '%s'", chrom);
return sqlQuickNum(conn, query);
}

int knownCount(struct sqlConnection *conn, char *chrom)
/* Number of island in chromosome. */
{
char query[512];

sqlSafef(query, sizeof query, "select count(*) from genieKnown where chrom = '%s'", chrom);
return sqlQuickNum(conn, query);
}




void chromTetDensity(struct sqlConnection *conn, char *chrom)
/* Figure out density of tet alignments on one chromosome. */
{
char query[512];
int chromSize;
int tetCount = 0;
int lastPos = -1000000;
struct sqlResult *sr;
char **row;
int chromPos;

sqlSafef(query, sizeof query, "select size from chromInfo where chrom = '%s'", chrom);
chromSize = sqlQuickNum(conn, query);
tetCount = tetCountInChrom(conn, chrom);
printf("%5s %6.3f %6.3f %6.3f\n", chrom, 
    1000000.0*(double)tetCount/(double)chromSize/0.7712, 
    100000.0*(double)islandCountInChrom(conn, chrom)/(double)chromSize/0.3047,
    1000000*(double)knownCount(conn, chrom)/(double)chromSize/0.5597);
}

void tetDensity(char *database)
/* tetDensity - density of tetraodon alignments vs. chromosomes. */
{
struct sqlConnection *conn = sqlConnect(database);
int i;

printf("Chrom  tetrao cpgIsl known\n");
for (i=0; i<ArraySize(chromNames); ++i)
    {
    chromTetDensity(conn, chromNames[i]);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
tetDensity(argv[1]);
return 0;
}

/* seqWithoutDeletions -- for a given chrom, generate "skinny" sequence: that is, exclude deletions. */
/* sequence file is separate input -- could also get from chromInfo */
/* write to chrom.fat */

#include "common.h"
#include "hdb.h"

static char const rcsid[] = "$Id: seqWithoutDeletions.c,v 1.1 2006/08/31 04:18:06 heather Exp $";

static char *database = NULL;
static char *chromName = NULL;
static char *snpTable = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "seqWithoutDeletions - generate sequence that excludes deletions\n"
    "usage:\n"
    "    seqWithoutDeletions database chrom sequenceFile snpTable\n");
}


struct dnaSeq *getSkinnySeq(char *sequenceFile, char *chromName)
/* mark deletions with '-' */
/* could filter out ObservedWrongSize and ObservedMismatch */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

struct dnaSeq *seq;
char *seqPtr = NULL;

int pos = 0;
int start = 0;
int end = 0;
int chromSize = 0;
int slashCount = 0;
int snpCount = 0;

char *snpChrom = NULL;
char *strand = NULL;
char *snpClass = NULL;
char *locType = NULL;
char *observed = NULL;
char *rsId = NULL;

verbose(1, "sequence file = %s\n", sequenceFile);
verbose(1, "chrom = %s\n", chromName);
chromSize = hChromSize(chromName);
verbose(1, "chromSize = %d\n", chromSize);
seq = hFetchSeq(sequenceFile, chromName, 0, chromSize);
// seq = hLoadChrom(chromName);
touppers(seq->dna);
seqPtr = seq->dna;

safef(query, sizeof(query), "select chrom, chromStart, chromEnd, strand, class, locType, observed, name from %s", snpTable);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpChrom = cloneString(row[0]);
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    strand = cloneString(row[3]);
    snpClass = cloneString(row[4]);
    locType = cloneString(row[5]);
    observed = cloneString(row[6]);
    rsId = cloneString(row[7]);

    /* input filters */
    if (!sameString(snpClass, "deletion")) continue;
    if (!sameString(snpChrom, chromName)) continue;
    if (sameString(locType, "between")) continue;
    if (end <= start) continue;
    if (start > chromSize) continue;
    if (end > chromSize) continue;
    if (strlen(observed) < 2) continue;
    /* First char should be dash, second char should be forward slash. */
    if (observed[0] != '-') continue;
    if (observed[1] != '/') continue;
    /* Only one forward slash. */
    int slashCount = chopString(observed, "/", NULL, 0);
    if (slashCount > 2) continue;

    snpCount++;

    for (pos = start; pos < end; pos++)
        seqPtr[pos] = '-';
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

if (snpCount == 0)
   verbose(1, "no matching SNPs\n");

return seq;
}


int main(int argc, char *argv[])
/* read snpTable, generate skinny sequence for chrom */
{
char fileName[64];
FILE *f;
struct dnaSeq *skinnySeq = NULL;

if (argc != 5)
    usage();

database = argv[1];
hSetDb(database);
chromName = argv[2];

snpTable = argv[4];
if (!hTableExists(snpTable)) 
    errAbort("no %s table\n", snpTable);

skinnySeq = getSkinnySeq(argv[3], chromName);
safef(fileName, ArraySize(fileName), "%s.fat", chromName);
f = mustOpen(fileName, "w");
/* read through to each mark, printing and consuming */
carefulClose(&f);

return 0;
}

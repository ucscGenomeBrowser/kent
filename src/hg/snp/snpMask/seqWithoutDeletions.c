/* seqWithoutDeletions -- for a given chrom, generate "skinny" sequence: that is, exclude deletions. */
/* sequence file is separate input -- could also get from chromInfo */
/* write to chrom.skinny */
/* no input filtering in this program */
/* assert that coords don't exceed chrom size */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hdb.h"


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
int snpCount = 0;

char *snpChrom = NULL;
char *rsId = NULL;

verbose(1, "sequence file = %s\n", sequenceFile);
verbose(1, "chrom = %s\n", chromName);
chromSize = hChromSize(chromName);
verbose(1, "chromSize = %d\n", chromSize);
seq = hFetchSeq(sequenceFile, chromName, 0, chromSize);
// seq = hLoadChrom(chromName);
touppers(seq->dna);
seqPtr = seq->dna;

sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name from %s", snpTable);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpChrom = cloneString(row[0]);
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    rsId = cloneString(row[3]);

    if (!sameString(snpChrom, chromName)) continue;

    assert (end < chromSize);
    assert (end > start);

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
stripChar(skinnySeq->dna, '-');
safef(fileName, ArraySize(fileName), "%s.skinny", chromName);
f = mustOpen(fileName, "w");
// faWriteNext(f, chromName, skinnySeq->dna, strlen(skinnySeq->dna));
fprintf(f, ">%s\n", chromName);
writeSeqWithBreaks(f, skinnySeq->dna, strlen(skinnySeq->dna), 50);
carefulClose(&f);

return 0;
}

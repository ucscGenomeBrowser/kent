/* seqWithInsertions -- for a given chrom, generate "fat" sequence: that is, include insertions. */
/* sequence file is separate input -- could also get from chromInfo */
/* write to chrom.fat */
/* no input filtering in this program */
/* assert that coords don't exceed chrom size */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hdb.h"
#include "twoBit.h"


static char *database = NULL;
static char *chromName = NULL;
static char *snpTable = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "seqWithInsertions - generate sequence that includes insertions\n"
    "usage:\n"
    "    seqWithInsertions database chrom sequenceFile snpTable\n");
}


void getSeq(char *sequenceFile, char *chromName)
/* interleave chrom sequence with observed sequence */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

struct twoBitFile *tbf;

char fileName[64];
FILE *f;

struct dnaSeq *leftFlank = NULL;
struct dnaSeq *rightFlank = NULL;

int start = 0;
int end = 0;
int chromSize = 0;

char *snpChrom = NULL;
char *rsId = NULL;
char *strand = NULL;
char *observed = NULL;
char *subString = NULL;

int seqPos = 0;

chromSize = hChromSize(chromName);

safef(fileName, ArraySize(fileName), "%s.fat", chromName);
f = mustOpen(fileName, "w");

tbf = twoBitOpen(sequenceFile);

sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, strand, observed from %s", snpTable);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpChrom = cloneString(row[0]);
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    rsId = cloneString(row[3]);
    strand = cloneString(row[4]);
    observed = cloneString(row[5]);

    if (!sameString(snpChrom, chromName)) continue;

    assert (start < chromSize);
    assert (end == start);

    /* skip duplicates (we've already checked that the observed matches) */
    if (start == seqPos) continue;

    /* add the left flank */
    leftFlank = twoBitReadSeqFrag(tbf, chromName, seqPos, start);
    touppers(leftFlank->dna);
    writeSeqWithBreaks(f, leftFlank->dna, leftFlank->size, 50);
    dnaSeqFree(&leftFlank);
    seqPos = start;

    /* add the observed string */
    subString = cloneString(observed);
    subString = subString + 2;
    if (sameString(strand, "-"))
        reverseComplement(subString, strlen(subString));
    writeSeqWithBreaks(f, subString, strlen(subString), 50);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

/* add the final (right) flank */
/* if no snps were found, this just writes the full sequence */
rightFlank = twoBitReadSeqFrag(tbf, chromName, seqPos, chromSize);
touppers(rightFlank->dna);
writeSeqWithBreaks(f, rightFlank->dna, rightFlank->size, 50);
dnaSeqFree(&rightFlank);

carefulClose(&f);
}


int main(int argc, char *argv[])
/* read snpTable, generate fat sequence for each chrom */
{

if (argc != 5)
    usage();

database = argv[1];
hSetDb(database);
chromName = argv[2];

snpTable = argv[4];
if (!hTableExists(snpTable)) 
    errAbort("no %s table\n", snpTable);

getSeq(argv[3], chromName);

return 0;
}

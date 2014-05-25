/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* snpOrtho - read an ortho file with format:
   chrom chromStart chromEnd
   name (rsId) score strand allele */
/* Read this into a hash */
/* Then read (human) snp table serially. */
/* Output:
   chrom chromStart chromEnd (human)
   name score strand
   refUCSC observed class locType weight
   orthoChrom orthoStart orthoEnd orthoScore orthoStrand orthoAllele */

#include "common.h"
#include "hash.h"
#include "hdb.h"


struct hash *orthoHash = NULL;

struct orthoSnp 
    {
    char *chrom;
    int start;
    int end;
    int score;
    char *strand;
    char *allele;
    };


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpOrtho - read ortho file; generate ortho table \n"
    "usage:\n"
    "    snpOrtho database snpTable orthoFile\n");
}


struct hash *getOrtho(char *inputFileName)
/* put all orthoSnp objects in ret hash */
{
struct lineFile *lf = NULL;
char *line;
char *row[7];
int elementCount;

struct orthoSnp *orthoSnpInstance = NULL;
struct hash *ret = newHash(16);
int count = 0;

verbose(1, "opening file %s\n", inputFileName);
lf = lineFileOpen(inputFileName, TRUE);
while (lineFileNext(lf, &line, NULL))
    {
    elementCount = chopString(line, "\t", row, ArraySize(row));
    if (elementCount != 7) continue;
    count++;

    AllocVar(orthoSnpInstance);
    orthoSnpInstance->chrom = cloneString(row[0]);
    orthoSnpInstance->start = sqlUnsigned(row[1]);
    orthoSnpInstance->end = sqlUnsigned(row[2]);
    orthoSnpInstance->score = sqlUnsigned(row[4]);
    orthoSnpInstance->strand = cloneString(row[5]);
    orthoSnpInstance->allele = cloneString(row[6]);
    hashAdd(ret, cloneString(row[3]), orthoSnpInstance);
    }
lineFileClose(&lf);
verbose(1, "%d rows in hash\n", count);
return ret;
}

void writeResults(char *tableName)
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

struct hashEl *hel = NULL;
struct orthoSnp *orthoData;

char *snpId;

FILE *outputFileHandle = mustOpen("snpOrtho.tab", "w");

sqlSafef(query, sizeof(query), 
      "select chrom, chromStart, chromEnd, name, score, strand, refUCSC, observed, class, locType, weight from %s", 
      tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpId = cloneString(row[3]);
    hel = hashLookup(orthoHash, snpId);
    if (!hel) continue;
    orthoData = (struct orthoSnp *)hel->val;
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t", row[0], row[1], row[2], snpId);
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t", row[4], row[5], row[6], row[7], row[8], row[9], row[10]);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", orthoData->chrom, orthoData->start, orthoData->end);
    fprintf(outputFileHandle, "%d\t%s\t%s\n", orthoData->score, orthoData->strand, orthoData->allele);
    }

carefulClose(&outputFileHandle);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* generate snpOrtho table from snp ortho file */
/* combine with snp table */
{
if (argc != 4)
    usage();

hSetDb(argv[1]);
if (!hTableExistsDb(argv[1], argv[2]))
    errAbort("can't get table %s\n", argv[2]);
if (!fileExists(argv[3]))
    errAbort("can't find %s\n", argv[3]);

orthoHash = getOrtho(argv[3]);
writeResults(argv[2]);

return 0;
}

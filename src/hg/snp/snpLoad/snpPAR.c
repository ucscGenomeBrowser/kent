/* snpPAR - get PAR SNPs for chrY from chrX. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hdb.h"


static char *snpDb = NULL;
FILE *outputFileHandle = NULL;
FILE *exceptionFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpPAR - get PAR SNPs for chrY from chrX\n"
    "usage:\n"
    "    snpPAR snpDb \n");
}


void getSnps()
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int start = 0;
int end = 0;
int bin = 0;

sqlSafef(query, sizeof(query), "select * from chrX_snp126 where chromEnd < 2709520");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t", 
                              row[0], "chrY", row[2], row[3], row[4], row[5], row[6], row[7], row[8]);
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
                              row[9], row[10], row[11], row[12], row[13], row[14], row[15], row[16], row[17]);
    }
sqlFreeResult(&sr);

sqlSafef(query, sizeof(query), "select * from chrX_snp126 where chromEnd > 154584237");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = sqlUnsigned(row[2]);
    end = sqlUnsigned(row[3]);
    start = start - 97140800;
    end = end - 97140800;
    bin = hFindBin(start, end);
    fprintf(outputFileHandle, "%d\t%s\t%d\t%d\t%s\t%s\t%s\t%s\t%s\t", 
                              bin, "chrY", start, end, row[4], row[5], row[6], row[7], row[8]);
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", 
                              row[9], row[10], row[11], row[12], row[13], row[14], row[15], row[16], row[17]);
    }
sqlFreeResult(&sr);

hFreeConn(&conn);
}

void getExceptions()
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int start = 0;
int end = 0;

sqlSafef(query, sizeof(query), "select * from snp126Exceptions where chrom = 'chrX' and chromEnd < 2709520");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    fprintf(exceptionFileHandle, "chrY\t%s\t%s\t%s\t%s\n", row[1], row[2], row[3], row[4]);
sqlFreeResult(&sr);

sqlSafef(query, sizeof(query), "select * from snp126Exceptions where chrom = 'chrX' and chromEnd > 154584237");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    start = start - 97140800;
    end = end - 97140800;
    fprintf(exceptionFileHandle, "chrY\t%d\t%d\t%s\t%s\n", start, end, row[3], row[4]);
    }
sqlFreeResult(&sr);

hFreeConn(&conn);
}


void loadDatabase()
{
FILE *f;
struct sqlConnection *conn = hAllocConn();

f = mustOpen("chrY_snp126.tab", "r");
hgLoadNamedTabFile(conn, ".", "chrY_snp126", "snpPAR", &f);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
{

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
outputFileHandle = mustOpen("snpPAR.tab", "w");
exceptionFileHandle = mustOpen("snpPARexceptions.tab", "w");
getSnps();
getExceptions();
loadDatabase();
carefulClose(&outputFileHandle);
carefulClose(&exceptionFileHandle);

return 0;
}

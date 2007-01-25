/* hapmapCondense -- determine allele1 and allele2 */
/* Input is table with chrom/chromStart/chromEnd/name/score/strand/observed */
/* and 16 counts. */
/* This will condense it to 8 counts. */
/* We have already confirmed that everything is biallelic */
/* We assert that here */
/* allele2 will be "?" if all populations are monomorphic */

/* There may be missing populations */
/* Don't log this (this will be logged in validation) */

/* Write to output file, possibly load into db */

/* Need to retain observed because it might be complex */

/* Output is of the form:     */
    /* int bin;               */
    /* char *chrom;           */
    /* int  chromStart;       */
    /* int  chromEnd;         */
    /* char *name;            */
    /* int  score;            */
    /* char *strand;          */
    /* char *observed;        */

    /* char *allele1;         */
    /* int  allele1CountCEU;    */
    /* int  allele1CountCHB;    */
    /* int  allele1CountJPT;    */
    /* int  allele1CountYRI;    */

    /* char *allele2;         */
    /* int  allele2CountCEU;    */
    /* int  allele2CountCHB;    */
    /* int  allele2CountJPT;    */
    /* int  allele2CountYRI;    */

#include "common.h"

#include "hdb.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: hapmapCondense.c,v 1.1 2007/01/25 01:17:31 heather Exp $";

FILE *outputFileHandle = NULL;
FILE *errorFileHandle = NULL;

struct hap16
    {
    int bin;
    char *chrom;
    int  chromStart;
    int  chromEnd;
    char *name;
    int  score;
    char *strand;
    char *observed;

    int  aCountCEU;       
    int  cCountCEU;       
    int  gCountCEU;       
    int  tCountCEU;       

    int  aCountCHB;       
    int  cCountCHB;       
    int  gCountCHB;
    int  tCountCHB;

    int  aCountJPT;       
    int  cCountJPT;       
    int  gCountJPT;
    int  tCountJPT;

    int  aCountYRI;       
    int  cCountYRI;       
    int  gCountYRI;       
    int  tCountYRI;       
    };

struct alleleSummary
    {
    int aCount;
    int cCount;
    int gCount;
    int tCount;
    int overallCount; /* 0 for degenerate case, 1 for monomorphic, 2 for biallelic, 3 for triallelic, 4 for quadallelic */
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hapmapCondense - Read hapmap table with 16 counts, condense to 8 counts.\n"
  "usage:\n"
  "  hapmapCondense database table\n");
}

void load(char **row, struct hap16 *h16)
{
h16->bin = sqlUnsigned(row[0]);
h16->chrom = cloneString(row[1]);
h16->chromStart = sqlUnsigned(row[2]);
h16->chromEnd = sqlUnsigned(row[3]);
h16->name = cloneString(row[4]);
h16->score = sqlUnsigned(row[5]);
h16->strand = cloneString(row[6]);
h16->observed = cloneString(row[7]);

h16->aCountCEU = sqlUnsigned(row[8]);       
h16->aCountCHB = sqlUnsigned(row[9]);       
h16->aCountJPT = sqlUnsigned(row[10]);       
h16->aCountYRI = sqlUnsigned(row[11]);       

h16->cCountCEU = sqlUnsigned(row[12]);       
h16->cCountCHB = sqlUnsigned(row[13]);       
h16->cCountJPT = sqlUnsigned(row[14]);       
h16->cCountYRI = sqlUnsigned(row[15]);       

h16->gCountCEU = sqlUnsigned(row[16]);       
h16->gCountCHB = sqlUnsigned(row[17]);
h16->gCountJPT = sqlUnsigned(row[18]);
h16->gCountYRI = sqlUnsigned(row[19]);       

h16->tCountCEU = sqlUnsigned(row[20]);       
h16->tCountCHB = sqlUnsigned(row[21]);
h16->tCountJPT = sqlUnsigned(row[22]);
h16->tCountYRI = sqlUnsigned(row[23]);       
}



void writeCounts(struct hap16 *h16, struct alleleSummary *summary)
/* Output is of the form:     */
    /* char *allele1 */
    /* int  allele1CountCEU;    */
    /* int  allele1CountCHB;    */
    /* int  allele1CountJPT;    */
    /* int  allele1CountYRI;    */

    /* char *allele2 */
    /* int  allele2CountCEU;    */
    /* int  allele2CountCHB;    */
    /* int  allele2CountJPT;    */
    /* int  allele2CountYRI;    */
{

if (summary->aCount > 0)
    fprintf(outputFileHandle, "A %d %d %d %d ", h16->aCountCEU, h16->aCountCHB,
                                                h16->aCountJPT, h16->aCountYRI);
if (summary->cCount > 0)
    fprintf(outputFileHandle, "C %d %d %d %d ", h16->cCountCEU, h16->cCountCHB,
                                                h16->cCountJPT, h16->cCountYRI);
if (summary->gCount > 0)
    fprintf(outputFileHandle, "G %d %d %d %d ", h16->gCountCEU, h16->gCountCHB,
                                                h16->gCountJPT, h16->gCountYRI);
if (summary->tCount > 0)
    fprintf(outputFileHandle, "T %d %d %d %d ", h16->tCountCEU, h16->tCountCHB,
                                                h16->tCountJPT, h16->tCountYRI);
if (summary->overallCount == 1)
    fprintf(outputFileHandle, "? 0 0 0 0 ");
}

void writeOutput(struct hap16 *h16, struct alleleSummary *summary)
/* write to output file */
{
fprintf(outputFileHandle, "%d %s %d %d ", h16->bin, h16->chrom, h16->chromStart, h16->chromEnd);
fprintf(outputFileHandle, "%s %d %s %s ", h16->name, h16->score, h16->strand, h16->observed);
writeCounts(h16, summary);
fprintf(outputFileHandle, "\n");
}

void getSummary(struct hap16 *h16, struct alleleSummary *summary)
/* sum up each allele over all populations */
/* assert biallelic */
{
summary->overallCount = 0;

summary->aCount = h16->aCountCEU + h16->aCountCHB + h16->aCountJPT + h16->aCountYRI;
if (summary->aCount > 0) summary->overallCount++;

summary->cCount = h16->cCountCEU + h16->cCountCHB + h16->cCountJPT + h16->cCountYRI;
if (summary->cCount > 0) summary->overallCount++;

summary->gCount = h16->gCountCEU + h16->gCountCHB + h16->gCountJPT + h16->gCountYRI;
if (summary->gCount > 0) summary->overallCount++;

summary->tCount = h16->tCountCEU + h16->tCountCHB + h16->tCountJPT + h16->tCountYRI;
if (summary->tCount > 0) summary->overallCount++;

assert(summary->overallCount <= 2);
}

void hapmapCondense(char *tableName)
/* read through input */
/* print final output form */
{
struct hap16 *h16 = NULL;
struct alleleSummary *summary = NULL;
char query[64];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

AllocVar(h16);
AllocVar(summary);
safef(query, sizeof(query), "select * from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    load(row, h16);
    getSummary(h16, summary);
    if (summary->overallCount == 0)
        {
	fprintf(errorFileHandle, "degenerate case, no data for %s\n", h16->name);
	continue;
	}
    writeOutput(h16, summary);
    }
}


int main(int argc, char *argv[])
{

if (argc != 3)
    usage();

hSetDb(argv[1]);

if (!hTableExists(argv[2])) 
    {
    verbose(1, "can't find table %s\n", argv[2]);
    return 1;
    }

outputFileHandle = mustOpen("hapmapCondense.out", "w");
errorFileHandle = mustOpen("hapmapCondense.err", "w");
hapmapCondense(argv[2]);
carefulClose(&outputFileHandle);
carefulClose(&errorFileHandle);

return 0;
}

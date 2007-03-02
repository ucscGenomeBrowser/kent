/* hapmapSummary */
/* hash in ortho alleles */
/* read through hapmapAllelesCombined */
/* don't preserve individual status */
/* used for filtering */
/* output format: */
/* bin
   chrom
   chromStart
   chromEnd
   name
   score (avHet)
   strand
   observed
   populations (set of CEU, CHB, JPT, YRI)
   majorAlleleCEU
   majorAlleleCountCEU
   totalAlleleCountCEU
   majorAlleleCHB
   majorAlleleCountCHB
   totalAlleleCountCHB
   majorAlleleJPT
   majorAlleleCountJPT
   totalAlleleCountJPT
   majorAlleleYRI
   majorAlleleCountYRI
   totalAlleleCountYRI
   chimpAllele (ACGTN or none)
   chimpAlleleQuality
   macaqueAllele (ACGTN or none)
   macaqueAlleleQuality
*/


#include "common.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hapmapSummary.c,v 1.2 2007/03/02 00:21:13 heather Exp $";

struct orthoAllele
    {
    char *allele;
    int score;
    };

struct alleleSummary
    {
    char *allele;
    int count;
    int total;
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
    "hapmapSummary - get hapmapAllelesSummary table from overall hapmapAllelesCombined and ortho tables\n"
    "usage:\n"
    "    hapmapSummary db hapmapTable hapmapOrthoTable1 hapmapOrthoTable2\n");
}


struct hash *storeAlleles(char *tableName)
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct orthoAllele *a = NULL;

ret = newHash(0);
safef(query, sizeof(query), "select name, score, orthoAllele from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(a);
    a->allele = cloneString(row[2]);
    a->score = sqlUnsigned(row[1]);
    hashAdd(ret, cloneString(row[0]), a);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

boolean gotPopulation(char **row, char *population)
{
int offset, count1, count2, hcount, sum;

if (sameString(population, "CEU"))
    offset = 0;
else if (sameString(population, "CHB"))
    offset = 1;
else if (sameString(population, "JPT"))
    offset = 2;
else if (sameString(population, "YRI"))
    offset = 3;
else 
    return FALSE;

count1 = sqlUnsigned(row[9+offset]);
count2 = sqlUnsigned(row[14+offset]);
hcount = sqlUnsigned(row[18+offset]);
sum = count1 + count2 + hcount;
if (sum > 0) return TRUE;
return FALSE;
}


struct alleleSummary *getAlleleSummary(char *allele1, char *allele1Count,
                                       char *allele2, char *allele2Count,
				       char *heteroCount)
{
int count1 = sqlUnsigned(allele1Count);
int count2 = sqlUnsigned(allele2Count);
int hcount = sqlUnsigned(heteroCount);
int sum = count1 + count2 + hcount;
struct alleleSummary *s = NULL;

AllocVar(s);
if (sum == 0)
    {
    s->allele = cloneString("none");
    s->count = 0;
    }
else if (count1 >= count2)
    {
    s->allele = cloneString(allele1);
    s->count = count1;
    }
else
    {
    s->allele = cloneString(allele2);
    s->count = count2;
    }
s->total = sum;
return s;
}

void processSnps(char *inputTableName, struct hash *orthoHash1, struct hash *orthoHash2)
/* read combined table, write summary table */
/* bin chrom chromStart chromEnd name score (avHet) strand observed
   populations (set of CEU, CHB, JPT, YRI)
   majorAlleleCEU majorAlleleCountCEU totalAlleleCountCEU
   majorAlleleCHB majorAlleleCountCHB totalAlleleCountCHB
   majorAlleleJPT majorAlleleCountJPT totalAlleleCountJPT
   majorAlleleYRI majorAlleleCountYRI totalAlleleCountYRI
   chimpAllele (ACGTN or none) chimpAlleleQuality
   macaqueAllele (ACGTN or none) macaqueAlleleQuality
   */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
FILE *outputFileHandle = mustOpen("hapmapSummary.tab", "w");
struct orthoAllele *a = NULL;
struct alleleSummary *i = NULL;
int offset = 0;

safef(query, sizeof(query), "select * from %s", inputTableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    offset = 0;
    /* first 7 columns same */
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t", 
            row[0], row[1], row[2], row[3], row[4], row[5], row[6]);
    if (gotPopulation(row, "CEU"))
        fprintf(outputFileHandle, "CEU,");
    if (gotPopulation(row, "CHB"))
        fprintf(outputFileHandle, "CHB,");
    if (gotPopulation(row, "JPT"))
        fprintf(outputFileHandle, "JPT,");
    if (gotPopulation(row, "YRI"))
        fprintf(outputFileHandle, "YRI,");
    fprintf(outputFileHandle, "\t");
    i = getAlleleSummary(row[8], row[9+offset], row[13], row[14+offset], row[18+offset]);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", i->allele, i->count, i->total);
    offset++;
    i = getAlleleSummary(row[8], row[9+offset], row[13], row[14+offset], row[18+offset]);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", i->allele, i->count, i->total);
    offset++;
    i = getAlleleSummary(row[8], row[9+offset], row[13], row[14+offset], row[18+offset]);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", i->allele, i->count, i->total);
    offset++;
    i = getAlleleSummary(row[8], row[9+offset], row[13], row[14+offset], row[18+offset]);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", i->allele, i->count, i->total);

    hel = hashLookup(orthoHash1, row[4]);
    if (hel) 
        {
	a = (struct orthoAllele *)hel->val;
        fprintf(outputFileHandle, "%s\t%d\t", a->allele, a->score);
	}
    else
        fprintf(outputFileHandle, "none\t0\t");
    hel = hashLookup(orthoHash2, row[4]);
    if (hel) 
        {
	a = (struct orthoAllele *)hel->val;
        fprintf(outputFileHandle, "%s\t%d\n", a->allele, a->score);
	}
    else
        fprintf(outputFileHandle, "none\t0\n");

    }

carefulClose(&outputFileHandle);
sqlFreeResult(&sr);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* output hapmapAllelesSummary table from overall hapmapAllelesCombined and ortho tables */
{
char *database = NULL;
char *inputTableName = NULL;
char *chimpTableName = NULL;
char *macaqueTableName = NULL;
struct hash *chimpHash = NULL;
struct hash *macaqueHash = NULL;

if (argc != 5)
    usage();

database = argv[1];
inputTableName = argv[2];
chimpTableName = argv[3];
macaqueTableName = argv[4];

hSetDb(database);
if (!hTableExists(inputTableName))
    errAbort("no %s table in %s\n", inputTableName, database);
if (!hTableExists(chimpTableName))
    errAbort("no %s table in %s\n", chimpTableName, database);
if (!hTableExists(macaqueTableName))
    errAbort("no %s table in %s\n", macaqueTableName, database);

chimpHash = storeAlleles(chimpTableName);
verbose(1, "got chimp hash\n");
macaqueHash = storeAlleles(macaqueTableName);
verbose(1, "got macaque hash\n");
processSnps(inputTableName, chimpHash, macaqueHash);

return 0;
}

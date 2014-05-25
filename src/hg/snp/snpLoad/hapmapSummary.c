/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* hapmapSummary */
/* hash in ortho alleles */
/* read through hapmapSnpsCombined */
/* don't preserve individual status */
/* used for filtering */
/* output format: */
/* bin
   chrom
   chromStart
   chromEnd
   name
   score (heterozygosity defined as 2pq)
   strand
   observed
   popCount
   isMixed (YES or NO)
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
#include "hapmapSnpsCombined.h"


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
    "hapmapSummary - create hapmapAllelesSummary table from overall hapmapSnpsCombined and ortho tables\n"
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
sqlSafef(query, sizeof(query), "select name, score, orthoAllele from %s", tableName);
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


struct alleleSummary *getAlleleSummary(char *allele1, int allele1Count, char *allele2, int allele2Count, int heteroCount)
/* convert from individuals to alleles */
{
int sum = 2 * (allele1Count + allele2Count + heteroCount);
struct alleleSummary *s = NULL;

AllocVar(s);
if (sum == 0)
    {
    s->allele = cloneString("none");
    s->count = 0;
    }
else if (allele1Count >= allele2Count)
    {
    s->allele = cloneString(allele1);
    s->count = (2 * allele1Count) + heteroCount;
    }
else
    {
    s->allele = cloneString(allele2);
    s->count = (2 * allele2Count) + heteroCount;
    }
s->total = sum;
return s;
}



void processSnps(char *inputTableName, struct hash *orthoHash1, struct hash *orthoHash2)
/* read combined table, write summary table */
/* bin chrom chromStart chromEnd name score (heterozygosity) strand observed
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
struct hapmapSnpsCombined *thisItem = NULL;
struct orthoAllele *a = NULL;
struct alleleSummary *as = NULL;
int popCount = 0;
int score = 0;

sqlSafef(query, sizeof(query), "select * from %s", inputTableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    thisItem = hapmapSnpsCombinedLoad(row + 1);
    score = calcHet(thisItem);
    /* update score (column 5); otherwise first 9 columns are the same */
    fprintf(outputFileHandle, "%s\t%d\t%d\t%s\t%d\t%s\t%s\t%s\t%s\t", 
	    thisItem->chrom, thisItem->chromStart, thisItem->chromEnd, thisItem->name, 
	    score, thisItem->strand, thisItem->observed, thisItem->allele1, thisItem->allele2);
    popCount = getPopCount(thisItem);
    fprintf(outputFileHandle, "%d\t", popCount);
    if (isMixed(thisItem))
        fprintf(outputFileHandle, "YES\t");
    else
        fprintf(outputFileHandle, "NO\t");

    as = getAlleleSummary(thisItem->allele1, thisItem->homoCount1CEU, 
                         thisItem->allele2, thisItem->homoCount2CEU,
			 thisItem->heteroCountCEU);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", as->allele, as->count, as->total);

    as = getAlleleSummary(thisItem->allele1, thisItem->homoCount1CHB, 
                         thisItem->allele2, thisItem->homoCount2CHB,
			 thisItem->heteroCountCHB);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", as->allele, as->count, as->total);

    as = getAlleleSummary(thisItem->allele1, thisItem->homoCount1JPT, 
                         thisItem->allele2, thisItem->homoCount2JPT,
			 thisItem->heteroCountJPT);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", as->allele, as->count, as->total);

    as = getAlleleSummary(thisItem->allele1, thisItem->homoCount1YRI, 
                         thisItem->allele2, thisItem->homoCount2YRI,
			 thisItem->heteroCountYRI);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", as->allele, as->count, as->total);

    hel = hashLookup(orthoHash1, thisItem->name);
    if (hel) 
        {
	a = (struct orthoAllele *)hel->val;
        fprintf(outputFileHandle, "%s\t%d\t", a->allele, a->score);
	}
    else
        fprintf(outputFileHandle, "none\t0\t");
    hel = hashLookup(orthoHash2, thisItem->name);
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
/* output hapmapAllelesSummary table from overall hapmapSnpsCombined and ortho tables */
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

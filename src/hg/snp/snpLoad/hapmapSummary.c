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
   popCount
   isMixed (TRUE or FALSE)
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
#include "hapmapAllelesCombined.h"

static char const rcsid[] = "$Id: hapmapSummary.c,v 1.4 2007/03/06 00:17:59 heather Exp $";

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

int getPopCount(struct hapmapAllelesCombined *hmac)
/* move this to kent/src/hg/lib/hapmapAllelesCombined.c */
{
int ret = 0;
if (hmac->allele1CountCEU > 0 || hmac->allele2CountCEU > 0 || hmac->heteroCountCEU > 0) ret++;
if (hmac->allele1CountCHB > 0 || hmac->allele2CountCHB > 0 || hmac->heteroCountJPT > 0) ret++;
if (hmac->allele1CountJPT > 0 || hmac->allele2CountJPT > 0 || hmac->heteroCountCHB > 0) ret++;
if (hmac->allele1CountYRI > 0 || hmac->allele2CountYRI > 0 || hmac->heteroCountYRI > 0) ret++;
return ret;
}


struct alleleSummary *getAlleleSummary(char *allele1, int allele1Count, char *allele2, int allele2Count, int heteroCount)
{
int sum = allele1Count + allele2Count + heteroCount;
struct alleleSummary *s = NULL;

// verbose(1, "allele1 = %s\n", allele1);
// verbose(1, "allele1Count = %d\n", allele1Count);
// verbose(1, "allele2 = %s\n", allele2);
// verbose(1, "allele2Count = %d\n", allele2Count);
// verbose(1, "heteroCount = %d\n", heteroCount);

AllocVar(s);
if (sum == 0)
    {
    s->allele = cloneString("none");
    s->count = 0;
    }
else if (allele1Count >= allele2Count)
    {
    s->allele = cloneString(allele1);
    s->count = allele1Count;
    }
else
    {
    s->allele = cloneString(allele2);
    s->count = allele2Count;
    }
s->total = sum;
return s;
}



boolean isMixed(struct hapmapAllelesCombined *hmac)
/* move this to kent/src/hg/lib/hapmapAllelesCombined.c */
/* return TRUE if different populations have a different major allele */
/* don't need to consider heteroCount */
{
int allele1Count = 0;
int allele2Count = 0;

if (hmac->allele1CountCEU >= hmac->allele2CountCEU && hmac->allele1CountCEU > 0) 
    allele1Count++;
else if (hmac->allele2CountCEU > 0)
    allele2Count++;

if (hmac->allele1CountCHB >= hmac->allele2CountCHB && hmac->allele1CountCHB > 0) 
    allele1Count++;
else if (hmac->allele2CountCHB > 0)
    allele2Count++;

if (hmac->allele1CountJPT >= hmac->allele2CountJPT && hmac->allele1CountJPT > 0) 
    allele1Count++;
else if (hmac->allele2CountJPT > 0)
    allele2Count++;

if (hmac->allele1CountYRI >= hmac->allele2CountYRI && hmac->allele1CountYRI > 0) 
    allele1Count++;
else if (hmac->allele2CountYRI > 0)
    allele2Count++;

if (allele1Count > 0 && allele2Count > 0) return TRUE;

return FALSE;
}

void printOne(struct hapmapAllelesCombined *hmac)
/* move this to kent/src/hg/lib/hapmapAllelesCombined.c */
{
verbose(1, "chrom = %s\n", hmac->chrom);
verbose(1, "chromStart = %d\n", hmac->chromStart);
verbose(1, "chromEnd = %d\n", hmac->chromEnd);
verbose(1, "name = %s\n", hmac->name);
verbose(1, "score = %d\n", hmac->score);
verbose(1, "strand = %s\n", hmac->strand);
verbose(1, "observed = %s\n", hmac->observed);
verbose(1, "allele1 = %s\n", hmac->allele1);
verbose(1, "allele1CountCEU = %d\n", hmac->allele1CountCEU);
verbose(1, "allele1CountCHB = %d\n", hmac->allele1CountCHB);
verbose(1, "allele1CountJPT = %d\n", hmac->allele1CountJPT);
verbose(1, "allele1CountYRI = %d\n", hmac->allele1CountYRI);
verbose(1, "allele2 = %s\n", hmac->allele2);
verbose(1, "allele2CountCEU = %d\n", hmac->allele2CountCEU);
verbose(1, "allele2CountCHB = %d\n", hmac->allele2CountCHB);
verbose(1, "allele2CountJPT = %d\n", hmac->allele2CountJPT);
verbose(1, "allele2CountYRI = %d\n", hmac->allele2CountYRI);
verbose(1, "heteroCountCEU = %d\n", hmac->heteroCountCEU);
verbose(1, "heteroCountCHB = %d\n", hmac->heteroCountCHB);
verbose(1, "heteroCountJPT = %d\n", hmac->heteroCountJPT);
verbose(1, "heteroCountYRI = %d\n", hmac->heteroCountYRI);
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
struct hapmapAllelesCombined *hmac = NULL;
struct orthoAllele *a = NULL;
struct alleleSummary *i = NULL;
int popCount = 0;

safef(query, sizeof(query), "select * from %s", inputTableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hmac = hapmapAllelesCombinedLoad(row + 1);
    /* first 7 columns same */
    fprintf(outputFileHandle, "%s\t%d\t%d\t%s\t%d\t%s\t%s\t%s\t%s\t", 
	    hmac->chrom, hmac->chromStart, hmac->chromEnd, hmac->name, 
	    hmac->score, hmac->strand, hmac->observed, hmac->allele1, hmac->allele2);
    popCount = getPopCount(hmac);
    fprintf(outputFileHandle, "%d\t", popCount);
    if (isMixed(hmac))
        fprintf(outputFileHandle, "TRUE\t");
    else
        fprintf(outputFileHandle, "FALSE\t");

    i = getAlleleSummary(hmac->allele1, hmac->allele1CountCEU, 
                         hmac->allele2, hmac->allele2CountCEU,
			 hmac->heteroCountCEU);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", i->allele, i->count, i->total);

    i = getAlleleSummary(hmac->allele1, hmac->allele1CountCHB, 
                         hmac->allele2, hmac->allele2CountCHB,
			 hmac->heteroCountCHB);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", i->allele, i->count, i->total);

    i = getAlleleSummary(hmac->allele1, hmac->allele1CountJPT, 
                         hmac->allele2, hmac->allele2CountJPT,
			 hmac->heteroCountJPT);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", i->allele, i->count, i->total);

    i = getAlleleSummary(hmac->allele1, hmac->allele1CountYRI, 
                         hmac->allele2, hmac->allele2CountYRI,
			 hmac->heteroCountYRI);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", i->allele, i->count, i->total);

    hel = hashLookup(orthoHash1, hmac->name);
    if (hel) 
        {
	a = (struct orthoAllele *)hel->val;
        fprintf(outputFileHandle, "%s\t%d\t", a->allele, a->score);
	}
    else
        fprintf(outputFileHandle, "none\t0\t");
    hel = hashLookup(orthoHash2, hmac->name);
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

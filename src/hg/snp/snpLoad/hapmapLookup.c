/* hapmapLookup - standalone utility program to check hapmap data against dbSNP. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


FILE *errorFileHandle = NULL;
FILE *logFileHandle = NULL;

struct snpSubset 
    {
    char *chrom;
    int start;
    int end;
    char strand;
    char *observed;
    char *class;
    char *locType;
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
    "hapmapLookup - get coords using rsId\n"
    "usage:\n"
    "    hapmapLookup db hapmapTable snpTable exceptionTable \n");
}

#define PAR1X 2642881
#define PAR1Y 2642881
#define PAR2X 154494748
#define PAR2Y 57372174


boolean isPar(char *chrom, int start, int end)
/* these are hg17 coordinates */
{
if (sameString(chrom, "chrY"))
    {
    if (end < PAR1Y) return TRUE;
    if (start > PAR2Y) return TRUE;
    }
if (sameString(chrom, "chrX"))
    {
    if (end < PAR1X) return TRUE;
    if (start > PAR2X) return TRUE;
    }
return FALSE;
}

boolean isComplexObserved(char *observed)
{
if (sameString(observed, "A/C")) return FALSE;
if (sameString(observed, "A/G")) return FALSE;
if (sameString(observed, "A/T")) return FALSE;
if (sameString(observed, "C/G")) return FALSE;
if (sameString(observed, "C/T")) return FALSE;
if (sameString(observed, "G/T")) return FALSE;
return TRUE;
}

struct hash *storeExceptions(char *tableName)
/* store multiply-aligning SNPs */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

verbose(1, "reading exceptions...\n");
ret = newHash(0);
sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, exception from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!sameString(row[4], "MultipleAlignments")) continue;
    if (isPar(row[0], sqlUnsigned(row[1]), sqlUnsigned(row[2]))) continue;
    hashAdd(ret, cloneString(row[3]), NULL);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

struct hash *storeSnps(char *tableName, struct hash *exceptionHash)
/* store subset data for SNPs in a hash */
/* exclude SNPs that align multiple places */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct snpSubset *subsetElement = NULL;
struct hashEl *hel = NULL;

verbose(1, "creating SNP hash...\n");
ret = newHash(16);
sqlSafef(query, sizeof(query), 
      "select name, chrom, chromStart, chromEnd, strand, observed, class, locType from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(exceptionHash, row[0]);
    if (hel != NULL) continue;
    AllocVar(subsetElement);
    subsetElement->chrom = cloneString(row[1]);
    subsetElement->start = sqlUnsigned(row[2]);
    subsetElement->end = sqlUnsigned(row[3]);
    subsetElement->strand = row[4][0];
    subsetElement->observed = cloneString(row[5]);
    subsetElement->class = cloneString(row[6]);
    subsetElement->locType = cloneString(row[7]);
    hashAdd(ret, cloneString(row[0]), subsetElement);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

boolean samePosition(char **row, struct snpSubset *ss)
/* simple comparison function */
{
/* must be simple */
assert(sameString(ss->class, "single"));
assert(sameString(ss->locType, "exact"));
assert(ss->end == ss->start + 1);

/* allow for PAR */
if (sameString(row[0], "chrX") && sameString(ss->chrom, "chrX"))
    {
    if (sqlUnsigned(row[1]) < PAR1X && ss->start < PAR1X) return TRUE;
    if (sqlUnsigned(row[1]) > PAR2X && ss->start > PAR2X) return TRUE;
    }
if (sameString(row[0], "chrX") && sameString(ss->chrom, "chrY"))
    {
    if (sqlUnsigned(row[1]) < PAR1X && ss->start < PAR1Y) return TRUE;
    if (sqlUnsigned(row[1]) > PAR2X && ss->start > PAR2Y) return TRUE;
    }
if (sameString(row[0], "chrY") && sameString(ss->chrom, "chrX"))
    {
    if (sqlUnsigned(row[1]) < PAR1Y && ss->start < PAR1X) return TRUE;
    if (sqlUnsigned(row[1]) > PAR2Y && ss->start > PAR2X) return TRUE;
    }
if (sameString(row[0], "chrY") && sameString(ss->chrom, "chrY"))
    {
    if (sqlUnsigned(row[1]) < PAR1Y && ss->start < PAR1Y) return TRUE;
    if (sqlUnsigned(row[1]) > PAR2Y && ss->start > PAR2Y) return TRUE;
    }

if (differentString(row[0], ss->chrom)) return FALSE;
if (sqlUnsigned(row[1]) != ss->start) return FALSE;
if (sqlUnsigned(row[2]) != ss->end) return FALSE;
return TRUE;
}

boolean reverseComplemented(char **row, struct snpSubset *ss)
{
char *rc;
int i, len;

len = strlen(ss->observed);
rc = needMem(len + 1);
rc = cloneString(ss->observed);
for (i = 0; i < len; i = i+2)
    {
    char c = ss->observed[i];
    rc[len-i-1] = ntCompTable[(int)c];
    }

if (sameString(rc, row[5])) return TRUE;

return FALSE;
}

boolean strandMatch(char **row, struct snpSubset *ss)
{
if (row[4][0] != ss->strand) return FALSE;
return TRUE;
}

void checkObservedAndStrand(char **row, struct snpSubset *ss)
{
boolean sameStrand = strandMatch(row, ss);
char *rsId = cloneString(row[3]);
char *hapmapObserved = cloneString(row[5]);

/* we know that ss has valid observed */
assert(differentString(ss->observed, "unknown"));
assert(differentString(ss->observed, "n/a"));

if (sameStrand && sameString(ss->observed, hapmapObserved)) return;

if (!sameStrand)
    {
    /* perhaps this is a reverse complement */
    if (reverseComplemented(row, ss))
        {
        fprintf(errorFileHandle, "snp %s reverse complemented\n", rsId);
	return;
	}

    if (sameString(hapmapObserved, ss->observed))
        {
        fprintf(errorFileHandle, "snp %s strand mismatch\n", rsId);
        return;
        }

    fprintf(errorFileHandle, "snp %s: hapmap observed and strand issue\n", rsId);
    return;
    }
        
fprintf(errorFileHandle, "snp %s observed mismatch\n", rsId);
        
}


boolean isComplexDbSnp(struct snpSubset *ss)
/* not including observed in this */
{
if (ss->end != ss->start + 1) return TRUE;
if (differentString(ss->class, "single")) return TRUE;
if (differentString(ss->locType, "exact")) return TRUE;
return FALSE;
}

void processSnps(struct hash *snpHash, struct hash *exceptionHash, char *hapmapTable)
/* read hapmapTable, lookup in snpHash */
/* report if SNP missing (could be due to multiple alignment, handle this if necessary) */
/* report if location is different */
/* report if observed is different */
/* report if strand is different */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
struct hashEl *hel2 = NULL;
struct snpSubset *subsetElement = NULL;
char *rsId = NULL;

verbose(1, "process SNPs...\n");
sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, strand, observed from %s", hapmapTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rsId = cloneString(row[3]);
    hel = hashLookup(snpHash, rsId);
    if (hel == NULL) 
        {
	hel2 = hashLookup(exceptionHash, rsId);
	if (hel2 == NULL)
	    fprintf(errorFileHandle, "%s not found\n", rsId);
	else
	    fprintf(errorFileHandle, "%s aligns multiple places\n", rsId);
	continue;
	}
    subsetElement = (struct snpSubset *)hel->val;

    if (sameString(subsetElement->observed, "unknown") || sameString(subsetElement->observed, "n/a"))
        {
	if (!strandMatch(row, subsetElement))
            fprintf(errorFileHandle, "snp %s strand mismatch (dbSNP observed unknown)\n", rsId);
	continue;
	}

    char *hapmapObserved = cloneString(row[5]);

    if (!isComplexObserved(subsetElement->observed) && !isComplexObserved(hapmapObserved))
        {
        checkObservedAndStrand(row, subsetElement);
        }

    if (isComplexDbSnp(subsetElement))
        {
	fprintf(logFileHandle, "%s %s %d %d %c %s %s %s\n", rsId, subsetElement->chrom, 
	                       subsetElement->start, subsetElement->end, subsetElement->strand,
			       subsetElement->observed, subsetElement->class, subsetElement->locType);
	/* could print strand here, too */
	if (differentString(subsetElement->observed, hapmapObserved))
	    {
	    if (isComplexObserved(subsetElement->observed) && !isComplexObserved(hapmapObserved))
	        fprintf(errorFileHandle, "%s: snp complex observed = %s, hapmap simple observed = %s\n", 
		        rsId, subsetElement->observed, hapmapObserved);
	    if (!isComplexObserved(subsetElement->observed) && isComplexObserved(hapmapObserved))
	        fprintf(errorFileHandle, "%s: snp simple observed = %s, hapmap complex observed = %s\n", 
		        rsId, subsetElement->observed, hapmapObserved);
	    if (isComplexObserved(subsetElement->observed) && isComplexObserved(hapmapObserved))
	        fprintf(errorFileHandle, "%s: snp complex observed = %s, hapmap complex observed = %s\n", 
		        rsId, subsetElement->observed, hapmapObserved);
	    }
	continue;
	}

    if (!samePosition(row, subsetElement))
        {
	fprintf(errorFileHandle, "position mismatch for snp %s\n", rsId);
	continue;
	}

    }
}


int main(int argc, char *argv[])
/* load SNPs into hash */
{
char *snpDb = NULL;
char *hapmapTableName = NULL;
char *snpTableName = NULL;
char *exceptionTableName = NULL;

struct hash *snpHash = NULL;
struct hash *exceptionHash = NULL;

if (argc != 5)
    usage();

snpDb = argv[1];
hapmapTableName = argv[2];
snpTableName = argv[3];
exceptionTableName = argv[4];

/* initialize ntCompTable */
dnaUtilOpen();

/* process args */
hSetDb(snpDb);
if (!hTableExists(hapmapTableName))
    errAbort("no %s table in %s\n", hapmapTableName, snpDb);
if (!hTableExists(snpTableName))
    errAbort("no %s table in %s\n", snpTableName, snpDb);
if (!hTableExists(exceptionTableName))
    errAbort("no %s table in %s\n", exceptionTableName, snpDb);

errorFileHandle = mustOpen("hapmapLookup.error", "w");
logFileHandle = mustOpen("hapmapLookup.log", "w");

exceptionHash = storeExceptions(exceptionTableName);
snpHash = storeSnps(snpTableName, exceptionHash);
verbose(1, "finished creating dbSNP hash\n");
processSnps(snpHash, exceptionHash, hapmapTableName);

carefulClose(&errorFileHandle);
carefulClose(&logFileHandle);

return 0;
}

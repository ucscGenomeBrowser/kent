/* illuminaLookup - validate illumina SNP arrays. */
/* report if SNP missing */
/* report unexpected position */
/* check strand */
/* check observed */
/* report if class != single */
/* report if locType != exact */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


struct snpSubset 
/* This is just a list of some of the snps.  We're not wanting to drag in snp127, snp128, etc. 
 * This is stored in a hash keyed by snpId, so we don't bother storing the snpId here. */
    {
    char *chrom;
    int start;
    int end;
    char *strand;
    char *observed;
    char *class;
    char *locType;
    };

static void usage()
/* Explain usage and exit. */
{
errAbort(
    "illuminaLookup - validation\n"
    "usage:\n"
    "    illuminaLookup db illuminaTable snpTable exceptionTable errorFile\n");
}


static struct hash *readMultipleAlignmentExceptions(char *tableName)
/* Read in MultipleAlignment exceptions from table and store in a hash keyed by snp ID. */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

verbose(1, "reading exceptions...\n");
ret = newHash(0);
sqlSafef(query, sizeof(query), "select name, exception from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!sameString(row[1], "MultipleAlignments")) continue;
    hashAdd(ret, cloneString(row[0]), NULL);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

static struct hash *readSnps(char *tableName, struct hash *multiMapHash)
/* Store subset data for SNPs in a hash.  Exclude SNPs that align multiple places. */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct snpSubset *subsetElement = NULL;
// FILE *errors = mustOpen(errorFileName, "w");

verbose(1, "creating SNP hash...\n");
ret = newHash(20);
sqlSafef(query, sizeof(query), 
      "select name, chrom, chromStart, chromEnd, strand, observed, class, locType from %s", 
      tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!hashLookup(multiMapHash, row[0]))
	{
	AllocVar(subsetElement);
	subsetElement->chrom = cloneString(row[1]);
	subsetElement->start = sqlUnsigned(row[2]);
	subsetElement->end = sqlUnsigned(row[3]);
	subsetElement->strand = cloneString(row[4]);
	subsetElement->observed = cloneString(row[5]);
	subsetElement->class = cloneString(row[6]);
	subsetElement->locType = cloneString(row[7]);
	hashAdd(ret, cloneString(row[0]), subsetElement);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
// carefulClose(&errors);
return ret;
}

static boolean isTriallelic(char *obs)
{
if (sameString(obs, "A/C/G")) return TRUE;
if (sameString(obs, "A/C/T")) return TRUE;
if (sameString(obs, "A/G/T")) return TRUE;
if (sameString(obs, "C/G/T")) return TRUE;
return FALSE;
}

static boolean isReverseComplement(char *obs1, char *obs2)
{
if (sameString(obs1, "A/C"))
    {
    if (sameString(obs2, "G/T")) return TRUE;
    return FALSE;
    }
if (sameString(obs1, "A/G"))
    {
    if (sameString(obs2, "C/T")) return TRUE;
    return FALSE;
    }
if (sameString(obs1, "C/T"))
    {
    if (sameString(obs2, "A/G")) return TRUE;
    return FALSE;
    }
if (sameString(obs1, "G/T"))
    {
    if (sameString(obs2, "A/C")) return TRUE;
    return FALSE;
    }
/* shouldn't get here */
return FALSE;
}

static char *reverseObserved(char *obs)
{
if (sameString(obs, "A/C"))
    return "G/T";
if (sameString(obs, "A/G"))
    return "C/T";
if (sameString(obs, "C/T"))
    return "A/G";
if (sameString(obs, "G/T"))
    return "A/C";
return obs;
}



static void logSnpStrandMismatch(struct hash *snpHash, char *illuminaTable)
/* Read illuminaTable.  Output +/+, +/-, -/+, -/- in various log files. */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
struct snpSubset *subsetElement = NULL;
int pos = 0;
char *strand = NULL;
char *observed = NULL;
FILE *plusplus = mustOpen("plusplus", "w");
FILE *minusplus = mustOpen("minusplus", "w");
FILE *plusminus = mustOpen("plusminus", "w");
FILE *minusminus = mustOpen("minusminus", "w");

verbose(1, "logging...\n");
sqlSafef(query, sizeof(query), "select name, chrom, chromStart, strand, observed from %s", illuminaTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(snpHash, row[0]);
    if (hel == NULL) continue;
    subsetElement = (struct snpSubset *)hel->val;
    if (!sameString(subsetElement->chrom, row[1])) continue;
    pos = sqlUnsigned(row[2]);
    if (pos != subsetElement->start) continue;
    strand = cloneString(row[3]);
    observed = cloneString(row[4]);
    if (sameString(strand, "+") && sameString(subsetElement->strand, "+"))
        fprintf(plusplus, "snp %s: illumina %s (%s), dbSNP %s (%s)\n",
	        row[0], observed, strand, subsetElement->observed, subsetElement->strand);
    else if (sameString(strand, "+") && sameString(subsetElement->strand, "-"))
        fprintf(plusminus, "snp %s: illumina %s (%s), dbSNP %s (%s)\n",
	        row[0], observed, strand, subsetElement->observed, subsetElement->strand);
    else if (sameString(strand, "-") && sameString(subsetElement->strand, "+"))
        fprintf(minusplus, "snp %s: illumina %s (%s), dbSNP %s (%s)\n",
	        row[0], observed, strand, subsetElement->observed, subsetElement->strand);
    else if (sameString(strand, "-") && sameString(subsetElement->strand, "-"))
        fprintf(minusminus, "snp %s: illumina %s (%s), dbSNP %s (%s)\n",
	        row[0], observed, strand, subsetElement->observed, subsetElement->strand);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&plusplus);
carefulClose(&plusminus);
carefulClose(&minusplus);
carefulClose(&minusminus);
}

static void processSnps(struct hash *snpHash, char *illuminaTable, char *errorFileName)
/* read illuminaTable */
/* lookup details in snpHash */
/* report if SNP missing */
/* report if class != single */
/* report if locType != exact */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
struct snpSubset *subsetElement = NULL;
FILE *errors = mustOpen(errorFileName, "w");
int pos = 0;
char *strand = NULL;
char *observed = NULL;

verbose(1, "process SNPs...\n");
sqlSafef(query, sizeof(query), "select name, chrom, chromStart, strand, observed from %s", illuminaTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(snpHash, row[0]);
    if (hel == NULL) 
        {
	fprintf(errors, "%s not found\n", row[0]);
	continue;
	}
    subsetElement = (struct snpSubset *)hel->val;

    if (!sameString(subsetElement->chrom, row[1]))
        {
	fprintf(errors, "unexpected chrom %s for snp %s\n", row[1], row[0]);
	continue;
	}

    pos = sqlUnsigned(row[2]);
    if (pos != subsetElement->start)
        {
        fprintf(errors, "unexpected position %d for snp %s\n", pos, row[0]);
	continue;
	}

    if (!sameString(subsetElement->class, "single"))
        {
	fprintf(errors, "unexpected class %s for snp %s\n", subsetElement->class, row[0]);
	continue;
	}

    strand = cloneString(row[3]);
    observed = cloneString(row[4]);

    if (sameString(subsetElement->observed, "A/C/G/T"))
        {
	fprintf(errors, "snp %s is quad-allelic in dbSNP\n", row[0]);
	continue;
	}

    if (isTriallelic(subsetElement->observed))
        {
	fprintf(errors, "snp %s is tri-allelic in dbSNP\n", row[0]);
	continue;
	}
        

    /* illumina positive strand - just compare observed */
    /* this seems odd... */
    if (sameString(strand, "+") && differentString(observed, subsetElement->observed))
	fprintf(errors, "observed mismatch for %s: illumina %s (%s), dbSNP %s (%s)\n", 
	        row[0], observed, strand, subsetElement->observed, subsetElement->strand);

    /* illumina negative strand - reverse complement and compare */
    if (sameString(strand, "-"))
        {
        if (differentString(reverseObserved(observed), subsetElement->observed))
	    fprintf(errors, "observed mismatch for %s: illumina %s (%s), dbSNP %s (%s)\n", 
	            row[0], observed, strand, subsetElement->observed, subsetElement->strand);
	}

    if (!sameString(subsetElement->locType, "exact"))
	fprintf(errors, "unexpected locType %s for snp %s\n", subsetElement->locType, row[0]);

    }

sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&errors);
}


int main(int argc, char *argv[])
/* load SNPs into hash */
{
char *snpDb = NULL;
char *illuminaTableName = NULL;
char *snpTableName = NULL;
char *exceptionTableName = NULL;
char errorFileName[64];

struct hash *snpHash = NULL;
struct hash *multiMapHash = NULL;

if (argc != 6)
    usage();

snpDb = argv[1];
illuminaTableName = argv[2];
snpTableName = argv[3];
exceptionTableName = argv[4];
safef(errorFileName, ArraySize(errorFileName), "%s", argv[5]);

/* process args */
hSetDb(snpDb);
if (!hTableExists(illuminaTableName))
    errAbort("no %s table in %s\n", illuminaTableName, snpDb);
if (!hTableExists(snpTableName))
    errAbort("no %s table in %s\n", snpTableName, snpDb);
if (!hTableExists(exceptionTableName))
    errAbort("no %s table in %s\n", exceptionTableName, snpDb);

multiMapHash = readMultipleAlignmentExceptions(exceptionTableName);
verbose(1, "Read %d multiply mapped SNPS\n", multiMapHash->elCount);
snpHash = readSnps(snpTableName, multiMapHash);
verbose(1, "Read %d singly SNPS\n", snpHash->elCount);
logSnpStrandMismatch(snpHash, illuminaTableName);
processSnps(snpHash, illuminaTableName, errorFileName);

return 0;
}

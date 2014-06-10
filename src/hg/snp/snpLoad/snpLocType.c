/* snpLocType - third step in dbSNP processing.
 * Read the chrN_snpTmp tables created by snpSplitSimple and handle locType.
 * Rewrite to new chrN_snpTmp tables.  Get chromInfo from ContigInfo.  */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "dystring.h"
#include "hdb.h"


static char *snpDb = NULL;
static char *contigGroup = NULL;
FILE *errorFileHandle = NULL;
FILE *exceptionFileHandle = NULL;

/* these are described in b125_mapping.doc */
/* Also snpFixed.LocTypeCode */
/* not currently used here */
char *locTypeStrings[] = {
    "unknown",
    "range",
    "exact",
    "between",
    "rangeInsertion",
    "rangeSubstitution",
    "rangeDeletion",
};

/* Errors detected here (written to snpLoctype.error, rows skipped): */
/* Unknown locType */
/* Between with end != start + 1 */
/* Between with allele != '-' */
/* Exact with end != start */
/* Range with end < start */

/* Exceptions detected: */
/* RefAlleleWrongSize */


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpLocType - handle locType in chrN_snpTmp\n"
    "usage:\n"
    "    snpLocType snpDb contigGroup\n");
}


struct slName *getChromListFromContigInfo(char *contigGroup)
/* get all chromNames that match contigGroup */
{
struct slName *ret = NULL;
struct slName *el = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char chromName[64];

sqlSafef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s' and contig_end != 0", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(chromName, sizeof(chromName), "%s", row[0]);
    el = slNameNew(chromName);
    slAddHead(&ret, el);
    }
sqlFreeResult(&sr);

sqlSafef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s' and contig_end = 0", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(chromName, sizeof(chromName), "%s_random", row[0]);
    el = slNameNew(chromName);
    slAddHead(&ret, el);
    }
sqlFreeResult(&sr);

hFreeConn(&conn);
return ret;
}

boolean needToExpand(char *allele)
/* return true if allele contains open paren */
{
char *openParen = NULL;
openParen = strchr(allele, '(');
if (openParen == NULL) return FALSE;
return TRUE;
}

void writeToExceptionFile(char *chrom, int start, int end, char *name, char *exception)
{
fprintf(exceptionFileHandle, "chr%s\t%d\t%d\trs%s\t%s\n", chrom, start, end, name, exception);
}


int doLocType(char *chromName)
/* read the database table, adjust, write .tab file */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
FILE *f;
int chromStart = 0;
int chromEnd = 0;
int skipCount = 0;
int locTypeInt = 0;
char *allele = NULL;
int alleleSize = 0;
int coordSpan = 0;
int expandCount = 0;

safef(tableName, ArraySize(tableName), "chr%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "chr%s_snpTmp.tab", chromName);

f = mustOpen(fileName, "w");

sqlSafef(query, sizeof(query), 
    "select snp_id, ctg_id, loc_type, start, end, orientation, allele, weight from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    locTypeInt = sqlUnsigned(row[2]);
    if (locTypeInt <= 0 || locTypeInt > 6)
        {
	skipCount++;
        fprintf(errorFileHandle, "Unknown locType for snp = %s\tchrom = %s\tchromStart = %s\tchromEnd=%s\n",
	        row[0], chromName, row[3], row[4]);
	continue;
	}

    chromStart = sqlUnsigned(row[3]);
    chromEnd = sqlUnsigned(row[4]);
    allele = cloneString(row[6]);

    /* dbSNP reports insertions with end = start + 1 */
    /* we use the convention start = end */
    /* we increment the start */
    if (locTypeInt == 3)
        {
	if (chromEnd != chromStart + 1)
	    {
	    skipCount++;
            fprintf(errorFileHandle, "Unexpected coords for between snp = %s\tchrom = %s\tchromStart = %s\tchromEnd=%s\n", 
	                          row[0], chromName, row[3], row[4]);
	    continue;
	    }
	if (!sameString(allele, "-"))
	    {
	    skipCount++;
            fprintf(errorFileHandle, "Unexpected allele %s for between snp = %s\tchrom = %s\tchromStart = %s\tchromEnd=%s\n", 
	                          allele, row[0], chromName, row[3], row[4]);
	    continue;
	    }
	chromStart++;
	}
    /* dbSNP reports exact with start == end */
    /* we increment the end */
    else if (locTypeInt == 2)
        {
	if (chromEnd != chromStart)
	    {
	    skipCount++;
            fprintf(errorFileHandle, "Unexpected coords for exact snp = %s\tchrom = %s\tchromStart = %s\tchromEnd=%s\n", 
	                          row[0], chromName, row[3], row[4]);
	    continue;
	    }
        alleleSize = strlen(allele);
	if (alleleSize != 1)
            writeToExceptionFile(chromName, chromStart, chromEnd, row[0], "RefAlleleWrongSize");
	chromEnd++;
        }
    else
        {
	if (chromEnd < chromStart)
	    {
	    skipCount++;
            fprintf(errorFileHandle, "Unexpected coords for range snp = %s\tchrom = %s\tchromStart = %s\tchromEnd=%s\n", 
	                          row[0], chromName, row[3], row[4]);
	    continue;
	    }
	chromEnd++;
	coordSpan = chromEnd - chromStart;
	alleleSize = strlen(allele);
        /* only check size if we don't need to expand (that is next step in processing) */
	if (needToExpand(allele))
	    expandCount++;
        else if (alleleSize != coordSpan)
            writeToExceptionFile(chromName, chromStart, chromEnd, row[0], "RefAlleleWrongSize");
	}

    fprintf(f, "%s\t%s\t%d\t%d\t%s\t%s\t%s\t%s\n", 
               row[0], row[1], chromStart, chromEnd, row[2], row[5], row[6], row[7]);

    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
if (skipCount > 0)
    verbose(1, "skipping %d rows\n", skipCount);
return expandCount;
}


void recreateDatabaseTable(char *chromName)
/* create a new chrN_snpTmp table with new definition */
/* could use enum for loc_type here */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    snp_id int(11) not null,\n"
"    ctg_id int(11) not null,\n"
"    chromStart int(11) not null,\n"
"    chromEnd int(11) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    orientation tinyint(4) not null,\n"
"    allele blob,\n"
"    weight int\n"
");\n";

struct dyString *dy = newDyString(1024);

safef(tableName, ArraySize(tableName), "chr%s_snpTmp", chromName);
sqlDyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
hFreeConn(&conn);
}


void loadDatabase(char *chromName)
/* load one table into database */
{
FILE *f;
struct sqlConnection *conn = hAllocConn();
char tableName[64], fileName[64];

safef(tableName, ArraySize(tableName), "chr%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "chr%s_snpTmp.tab", chromName);

f = mustOpen(fileName, "r");
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, handle locType, rewrite to individual chrom tables */
{
struct slName *chromList, *chromPtr;
int expandCount = 0;

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

chromList = getChromListFromContigInfo(contigGroup);
if (chromList == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

errorFileHandle = mustOpen("snpLocType.errors", "w");
exceptionFileHandle = mustOpen("snpLocType.exceptions", "w");

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    verbose(1, "chrom = %s\n", chromPtr->name);
    expandCount = expandCount + doLocType(chromPtr->name);
    recreateDatabaseTable(chromPtr->name);
    loadDatabase(chromPtr->name);
    }

if (expandCount > 0)
    verbose(1, "need to expand %d alleles\n", expandCount);
carefulClose(&errorFileHandle);
carefulClose(&exceptionFileHandle);
return 0;
}

/* snpExpandAllele - fourth step in dbSNP processing.
 * Read the chrN_snpTmp tables created by snpLocType and expand allele.
 * Rewrite to new chrN_snpTmp tables.  Get chromInfo from ContigInfo. 
 * Write syntax errors to snpExpandAllele.errors.
 * Write exceptions to snpExpandAllele.exceptions:
      RefAlleleWrongSize
 */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "dystring.h"
#include "hdb.h"


static char *snpDb = NULL;
static char *contigGroup = NULL;

FILE *errorFileHandle = NULL;
FILE *tabFileHandle = NULL;
FILE *exceptionFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpExpandAllele - expand allele in chrN_snpTmp\n"
    "usage:\n"
    "    snpExpandAllele snpDb contigGroup\n");
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

void writeToExceptionFile(char *chrom, int start, int end, char *name, char *exception)
{
fprintf(exceptionFileHandle, "chr%s\t%d\t%d\trs%s\t%s\n", chrom, start, end, name, exception);
}


boolean needToSplit(char *allele)
/* return true if allele contains open paren */
{
char *openParen = NULL;
openParen = strchr(allele, '(');
if (openParen == NULL) return FALSE;
return TRUE;
}

char *getLeftFlank(char *allele)
/* We know allele has an open paren (could assert that). */
/* The flank will contain all ACGT. */
/* We may or may not have any flank. */
{
int splitPos = 0;
char *leftFlank = NULL;

splitPos = strcspn(allele, "(");
leftFlank = cloneString(allele);
leftFlank[splitPos] = '\0';
return leftFlank;
}

int getRepeatCount(char *allele)
/* We believe allele starts with one or two digit number. */
{
char *localAllele = cloneString(allele);
int size = strspn(localAllele, "0123456789");
if (size != 1 && size != 2) return 0;
localAllele[size] = '\0';
return (sqlUnsigned(localAllele));
}

char *generateRepeatString(char nucleotide, int count)
/* create a string with count nucleotides */
{
int pos;
char *repeatString = needMem(count+1);

for (pos = 0; pos < count; pos++)
   repeatString[pos] = nucleotide;
repeatString[count] = '\0';

return repeatString;
}

char *expandAllele(char *startAllele)
/* look for substrings such as (A)2, (C)3, (G)4, (T)5 etc. */
/* expand to AA, CCC, GGGG, TTTTT etc. */
/* can handle multiple substitutions in one string */
/* if syntax error detected, log to errorFile and return startAllele */
/* only called if at least one expansion is required */
{
struct dyString *newAllele = newDyString(1024);
/* copy startAllele into oldAllele, which we consume as we go */
char *oldAllele = cloneString(startAllele);
char *leftFlank = NULL;
char nucleotide;
int repeatCount = 0;
char *repeatString = NULL;

while (oldAllele != NULL)
    {
    if (!needToSplit(oldAllele))
        {
	dyStringAppend(newAllele, oldAllele);
	if (!sameString(oldAllele, startAllele))
	    {
	    verbose(5, "expandAllele started with %s\n", startAllele);
	    verbose(5, "expandAllele returning %s\n", newAllele->string);
	    verbose(5, "---------------------\n");
	    }
	return (newAllele->string);
	}
    leftFlank = getLeftFlank(oldAllele);
    verbose(5, "leftFlank = %s\n", leftFlank);
    // perhaps dyStringAppend can handle NULL input?
    if (leftFlank != NULL)
        {
	dyStringAppend(newAllele, leftFlank);
	}

    oldAllele = strpbrk(oldAllele, "(");
    verbose(5, "oldAllele = %s\n", oldAllele);

    /* if syntax error, give up */
    if (strlen(oldAllele) < 3) return startAllele;

    nucleotide = oldAllele[1];
    oldAllele = oldAllele + 3;

    /* if syntax error, give up */
    if (strlen(oldAllele) < 1) return startAllele;
        
    repeatCount = getRepeatCount(oldAllele);
    /* if syntax error, give up */
    if (repeatCount == 0) return startAllele;

    verbose(5, "repeatCount = %d\n", repeatCount);
    if (repeatCount <= 9)
        oldAllele = oldAllele + 1;
    else if (repeatCount <= 99)
        oldAllele = oldAllele + 2;
    else
        oldAllele = oldAllele + 3;
    repeatString = generateRepeatString(nucleotide, repeatCount);
    verbose(5, "repeatString = %s\n", repeatString);
    dyStringAppend(newAllele, repeatString);
    }
return newAllele->string;
}

void writeToTabFile(char *snp_id, char *ctg_id, char *start, char *end, char *loc_type, char *orientation, char *allele, char *weight)
{
fprintf(tabFileHandle, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", snp_id, ctg_id, start, end, loc_type, orientation, allele, weight);
}

void writeToErrorFile(char *snp_id, char *ctg_id, char *start, char *end, char *loc_type, char *orientation, char *allele, char *weight)
{
fprintf(errorFileHandle, 
        "snp_id = %s\nctg=%s\nchromStart = %s\nchromEnd = %s\nloc_type = %s\norientation=%s\nallele = %s\nweight=%s\n", 
        snp_id, ctg_id, start, end, loc_type, orientation, allele, weight);
fprintf(errorFileHandle, "---------------------------------------\n");
}

void doExpandAllele(char *chromName)
/* call expandAllele for all rows for this chrom */
/* don't do for loc_type 2 (exact) and loc_type 3 (between) */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
char *allele = NULL;
int count = 0;
int errorCount = 0;
int start = 0;
int end = 0;
int size = 0;
char newEndString[64];

safef(tableName, ArraySize(tableName), "chr%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "chr%s_snpTmp.tab", chromName);

tabFileHandle = mustOpen(fileName, "w");
sqlSafef(query, sizeof(query), "select snp_id, ctg_id, chromStart, chromEnd, loc_type, orientation, allele, weight from %s ", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {

    /* no possible candidates in exact and between */
    if (sameString(row[4], "2") || sameString(row[4], "3"))
        {
	writeToTabFile(row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7]);
	continue;
	}

    /* pass through alleles with no parens */
    if (!needToSplit(row[6]))
	{
	writeToTabFile(row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7]);
	continue;
	}

    allele = expandAllele(row[6]);
    /* expandAllele returns original allele if syntax error detected */
    /* detect errors but not removing from data set */
    if (sameString(allele, row[6]))
        {
        errorCount++;
	writeToErrorFile(row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7]);
        }

    /* calculate end for locType 4, 5, 6 */
    if (sameString(row[4], "4") || sameString(row[4], "5") || sameString(row[4], "6"))
        {
	start = sqlUnsigned(row[2]);
        end = start + strlen(allele);
	verbose(5, "setting end coord for %s, locType = %s, startAllele = %s\n", row[0], row[4], row[6]);
	sprintf(newEndString, "%d", end);
        writeToTabFile(row[0], row[1], row[2], newEndString, row[4], row[5], allele, row[7]);
	continue;
	}

    /* check for exceptions in locType 1 */
    if (sameString(row[4], "1"))
        {
	start = sqlUnsigned(row[2]);
	end = sqlUnsigned(row[3]);
	size = end - start;
	if (size != strlen(allele))
	    writeToExceptionFile(chromName, start, end, row[0], "RefAlleleWrongSize");
	}

    writeToTabFile(row[0], row[1], row[2], row[3], row[4], row[5], allele, row[7]);

    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&tabFileHandle);
verbose(2, "%d alleles expanded\n", count);
verbose(2, "%d errors found\n", errorCount);
}


void cleanDatabaseTable(char *chromName)
/* do a 'delete from tableName' */
{
struct sqlConnection *conn = hAllocConn();
char query[256];

sqlSafef(query, ArraySize(query), "delete from chr%s_snpTmp", chromName);
sqlUpdate(conn, query);
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
/* hgLoadTabFile closes the file handle */
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, handle allele, rewrite to individual chrom tables */
{
struct slName *chromList, *chromPtr;

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

errorFileHandle = mustOpen("snpExpandAllele.errors", "w");
exceptionFileHandle = mustOpen("snpExpandAllele.exceptions", "w");

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    verbose(1, "chrom = %s\n", chromPtr->name);
    doExpandAllele(chromPtr->name);
    /* snpExpandAllele doesn't change the table format, so just delete old rows */
    cleanDatabaseTable(chromPtr->name);
    loadDatabase(chromPtr->name);
    verbose(2, "------------------------------\n");
    }

carefulClose(&errorFileHandle);
carefulClose(&exceptionFileHandle);
return 0;
}

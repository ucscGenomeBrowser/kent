/* snpExpandAllele - fourth step in dbSNP processing.
 * Read the chrN_snpTmp tables created by snpLocType and expand allele.
 * Rewrite to new chrN_snpTmp tables.  Get chromInfo from ContigInfo. */

#include "common.h"

#include "dystring.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpExpandAllele.c,v 1.9 2006/02/08 00:31:40 heather Exp $";

char *snpDb = NULL;
char *contigGroup = NULL;
static struct hash *chromHash = NULL;
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


struct hash *loadChroms(char *contigGroup)
/* hash all chromNames that match contigGroup */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *chromName;

ret = newHash(0);
safef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s'", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromName = cloneString(row[0]);
    hashAdd(ret, chromName, NULL);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

void writeToExceptionFile(char *chrom, int start, int end, char *name, char *exception)
{
fprintf(exceptionFileHandle, "%s\t", chrom);
fprintf(exceptionFileHandle, "%d\t", start);
fprintf(exceptionFileHandle, "%d\t", end);
fprintf(exceptionFileHandle, "%s\t", name);
fprintf(exceptionFileHandle, "%s\n", exception);
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
return (atoi(localAllele));
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
    else
        oldAllele = oldAllele + 2;
    repeatString = generateRepeatString(nucleotide, repeatCount);
    verbose(5, "repeatString = %s\n", repeatString);
    dyStringAppend(newAllele, repeatString);
    }
return newAllele->string;
}

void writeToTabFile(char *snp_id, char *start, char *end, char *loc_type, char *orientation, char *allele)
{
fprintf(tabFileHandle, "%s\t", snp_id);
fprintf(tabFileHandle, "%s\t", start);
fprintf(tabFileHandle, "%s\t", end);
fprintf(tabFileHandle, "%s\t", loc_type);
fprintf(tabFileHandle, "%s\t", orientation);
fprintf(tabFileHandle, "%s\n", allele);
}

void writeToErrorFile(char *snp_id, char *start, char *end, char *loc_type, char *orientation, char *allele)
{
fprintf(errorFileHandle, "snp_id = %s\n", snp_id);
fprintf(errorFileHandle, "chromStart = %s\n", start);
fprintf(errorFileHandle, "chromEnd = %s\n", end);
fprintf(errorFileHandle, "loc_type = %s\n", loc_type);
fprintf(errorFileHandle, "orientation = %s\n", orientation);
fprintf(errorFileHandle, "allele = %s\n", allele);
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

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");
strcpy(fileName, tableName);
strcat(fileName, ".tab");

tabFileHandle = mustOpen(fileName, "w");
safef(query, sizeof(query), "select snp_id, chromStart, chromEnd, loc_type, orientation, allele from %s ", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {

    /* no possible candidates in exact and between */
    if (sameString(row[3], "2") || sameString(row[3], "3"))
        {
	writeToTabFile(row[0], row[1], row[2], row[3], row[4], row[5]);
	continue;
	}

    /* pass through alleles with no parens */
    if (!needToSplit(row[5]))
	{
	writeToTabFile(row[0], row[1], row[2], row[3], row[4], row[5]);
	continue;
	}

    allele = expandAllele(row[5]);
    /* detect errors but not removing from data set */
    if (sameString(allele, row[5]))
        {
        errorCount++;
	writeToErrorFile(row[0], row[1], row[2], row[3], row[4], row[5]);
        }

    /* calculate end for locType 4, 5, 6 */
    if (sameString(row[3], "4") || sameString(row[3], "5") || sameString(row[3], "6"))
        {
	start = atoi(row[1]);
        end = start + strlen(allele);
	verbose(5, "setting end coord for %s, locType = %s, startAllele = %s\n", row[0], row[3], row[5]);
	sprintf(newEndString, "%d", end);
        writeToTabFile(row[0], row[1], newEndString, row[3], row[4], allele);
	continue;
	}

    /* check for exceptions in locType 1 */
    if (sameString(row[3], "1"))
        {
	start = atoi(row[1]);
	end = atoi(row[2]);
	size = end - start;
	if (size != strlen(allele))
	    {
	    if (size > 256)
	        writeToExceptionFile(chromName, start, end, row[0], "RangeLocTypeWrongSizeLargeAllele");
            else
	        writeToExceptionFile(chromName, start, end, row[0], "RangeLocTypeWrongSize");
	    }
	}

    writeToTabFile(row[0], row[1], row[2], row[3], row[4], allele);

    count++;
    // short-circuit
    // if (count == 1000) 
        // {
        // sqlFreeResult(&sr);
        // hFreeConn(&conn);
        // fclose(f);
        // return;
	// }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
fclose(tabFileHandle);
verbose(2, "%d alleles expanded\n", count);
verbose(2, "%d errors found\n", errorCount);
}


void cleanDatabaseTable(char *chromName)
/* do a 'delete from tableName' */
{
struct sqlConnection *conn = hAllocConn();
char query[256];
strcpy(query, "delete from chr");
strcat(query, chromName);
strcat(query, "_snpTmp");
sqlUpdate(conn, query);
hFreeConn(&conn);
}


void loadDatabase(char *chromName)
/* load one table into database */
{
FILE *f;
struct sqlConnection *conn = hAllocConn();
char tableName[64], fileName[64];

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");

strcpy(fileName, "chr");
strcat(fileName, chromName);
strcat(fileName, "_snpTmp.tab");

f = mustOpen(fileName, "r");
/* hgLoadTabFile closes the file handle */
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, handle allele, rewrite to individual chrom tables */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

chromHash = loadChroms(contigGroup);
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 0;
    }

errorFileHandle = mustOpen("snpExpandAllele.errors", "w");
exceptionFileHandle = mustOpen("snpExpandAllele.exceptions", "w");

// doExpandAllele("22");
// return 0;

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    verbose(1, "chrom = %s\n", chromName);
    doExpandAllele(chromName);
    /* snpExpandAllele doesn't change the table format, so just delete old rows */
    cleanDatabaseTable(chromName);
    loadDatabase(chromName);
    verbose(2, "------------------------------\n");
    }

fclose(errorFileHandle);
fclose(exceptionFileHandle);
return 0;
}

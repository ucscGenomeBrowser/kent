/* snpLocType - third step in dbSNP processing.
 * Read the chrN_snpTmp tables created by snpSplitSimple and handle locType.
 * Rewrite to new chrN_snpTmp tables.  Get chromInfo from ContigInfo. 

 * This could use chromInfo from assembly to check coords, or I could check 
 * ContigInfo earlier. */

#include "common.h"

#include "dystring.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpLocType.c,v 1.19 2006/03/08 22:57:11 heather Exp $";

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

safef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s' and contig_end != 0", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(chromName, sizeof(chromName), "%s", row[0]);
    el = slNameNew(chromName);
    slAddHead(&ret, el);
    }
sqlFreeResult(&sr);

safef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s' and contig_end = 0", contigGroup);
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


int getChromEnd(char *chromName, char *snpId, char *locTypeString, char *chromStartString, char *rangeString, char *allele)
/* calculate chromEnd based on locType */
/* return -1 if any problems */
/* rangeString is not provided for locType 4,5,6 */
/* calculate here, but return 0 (defer calculation) if allele needs to be expanded */
{
int locTypeInt = sqlUnsigned(locTypeString);
int chromStart = sqlUnsigned(chromStartString);
int alleleSize = strlen(allele);
int chromEnd = 0;
char *tmpString;
int size = 0;
boolean randomChrom = FALSE;

tmpString = strstr(chromName, "random");
if (tmpString != NULL) randomChrom = TRUE;

/* range */
if (locTypeInt == 1)
    {
    if (!randomChrom)
        {
        tmpString = strstr(rangeString, "..");
        if (tmpString == NULL) 
            {
	    fprintf(errorFileHandle, "Missing quotes in phys_pos for range\n");
            return (-1);
	    }
        tmpString = tmpString + 2;
        chromEnd = sqlUnsigned(tmpString);
	}
    else
        {
        chromEnd = sqlUnsigned(rangeString);
	chromEnd++;
	}

    if (chromEnd <= chromStart) 
        {
	fprintf(errorFileHandle, "Chrom end <= chrom start for range\n");
        return (-1);
	}
    size = chromEnd - chromStart;

    /* only check size if we don't need to expand (that is next step in processing */
    if (!needToExpand(allele) && alleleSize != size)
        {
        /* distinguish large alleles because rs_fasta should have correct data for them */
        if (size > 256)
            writeToExceptionFile(chromName, chromStart, chromEnd, snpId, "RangeLocTypeWrongSizeLargeAllele");
        else
            writeToExceptionFile(chromName, chromStart, chromEnd, snpId, "RangeLocTypeWrongSize");
        }

    return chromEnd;
    }

/* exact */
if (locTypeInt == 2)
    {
    chromEnd = sqlUnsigned(rangeString);
    if (randomChrom)
        {
	if (chromEnd != chromStart)
	    {
	    fprintf(errorFileHandle, "Wrong size for exact\n");
	    writeToExceptionFile(chromName, chromStart, chromEnd+1, snpId, "ExactLocTypeWrongSize");
	    }
	return chromEnd + 1;
	}

    if (chromEnd != chromStart + 1) 
        {
	fprintf(errorFileHandle, "Wrong size for exact\n");
	writeToExceptionFile(chromName, chromStart, chromEnd, snpId, "ExactLocTypeWrongSize");
        return (-1);
	}
    return chromEnd;
    }

/* between */
/* could check rangeString */
if (locTypeInt == 3)
    {
    return chromStart;
    }

/* rangeInsertion, rangeSubstitution, or rangeDeletion */
if (locTypeInt == 4 || locTypeInt == 5 || locTypeInt == 6)
    {
    if (needToExpand(allele)) return 0;
    chromEnd = chromStart + alleleSize;
    return chromEnd;
    }

fprintf(errorFileHandle, "Unknown locType\n");
return (-1);

}

void doLocType(char *chromName)
/* read the database table, adjust, write .tab file, load */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
FILE *f;
int chromEnd = 0;
int chromStart = 0;
int skipCount = 0;

safef(tableName, ArraySize(tableName), "chr%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "chr%s_snpTmp.tab", chromName);

f = mustOpen(fileName, "w");

safef(query, sizeof(query), 
    "select snp_id, ctg_id, loc_type, phys_pos_from, phys_pos, orientation, allele from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromEnd = getChromEnd(chromName,
                           cloneString(row[0]),
                           cloneString(row[2]), 
                           cloneString(row[3]), 
			   cloneString(row[4]), 
			   cloneString(row[6]));
    if (chromEnd == -1)
        {
	skipCount++;
        fprintf(errorFileHandle, "snp = %s\tlocType = %s\tchrom = %s\tchromStart = %s\trangeString=%s\n", 
	                          row[0], row[2], chromName, row[3], row[4]);
	continue;
	}

    if (sameString(row[2], "1"))
        {
	chromStart = sqlUnsigned(row[3]);
	chromStart++;
	chromEnd++;
        fprintf(f, "%s\t%s\t%d\t%d\t%s\t%s\t%s\n", 
               row[0], row[1], chromStart, chromEnd, row[2], row[5], row[6]);
	continue;
	}
    fprintf(f, "%s\t%s\t%s\t%d\t%s\t%s\t%s\n", 
               row[0], row[1], row[3], chromEnd, row[2], row[5], row[6]);

    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
if (skipCount > 0)
    verbose(1, "skipping %d rows\n", skipCount);
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
"    allele blob\n"
");\n";

struct dyString *dy = newDyString(1024);

safef(tableName, ArraySize(tableName), "chr%s_snpTmp", chromName);
dyStringPrintf(dy, createString, tableName);
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
    doLocType(chromPtr->name);
    recreateDatabaseTable(chromPtr->name);
    loadDatabase(chromPtr->name);
    }

carefulClose(&errorFileHandle);
carefulClose(&exceptionFileHandle);
return 0;
}

/* snpLocType - third step in dbSNP processing.
 * Read the chrN_snpTmp tables created by snpSplitSimple and handle locType.
 * Rewrite to new chrN_snpTmp tables.  Get chromInfo from ContigInfo. 

 * This also uses chromInfo from assembly to check coords? 
 * Or should I check ContigInfo earlier??!! */

#include "common.h"

#include "dystring.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpLocType.c,v 1.1 2006/02/01 09:57:58 heather Exp $";

char *snpDb = NULL;
char *contigGroup = NULL;
static struct hash *chromHash = NULL;
FILE *errorFileHandle = NULL;

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

int getChromEnd(char *locTypeString, char *chromStartString, char *rangeString, char *leftNeighbor, char *rightNeighbor, char *allele)
/* calculate chromEnd based on locType */
/* return -1 if any problems */
{
int locTypeInt = atoi(locTypeString);
int chromStart = atoi(chromStartString);
int alleleSize = strlen(allele);
int chromEnd = 0;
char *tmpString;
int rangeInContig = 0;

/* range */
if (locTypeInt == 1)
    {
    tmpString = strstr(rangeString, "..");
    if (tmpString == NULL) 
        {
	fprintf(errorFileHandle, "Missing quotes in phys_pos for range\n");
        return (-1);
	}
    tmpString = tmpString + 1;
    chromEnd = atoi(tmpString);
    if (chromEnd <= chromStart) 
        {
	fprintf(errorFileHandle, "Chrom end <= chrom start for range\n");
        return (-1);
	}
    return chromEnd;
    }

/* exact */
if (locTypeInt == 2)
    {
    chromEnd = atoi(rangeString);
    if (chromEnd != chromStart + 1) 
        {
	fprintf(errorFileHandle, "Wrong size for exact\n");
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

/* rangeInsertion or rangeDeletion */
if (locTypeInt == 4 || locTypeInt == 6)
    {
    rangeInContig = atoi(rightNeighbor) - atoi(leftNeighbor) - alleleSize;
    chromEnd = chromStart + rangeInContig;
    return chromEnd;
    }

/* rangeSubstitution */
if (locTypeInt == 5)
    {
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
int skipCount = 0;

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");
strcpy(fileName, tableName);
strcat(fileName, ".tab");

f = mustOpen(fileName, "w");

safef(query, sizeof(query), 
    "select snp_id, loc_type, lc_ngbr, rc_ngbr, phys_pos_from, phys_pos, orientation, allele from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromEnd = getChromEnd(cloneString(row[1]), 
                           cloneString(row[4]), 
			   cloneString(row[5]), 
			   cloneString(row[2]), 
			   cloneString(row[3]), 
			   cloneString(row[7]));
    if (chromEnd == -1)
        {
	skipCount++;
        fprintf(errorFileHandle, "%s\t", tableName);
        fprintf(errorFileHandle, "%s\t", row[0]);
        fprintf(errorFileHandle, "%s\t", row[1]);
        fprintf(errorFileHandle, "%s\t", row[2]);
        fprintf(errorFileHandle, "%s\t", row[3]);
        fprintf(errorFileHandle, "%s\t", row[4]);
        fprintf(errorFileHandle, "%s\t", row[5]);
        fprintf(errorFileHandle, "%s\t", row[6]);
        fprintf(errorFileHandle, "%s\n", row[7]);
	continue;
	}

    fprintf(f, "%s\t", row[0]);
    fprintf(f, "%s\t", row[4]);
    fprintf(f, "%d\t", chromEnd);
    fprintf(f, "%s\t", row[1]);
    fprintf(f, "%s\t", row[6]);
    fprintf(f, "%s\n", row[7]);

    }
sqlFreeResult(&sr);
hFreeConn(&conn);
fclose(f);
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
"    chromStart int(11) not null,\n"
"    chromEnd int(11) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    orientation tinyint(4) not null,\n"
"    allele blob\n"
");\n";

struct dyString *dy = newDyString(1024);

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");

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

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");

strcpy(fileName, "chr");
strcat(fileName, chromName);
strcat(fileName, "_snpTmp.tab");

f = mustOpen(fileName, "r");
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, handle locType, rewrite to individual chrom tables */
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

errorFileHandle = mustOpen("snpLocType.errors", "w");

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    verbose(1, "chrom = %s\n", chromName);
    doLocType(chromName);
    recreateDatabaseTable(chromName);
    loadDatabase(chromName);
    }

fclose(errorFileHandle);
return 0;
}

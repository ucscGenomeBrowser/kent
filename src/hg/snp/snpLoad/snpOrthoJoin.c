/* snpOrthoJoin - given a snpOrtho table, combine species. */
/* I couldn't see how to do this with UNIX join. */
/* Another option is to do it within SQL. */
/* assume panTro2/rheMac2, or discover it? */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


static char *database = NULL;
static char *snpSimpleTable = NULL;
static char *orthoTable = NULL;
struct hash *chimpHash = NULL;
struct hash *macaqueHash = NULL;
FILE *outputFileHandle = NULL;
FILE *missingFileHandle = NULL;

struct orthoSnp 
    {
    char *chrom;
    int start;
    int end;
    char *strand;
    char *allele;
    };


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpOrthoJoin - read snp ortho table and combine species\n"
    "usage:\n"
    "    snpOrthoStats database snpSimpleTable orthoTable\n");
}


struct hash *getOrtho(char *tableName, char *speciesName)
/* put all orthoSnp objects in ret hash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct orthoSnp *orthoSnpInstance = NULL;
struct hash *ret = newHash(16);
int count = 0;

sqlSafef(query, sizeof(query), 
    "select name, species, orthoChrom, orthoChromStart, orthoChromEnd, orthoStrand, orthoAllele from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!sameString(row[1], speciesName)) continue;
    AllocVar(orthoSnpInstance);
    orthoSnpInstance->chrom = cloneString(row[2]);
    orthoSnpInstance->start = sqlUnsigned(row[3]);
    orthoSnpInstance->end = sqlUnsigned(row[4]);
    orthoSnpInstance->strand = cloneString(row[5]);
    orthoSnpInstance->allele = cloneString(row[6]);
    hashAdd(ret, cloneString(row[0]), orthoSnpInstance);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "%d rows in hash for %s\n", count, speciesName);
return ret;
}

void writeResults(char *tableName)
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

struct hashEl *helChimp = NULL, *helMacaque = NULL;
struct orthoSnp *chimpData, *macaqueData, *missingData = NULL;

char *snpId;
char *humanObserved;
char *humanAllele;
char *humanStrand;

int humanCount = 0;
int chimpOnlyCount = 0;
int macaqueOnlyCount = 0;
int missingCount = 0;
int bothCount = 0;
int chromStart = 0;
int chromEnd = 0;
int binVal = 0;

AllocVar(missingData);
missingData->chrom = cloneString("?");
missingData->start = 0;
missingData->end = 0;
missingData->strand = cloneString("?");
missingData->allele = cloneString("?");

sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, strand, refUCSC, observed from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromStart = sqlUnsigned(row[1]);
    chromEnd = sqlUnsigned(row[2]);
    snpId = cloneString(row[3]);
    humanStrand = cloneString(row[4]);
    humanAllele = cloneString(row[5]);
    humanObserved = cloneString(row[6]);
    humanCount++;

    helChimp = hashLookup(chimpHash, snpId);
    if (helChimp != NULL)
        chimpData = (struct orthoSnp *)helChimp->val;
    else
        chimpData = missingData;

    helMacaque = hashLookup(macaqueHash, snpId);
    if (helMacaque != NULL)
        macaqueData = (struct orthoSnp *)helMacaque->val;
    else
        macaqueData = missingData;

    if (!helChimp && !helMacaque)
        {
	fprintf(missingFileHandle, "%s not found\n", snpId);
	missingCount++;
	continue;
	}

    if (helChimp && helMacaque)
	bothCount++;

    if (!helChimp && helMacaque)
	macaqueOnlyCount++;

    if (helChimp && !helMacaque)
	chimpOnlyCount++;

    binVal = hFindBin(chromStart, chromEnd);
    fprintf(outputFileHandle, "%d\t%s\t%s\t%s\t", binVal, row[0], row[1], row[2]);
    fprintf(outputFileHandle, "%s\t%s\t%s\t%s\t", snpId, humanObserved, humanAllele, humanStrand);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", chimpData->chrom, chimpData->start, chimpData->end);
    fprintf(outputFileHandle, "%s\t%s\t", chimpData->allele, chimpData->strand);
    fprintf(outputFileHandle, "%s\t%d\t%d\t", macaqueData->chrom, macaqueData->start, macaqueData->end);
    fprintf(outputFileHandle, "%s\t%s\n", macaqueData->allele, macaqueData->strand);
    }

verbose(1, "humanCount = %d\n", humanCount);
verbose(1, "chimpOnlyCount = %d\n", chimpOnlyCount);
verbose(1, "macaqueOnlyCount = %d\n", macaqueOnlyCount);
verbose(1, "missingCount = %d\n", missingCount);
verbose(1, "bothCount = %d\n", bothCount);
}


int main(int argc, char *argv[])
{
if (argc != 4)
    usage();

database = argv[1];
hSetDb(database);
snpSimpleTable = argv[2];
if (!hTableExistsDb(database, snpSimpleTable))
    errAbort("can't get table %s\n", snpSimpleTable);
orthoTable = argv[3];
if (!hTableExistsDb(database, orthoTable))
    errAbort("can't get table %s\n", orthoTable);

outputFileHandle = mustOpen("snpOrthoJoin.tab", "w");
missingFileHandle = mustOpen("snpOrthoJoin.missing", "w");

chimpHash = getOrtho(orthoTable, "panTro2");
macaqueHash = getOrtho(orthoTable, "rheMac2");
writeResults(snpSimpleTable);

carefulClose(&outputFileHandle);
carefulClose(&missingFileHandle);

return 0;
}

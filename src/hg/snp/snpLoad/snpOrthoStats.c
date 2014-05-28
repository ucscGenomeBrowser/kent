/* snpOrthoStats - report on ortho mappings and alleles. */
/* assume panTro2/rheMac2, or discover it? */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hash.h"
#include "hdb.h"


static char *snpDb = NULL;
static char *snpTable = NULL;
static char *orthoTable = NULL;
struct hash *chimpHash = NULL;
struct hash *macaqueHash = NULL;
FILE *outputFileHandle = NULL;
FILE *missingFileHandle = NULL;
FILE *chimpOnlyFileHandle = NULL;
FILE *macaqueOnlyFileHandle = NULL;

struct coords 
    {
    char *chrom;
    int start;
    int end;
    char *allele;
    };


void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpOrthoStats - read snp ortho table and report stats\n"
    "usage:\n"
    "    snpOrthoStats database snpTable orthoTable\n");
}


struct hash *getOrtho(char *tableName, char *speciesName)
/* put all coords in coordHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct coords *cel = NULL;
struct hash *ret = newHash(16);
int count = 0;

sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, species, strand, allele from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!sameString(row[4], speciesName)) continue;
    /* store all coords */
    AllocVar(cel);
    cel->chrom = cloneString(row[0]);
    cel->start = sqlUnsigned(row[1]);
    cel->end = sqlUnsigned(row[2]);
    cel->allele = cloneString(row[6]);
    // do I need strand, row[5], to interpret allele??!!
    hashAdd(ret, cloneString(row[3]), cel);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "%d rows in hash for %s\n", count, speciesName);
return ret;
}

void compareAndLog(char *snpId, char *humanAllele, char *chimpAllele, char *macaqueAllele)
{
if (macaqueAllele == NULL)
    {
    if (sameString(humanAllele, chimpAllele))
        fprintf(chimpOnlyFileHandle, "%s: human = chimp; macaque missing\n", snpId);
    else
        fprintf(chimpOnlyFileHandle, "%s: human != chimp; macaque missing\n", snpId);
    return;
    }
if (chimpAllele == NULL)
    {
    if (sameString(humanAllele, macaqueAllele))
        fprintf(macaqueOnlyFileHandle, "%s: human = macaque; chimp missing\n", snpId);
    else
        fprintf(macaqueOnlyFileHandle, "%s: human != macaque; chimp missing\n", snpId);
    return;
    }
if (sameString(humanAllele, chimpAllele) && sameString(humanAllele, macaqueAllele))
    {
    fprintf(outputFileHandle, "%s: all equal\n", snpId);
    return;
    }
if (sameString(humanAllele, chimpAllele))
    {
    fprintf(outputFileHandle, "%s: human == chimp; macaque different\n", snpId);
    return;
    }
if (sameString(humanAllele, macaqueAllele))
    {
    fprintf(outputFileHandle, "%s: human == macaque; chimp different\n", snpId);
    return;
    }
if (sameString(chimpAllele, macaqueAllele))
    {
    fprintf(outputFileHandle, "%s: chimp == macaque; human different\n", snpId);
    return;
    }
fprintf(outputFileHandle, "%s: all different\n", snpId);
}


void writeResults(char *tableName)
{
struct hashEl *hel= NULL;
struct coords *cel = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *helChimp, *helMacaque = NULL;
struct coords *coordsChimp, *coordsMacaque = NULL;
char *snpId;
char *humanAllele;
char *chimpAllele = NULL;
char *macaqueAllele = NULL;
int humanCount = 0;
int chimpOnlyCount = 0;
int macaqueOnlyCount = 0;
int bothCount = 0;

sqlSafef(query, sizeof(query), "select name, refUCSC from %s", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpId = cloneString(row[0]);
    humanAllele = cloneString(row[1]);
    humanCount++;

    helChimp = hashLookup(chimpHash, snpId);
    if (helChimp != NULL)
        {
        coordsChimp = (struct coords *)helChimp->val;
	chimpAllele = coordsChimp->allele;
	}

    helMacaque = hashLookup(macaqueHash, snpId);
    if (helMacaque != NULL)
        {
        coordsMacaque = (struct coords *)helMacaque->val;
	macaqueAllele = coordsMacaque->allele;
	}

    if (!helChimp && !helMacaque)
        {
	fprintf(missingFileHandle, "%s not found\n", snpId);
	continue;
	}

    if (helChimp != NULL && helMacaque != NULL)
        {
	verbose(5, "%s found in both chimp and macaque\n", snpId);
	bothCount++;
	compareAndLog(snpId, humanAllele, chimpAllele, macaqueAllele);
	continue;
	}

    if (helChimp != NULL && helMacaque == NULL)
        {
	verbose(5, "%s found in chimp only\n", snpId);
	chimpOnlyCount++;
	compareAndLog(snpId, humanAllele, chimpAllele, NULL);
	}
    else 
	{
	verbose(5, "%s found in macaque only\n", snpId);
	macaqueOnlyCount++;
	compareAndLog(snpId, humanAllele, NULL, macaqueAllele);
	}
    }

verbose(1, "chimpOnlyCount = %d\n", chimpOnlyCount);
verbose(1, "macaqueOnlyCount = %d\n", macaqueOnlyCount);
verbose(1, "bothCount = %d\n", bothCount);
}


int main(int argc, char *argv[])
{
if (argc != 4)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
snpTable = argv[2];
if (!hTableExistsDb(snpDb, snpTable))
    errAbort("can't get table %s\n", snpTable);
orthoTable = argv[3];
if (!hTableExistsDb(snpDb, orthoTable))
    errAbort("can't get table %s\n", orthoTable);

outputFileHandle = mustOpen("snpOrthoStats.out", "w");
missingFileHandle = mustOpen("snpOrthoStats.humanOnly", "w");
chimpOnlyFileHandle = mustOpen("snpOrthoStats.chimpOnly", "w");
macaqueOnlyFileHandle = mustOpen("snpOrthoStats.macaqueOnly", "w");

chimpHash = getOrtho(orthoTable, "panTro2");
macaqueHash = getOrtho(orthoTable, "rheMac2");
writeResults(snpTable);

carefulClose(&outputFileHandle);
carefulClose(&missingFileHandle);
carefulClose(&chimpOnlyFileHandle);
carefulClose(&macaqueOnlyFileHandle);

return 0;
}

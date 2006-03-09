/* snpSNP - tenth step in dbSNP processing.
 * Read the chrN_snpTmp tables into memory.
 * Do lookups into SNP for validation status and heterozygosity.
 * Report if missing.
 * Use UCSC chromInfo. */

#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpSNP.c,v 1.5 2006/03/09 03:17:44 heather Exp $";

struct snpData
    {
    int validation_status;
    float avHet;
    float avHetSE;
    };

static char *snpDb = NULL;
FILE *errorFileHandle = NULL;
static struct hash *snpDataHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpSNP - lookup validation status and heterozygosity in SNP\n"
    "recreate chrN_snpTmp\n"
    "usage:\n"
    "    snpSNP snpDb \n");
}


void readSNP()
/* get validation status and heterozygosity from SNP table */
/* store in global snpDataHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int validInt = 0;

/* could increase the default size */
snpDataHash = newHash(0);
safef(query, sizeof(query), "select snp_id, validation_status, avg_heterozygosity, het_se from SNP");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    validInt = sqlUnsigned(row[1]);
    if (validInt > 31)
        {
        fprintf(errorFileHandle, 
	        "unexpected validation_status %d for snp_id %s\n", validInt, row[0]);
	validInt = 0;
	}

    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

void readSnps(char *chromName)
/* read through chrN_snpTmp tables */
/* lookup into snpDataHash */
/* write to output file */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
FILE *f;
struct snpData *sel = NULL;

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);
f = mustOpen(fileName, "w");

safef(query, sizeof(query), 
     "select snp_id, chromStart, chromEnd, loc_type, class, orientation, molType, fxn_class, "
     "allele, refUCSC, refUCSCReverseComp, observed from %s ", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    sel = hashFindVal(snpDataHash, row[0]);
    if (sel == NULL)
        {
        fprintf(f, "%s\t%s\t%s\t%s\t", row[0], row[1], row[2], row[3]);
	fprintf(f, "%s\t%s\t%s\t%s\t", row[4], row[5], row[6], row[7]);
	fprintf(f, "0\t0.0\t0.0\t");
	fprintf(f, "%s\t%s\t%s\t%s\n", row[8], row[9], row[10], row[11]);
        fprintf(errorFileHandle, "no match for snp_id %s\n", row[0]);
	continue;
	}
    fprintf(f, "%s\t%s\t%s\t%s\t", row[0], row[1], row[2], row[3]);
    fprintf(f, "%s\t%s\t%s\t%s\t", row[4], row[5], row[6], row[7]);
    fprintf(f, "%d\t%f\t%f\t", sel->validation_status, sel->avHet, sel->avHetSE);
    fprintf(f, "%s\t%s\t%s\t%s\n", row[8], row[9], row[10], row[11]);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}

void recreateDatabaseTable(char *chromName)
/* create a new chrN_snpTmp table with new definition */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    snp_id int(11) not null,\n"
"    chromStart int(11) not null,\n"
"    chromEnd int(11) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    class varchar(255) not null,\n"
"    orientation tinyint(4) not null,\n"
"    molType varchar(255) not null,\n"
"    fxn_class varchar(255) not null,\n"
"    validation_status tinyint(4) not null,\n"
"    avHet float not null, \n"
"    avHetSE float not null, \n"
"    allele blob,\n"
"    refUCSC blob,\n"
"    refUCSCReverseComp blob,\n"
"    observed blob\n"
");\n";


struct dyString *dy = newDyString(1024);

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
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

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);

f = mustOpen(fileName, "r");
/* hgLoadTabFile closes the file handle */
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, lookup in snpFasta, rewrite to individual chrom tables */
{
struct slName *chromList, *chromPtr;
char tableName[64];

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
chromList = hAllChromNamesDb(snpDb);
if (chromList == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

errorFileHandle = mustOpen("snpSNP.errors", "w");

/* read into global hash */
verbose(1, "reading SNP table...\n");
readSNP();
    
for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    if (!hTableExists(tableName)) continue;
 
    verbose(1, "chrom = %s\n", chromPtr->name);

    readSnps(chromPtr->name);
    }

carefulClose(&errorFileHandle);

// for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    // {
    // safef(tableName, ArraySize(tableName), "%s_snpTmp", chromPtr->name);
    // if (!hTableExists(tableName)) continue;
    // recreateDatabaseTable(chromPtr->name);
    // verbose(1, "loading chrom = %s\n", chromPtr->name);
    // loadDatabase(chromPtr->name);
    // }

return 0;
}

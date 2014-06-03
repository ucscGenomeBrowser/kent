/* snpSplitByChrom - second step in dbSNP processing.
 * Split the ContigLocFilter table by distinct chrom from ContigInfo. 
 * ContigLocFilter contains chrom (added by snpContigLocFilter, the first step) */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

#include "dystring.h"
#include "hash.h"
#include "hdb.h"


static char *snpDb = NULL;
static char *contigGroup = NULL;
static struct hash *chromHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpSplitByChrom - split the ContigLocFilter table by chrom\n"
    "usage:\n"
    "    snpSplitByChrom snpDb contigGroup\n");
}


struct hash *loadChroms(char *contigGroup)
/* hash all chromNames that match contigGroup and open file for each */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char chromName[64];
FILE *f;
char fileName[64];

ret = newHash(0);
verbose(1, "getting chroms...\n");
sqlSafef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s' and contig_end != 0", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(chromName, sizeof(chromName), "%s", row[0]);
    safef(fileName, ArraySize(fileName), "chr%s_snpTmp.tab", chromName);
    f = mustOpen(fileName, "w");
    hashAdd(ret, chromName, f);
    }
sqlFreeResult(&sr);

sqlSafef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s' and contig_end = 0", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(chromName, sizeof(chromName), "%s_random", row[0]);
    safef(fileName, ArraySize(fileName), "chr%s_snpTmp.tab", chromName);
    f = mustOpen(fileName, "w");
    hashAdd(ret, chromName, f);
    }
sqlFreeResult(&sr);

hFreeConn(&conn);
return ret;
}

void writeSplitTables()
/* sequentially read ContigLocFilter, writing to appropriate chrom file */
/* we are storing chromName in the table just as a sanity check */
/* It is dropped in the next step of the pipeline */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel;

verbose(1, "reading ContigLocFilter...\n");

sqlSafef(query, sizeof(query), 
    "select snp_id, ctg_id, chromName, loc_type, start, end, orientation, allele, weight from ContigLocFilter");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    // better to use FILE *f = hashMustFindVal(chromHash,row[2]);
    hel = hashLookup(chromHash,row[2]);
    if (hel == NULL)
        {
	verbose(1, "%s not found\n", row[2]);
	continue;
	}
    fprintf(hel->val, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8]);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void createTable(char *chromName)
/* create a chrN_snpTmp table */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    snp_id int(11) not null,\n"
"    ctg_id int(11) not null,\n"
"    chromName char(32) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    start int(11) not null,\n"
"    end int(11),\n"
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
/* read ContigLocFilter, writing to individual chrom tables */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLocFilter"))
    errAbort("no ContigLocFilter table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "ContigInfo"))
    errAbort("no ContigInfo table in %s\n", snpDb);

chromHash = loadChroms(contigGroup);
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

writeSplitTables();

verbose(1, "closing files...\n");
cookie = hashFirst(chromHash);
while ((hel = hashNext(&cookie)))
    fclose(hel->val);

verbose(1, "creating tables...\n");
cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    createTable(chromName);

verbose(1, "loading database...\n");
cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    verbose(1, "chrom = %s\n", chromName);
    loadDatabase(chromName);
    }

return 0;
}

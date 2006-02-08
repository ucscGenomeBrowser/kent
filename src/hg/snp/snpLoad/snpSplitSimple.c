/* snpSplitSimple - second step in dbSNP processing.
 * Split the ContigLocFilter table by distinct chrom from ContigInfo. 
 * ContigLocFilter contains chrom (added by snpContigLocFilter, the first step) */
#include "common.h"

#include "dystring.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpSplitSimple.c,v 1.4 2006/02/08 21:21:52 heather Exp $";

static char *snpDb = NULL;
static char *contigGroup = NULL;
static struct hash *chromHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpSplitSimple - split the ContigLocFilter table by chrom\n"
    "usage:\n"
    "    snpSplitSimple snpDb contigGroup\n");
}


struct hash *loadChroms(char *contigGroup)
/* hash all chromNames that match contigGroup and open file for each */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *chromName;
FILE *f;
char fileName[32];

ret = newHash(0);
verbose(1, "getting chroms...\n");
safef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s'", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromName = cloneString(row[0]);
    strcpy(fileName, "chr");
    strcat(fileName, chromName);
    strcat(fileName, "_snpTmp");
    strcat(fileName, ".tab");
    f = mustOpen(fileName, "w");
    hashAdd(ret, chromName, f);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

void writeSplitTables()
/* sequentially read ContigLocFilter, writing to appropriate chrom file */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel;
char *chromName;

verbose(1, "reading ContigLocFilter...\n");

safef(query, sizeof(query), 
    "select snp_id, loc_type, lc_ngbr, rc_ngbr, phys_pos_from, phys_pos, orientation, allele, chromName "
    "from ContigLocFilter");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(chromHash,row[8]);
    if (hel == NULL)
        {
	verbose(1, "%s not found\n", row[8]);
	continue;
	}
    fprintf(hel->val, "%s\t", row[0]);
    fprintf(hel->val, "%s\t", row[8]);
    fprintf(hel->val, "%s\t", row[1]);
    fprintf(hel->val, "%s\t", row[2]);
    fprintf(hel->val, "%s\t", row[3]);
    fprintf(hel->val, "%s\t", row[4]);
    fprintf(hel->val, "%s\t", row[5]);
    fprintf(hel->val, "%s\t", row[6]);
    fprintf(hel->val, "%s\n", row[7]);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}


void createTable(char *chromName)
/* create a chrN_snpTmp table */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    snp_id int(11) not null,\n"
"    chromName char(32) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    lc_ngbr int(11) not null,\n"
"    rc_ngbr int(11) not null,\n"
"    phys_pos_from int(11) not null,\n"
"    phys_pos varchar(32),\n"
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
while (hel = hashNext(&cookie))
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

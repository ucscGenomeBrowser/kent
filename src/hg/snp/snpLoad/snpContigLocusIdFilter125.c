/* snpContigLocusIdFilter125
 * Filter the ContigLocusId table to remove contigs that aren't the reference assembly.
   Save only the fxn_class column. 
   Create the ContigLocusIdFilter table. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

#include "hash.h"
#include "hdb.h"


static char *snpDb = NULL;
static struct hash *contigHash = NULL;
static char *contigGroup = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpContigLocusIdFilter125 - filter the ContigLocusId table using contig_acc\n"
    "usage:\n"
    "    snpContigLocusIdFilter125 snpDb contigGroup\n");
}


struct hash *loadContigs(char *contigGroup)
/* hash all ctg IDs that match contigGroup */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;

ret = newHash(0);
verbose(1, "getting contigs...\n");
sqlSafef(query, sizeof(query), "select contig_acc, group_term from ContigInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (!sameString(row[1], contigGroup)) continue;
    hashAdd(ret, cloneString(row[0]), NULL);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "contigs found = %d\n", count);
return ret;
}


void filterSNPs()
/* read all rows, relevant columns of ContigLocusId into memory. */
/* Write out rows where ctg_id is in our hash. */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *el1;
FILE *f;

f = hgCreateTabFile(".", "ContigLocusIdFilter");

sqlSafef(query, sizeof(query), "select snp_id, contig_acc, fxn_class, mrna_acc, protein_acc from ContigLocusId");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el1 = hashLookup(contigHash,row[1]);
    if (el1 != NULL)
        {
	fprintf(f, "%s\t%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3], row[4]);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}


void createTable()
/* create a ContigLocusIdFilter table */
{
struct sqlConnection *conn = hAllocConn();
char *createString =
NOSQLINJ "CREATE TABLE ContigLocusIdFilter (\n"
"    snp_id int(11) not null,\n"
"    contig_acc varchar(32) not null,\n"
"    fxn_class tinyint(4) not null,\n"
"    mrna_acc varchar(15),\n"
"    protein_acc varchar(15)\n"
");\n";

sqlRemakeTable(conn, "ContigLocusIdFilter", createString);
}


void loadDatabase()
{
struct sqlConnection *conn = hAllocConn();
FILE *f = mustOpen("ContigLocusIdFilter.tab", "r");
hgLoadTabFile(conn, ".", "ContigLocusIdFilter", &f);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* Read ContigInfo into hash. */
/* Filter ContigLocusId and write to ContigLocusIdFilter. */
{

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLocusId"))
    errAbort("no ContigLocusId table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "ContigInfo"))
    errAbort("no ContigInfo table in %s\n", snpDb);


contigHash = loadContigs(contigGroup);
if (contigHash == NULL) 
    {
    verbose(1, "couldn't get ContigInfo hash\n");
    return 1;
    }

filterSNPs();
createTable();
loadDatabase();

return 0;
}

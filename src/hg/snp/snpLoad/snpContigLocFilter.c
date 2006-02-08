/* snpContigLocFilter - first step in dbSNP processing.
 * Filter the ContigLoc table in 2 ways: remove contigs that aren't the reference assembly,
   and remove SNPs that have weight = 10 */
#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpContigLocFilter.c,v 1.8 2006/02/08 20:40:58 heather Exp $";

static char *snpDb = NULL;
static struct hash *contigHash = NULL;
static struct hash *weightHash = NULL;
static char *contigGroup = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpContigLocFilter - filter the ContigLoc table\n"
    "usage:\n"
    "    snpContigLocFilter snpDb contigGroup\n");
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
int orient = 0;

ret = newHash(0);
verbose(1, "getting contigs...\n");
safef(query, sizeof(query), "select ctg_id, contig_chr, orient from ContigInfo where group_term = '%s'", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    orient = atoi(row[2]);
    if (orient != 0)
        verbose(1, "Contig %s has non-zero orientation!!\n", row[0]);
    hashAdd(ret, cloneString(row[0]), cloneString(row[1]));
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "contigs found = %d\n", count);
return ret;
}

struct hash *loadMapInfo()
/* hash all snp IDs that have weight = 10 */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;

ret = newHash(0);
verbose(1, "getting weight = 10 from SNPMapInfo...\n");
safef(query, sizeof(query), "select snp_id from SNPMapInfo where weight = 10");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(ret, row[0], NULL);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "SNPs with weight 10 found = %d\n", count);
return ret;
}

void filterContigs()
/* read all rows, relevant columns of ContigLoc into memory. */
/* Write out rows where ctg_id is in our hash. */
/* Don't write rows where weight = 10 from MapInfo. */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *el1, *el2;
FILE *f;
char *chromName;

f = hgCreateTabFile(".", "ContigLocFilter");

safef(query, sizeof(query), 
    "select snp_id, ctg_id, loc_type, lc_ngbr, rc_ngbr, phys_pos_from, phys_pos, orientation, allele from ContigLoc");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el1 = hashLookup(contigHash,row[1]);
    if (el1 != NULL)
        {
	if (sameString(row[5], "0")) continue;
	el2 = hashLookup(weightHash,row[0]);
	if (el2 != NULL) continue;
	fprintf(f, "%s\t", row[0]);
	fprintf(f, "%s\t", row[1]);
	fprintf(f, "%s\t", row[2]);
	fprintf(f, "%s\t", row[3]);
	fprintf(f, "%s\t", row[4]);
	fprintf(f, "%s\t", row[5]);
	fprintf(f, "%s\t", row[6]);
	fprintf(f, "%s\t", row[7]);
	fprintf(f, "%s\t", row[8]);
	chromName = hashFindVal(contigHash,row[1]);
	fprintf(f, "%s\n", chromName);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
fclose(f);
}


void createTable()
/* create a ContigLocFilter table */
{
struct sqlConnection *conn = hAllocConn();
char *createString =
"CREATE TABLE ContigLocFilter (\n"
"    snp_id int(11) not null,       \n"
"    ctg_id int(11) not null,       \n"
"    loc_type tinyint(4) not null,       \n"
"    lc_ngbr int(11) not null,       \n"
"    rc_ngbr int(11) not null,       \n"
"    phys_pos_from int(11) not null,       \n"
"    phys_pos varchar(32), \n"
"    orientation tinyint(4) not null, \n"
"    allele blob,\n"
"    chromName varchar(32)\n"
");\n";

sqlRemakeTable(conn, "ContigLocFilter", createString);
}


void loadDatabase()
{
struct sqlConnection *conn = hAllocConn();
FILE *f = mustOpen("ContigLocFilter.tab", "r");
hgLoadTabFile(conn, ".", "ContigLocFilter", &f);
hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* Read ContigInfo into hash. */
/* Filter ContigLoc and write to ContigLocFilter. */
{

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLoc"))
    errAbort("no ContigLoc table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "ContigInfo"))
    errAbort("no ContigInfo table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "SNPMapInfo"))
    verbose(1, "no SNPMapInfo table in %s\n", snpDb);


contigHash = loadContigs(contigGroup);
if (contigHash == NULL) 
    {
    verbose(1, "couldn't get ContigInfo hash\n");
    return 0;
    }

weightHash = loadMapInfo();
if (weightHash == NULL)
    {
    verbose(1, "couldn't get MapInfo weight hash\n");
    return 0;
    }

filterContigs();
createTable();
loadDatabase();

return 0;
}

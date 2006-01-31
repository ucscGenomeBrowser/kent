/* snpContigLocFilter - first step in dbSNP processing.
 * Filter the ContigLoc table in 2 ways: remove contigs that aren't the reference assembly,
   and remove SNPs that have weight = 10 */
#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpContigLocFilter.c,v 1.1 2006/01/31 18:35:42 heather Exp $";

char *snpDb = NULL;
static struct hash *contigHash = NULL;
char *contigGroup = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpContigLocFilter - filter the ContigLoc table\n"
    "usage:\n"
    "    ContigLocFilter snpDb contigGroup\n");
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
safef(query, sizeof(query), "select ctg_id from ContigInfo where group_term = '%s'", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(ret, row[0], NULL);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "contigs found = %d\n", count);
return ret;
}


void filterContigs()
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *el;
FILE *f;

f = hgCreateTabFile(".", "ContigLocFilter");

safef(query, sizeof(query), 
    "select snp_id, ctg_id, loc_type, lc_ngbr, rc_ngbr, phys_pos_from, phys_pos, orientation, allele from ContigLoc");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = hashLookup(contigHash,row[1]);
    if (el != NULL)
        {
	fprintf(f, "%s\t", row[0]);
	fprintf(f, "%s\t", row[1]);
	fprintf(f, "%s\t", row[2]);
	fprintf(f, "%s\t", row[3]);
	fprintf(f, "%s\t", row[4]);
	fprintf(f, "%s\t", row[5]);
	fprintf(f, "%s\t", row[6]);
	fprintf(f, "%s\t", row[7]);
	fprintf(f, "%s\n", row[8]);
	}
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
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
"    allele blob\n"
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

contigHash = loadContigs(contigGroup);
if (contigHash == NULL) 
    {
    verbose(1, "couldn't get hash\n");
    return 0;
    }

filterContigs();
createTable();
loadDatabase();

return 0;
}

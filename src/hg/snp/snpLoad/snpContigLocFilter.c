/* snpContigLocFilter - first step in dbSNP processing.
 * Filter the ContigLoc table in 2 ways: remove contigs that aren't in the reference assembly,
   and remove SNPs that have weight = 10 */
/* Note: all PAR SNPs have weight = 3. */
/* Translate to N_random, using ctgPos table (must exist). */ 
/* Create ContigLocFilter table. */
#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpContigLocFilter.c,v 1.30 2006/06/16 18:12:57 heather Exp $";

static char *snpDb = NULL;
static char *contigGroup = NULL;

static struct hash *ctgPosHash = NULL;
static struct hash *contigCoords = NULL;
static struct hash *weightHash = NULL;

struct coords
    {
    char *chrom;
    int start;
    int end;
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpContigLocFilter - filter the ContigLoc table\n"
    "usage:\n"
    "    snpContigLocFilter snpDb contigGroup\n");
}

void loadCtgPos()
/* store the coords for each contig_acc */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct coords *cel = NULL;

ctgPosHash = newHash(0);
verbose(1, "reading ctgPos...\n");
safef(query, sizeof(query), "select contig, chrom, chromStart, chromEnd from ctgPos");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(cel);
    cel->chrom = cloneString(row[1]);
    cel->start = sqlUnsigned(row[2]);
    cel->end = sqlUnsigned(row[3]);
    hashAdd(ctgPosHash, cloneString(row[0]), cel);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}


void loadContigCoords(char *contigGroup)
/* store the coords for each ctg_id */
/* lookup in ctgPosHash via contigAccHash */
/* if contig_end == 0 append _random to chromName */
/* Also check that orientation is always positive! */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;
int orient = 0;
int end = 0;
char chromName[64];
struct hashEl *hel = NULL;
struct coords *celOld, *celNew = NULL;

contigCoords = newHash(0);

verbose(1, "reading ContigInfo...\n");
safef(query, sizeof(query), 
      "select ctg_id, contig_acc, contig_chr, contig_end, orient from ContigInfo where group_term = '%s'", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    orient = sqlUnsigned(row[4]);
    if (orient != 0)
        errAbort("Contig %s has non-zero orientation!!\n", row[0]);

    /* detect randoms */
    end = sqlUnsigned(row[3]);
    if (end == 0) 
	safef(chromName, ArraySize(chromName), "%s_random", row[2]);
    else
	safef(chromName, ArraySize(chromName), "%s", row[2]);

    /* use the contig_acc to get the coords */
    /* don't need to save contig_acc */
    AllocVar(celNew);
    celOld = hashFindVal(ctgPosHash, row[1]);
    if (celOld == NULL)
        errAbort("Can't find %s in ctgPos\n", row[1]);

    celNew->chrom = cloneString(chromName);
    celNew->start = celOld->start;
    celNew->end =  celOld->end;
    hashAdd(contigCoords, cloneString(row[0]), celNew);
    verbose(1, "%s chr%s:%d-%d\n", row[0], chromName, celNew->start, celNew->end);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "contigs found = %d\n", count);
verbose(1, "-------------\n");
}

void getWeight()
/* hash all snp IDs into global weightHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int weight = 0;
int count1 = 0;
int count2 = 0;
int count3 = 0;
int count10 = 0;
struct hashEl *hel = NULL;
    
weightHash = newHash(16);

verbose(1, "hashing MapInfo...\n");
safef(query, sizeof(query), "select snp_id, weight from MapInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hel = hashLookup(weightHash, row[0]);
    // storing weight as a string
    if (hel == NULL)
        hashAdd(weightHash, cloneString(row[0]), cloneString(row[1]));
    else
        verbose(1, "duplicate entry for %s\n", row[0]);
    weight = sqlUnsigned(row[1]);
    switch (weight)
        {
        case 1:
            count1++;
	    break;
       case 2:
          count2++;
	  break;
       case 3:
          count3++;
	  break;
       case 10:
          count10++;
	  break;
       }
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "count of snps with weight 1 = %d\n", count1);
verbose(1, "count of snps with weight 2 = %d\n", count2);
verbose(1, "count of snps with weight 3 = %d\n", count3);
verbose(1, "count of snps with weight 10 = %d\n", count10);
}


void filterSnps()
/* Do a serial read through ContigLoc. */
/* For each SNP, get loc_type (1-6), orientation (strand) and allele (refNCBI). */
/* Output to ContigLocFilter table (tableName hard-coded). */
/* Use ctg_id to filter: only save rows where ctg_id is in our hash. */
/* Also, skip whenever weight = 10 from MapInfo weightHash. */

/* phys_pos_from (start) is always an integer. */
/* If phys_pos_from is 0, this is a random contig and we generate lifted coords. */

{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *el1, *el2;
struct hashEl *helRandom;
FILE *f;
struct coords *cel = NULL;
int start = 0;
int end = 0;
int loc_type = 0;
int phys_pos_from = 0;

f = hgCreateTabFile(".", "ContigLocFilter");

safef(query, sizeof(query), 
    "select snp_id, ctg_id, loc_type, phys_pos_from, asn_from, asn_to, orientation, allele from ContigLoc");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el1 = hashLookup(contigCoords, row[1]);
    if (el1 != NULL)
        {
	el2 = hashLookup(weightHash, row[0]);
	if (sameString(el2->val, "10")) continue;

	cel = hashFindVal(contigCoords, row[1]);

	loc_type = sqlUnsigned(row[2]);

        start = sqlUnsigned(row[4]) + cel->start;
        end = sqlUnsigned(row[5]) + cel->start;

        /* use phys_pos_from to check coords for non_randoms */
	phys_pos_from = sqlUnsigned(row[3]);
	if (phys_pos_from > 0 && phys_pos_from != start)
	   verbose(1, "unexpected coords snp_id = %d, ctg_id = %d, phys_pos_from = %d, start = %d\n", 
	               row[0], row[1], phys_pos_from, start); 
	
	fprintf(f, "%s\t%s\t%s\t%s\t%d\t%d\t%s\t%s\t%s\n", 
	            row[0], row[1], cel->chrom, row[2], start, end, row[6], row[7], (char *)el2->val);
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&f);
}


void createTable()
/* create a ContigLocFilter table */
{
struct sqlConnection *conn = hAllocConn();
char *createString =
"CREATE TABLE ContigLocFilter (\n"
"    snp_id int(11) not null,       \n"
"    ctg_id int(11) not null,       \n"
"    chromName varchar(32) not null,\n"
"    loc_type tinyint(4) not null,       \n"
"    start int(11) not null,       \n"
"    end int(11), \n"
"    orientation tinyint(4) not null, \n"
"    allele blob,\n"
"    weight int not null\n"
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
struct sqlConnection *conn ;

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

/* check for needed tables */
conn = hAllocConn();
if(!sqlTableExists(conn, "ContigLoc"))
    errAbort("no ContigLoc table in %s\n", snpDb);
if(!sqlTableExists(conn, "ContigInfo"))
    errAbort("no ContigInfo table in %s\n", snpDb);
if(!sqlTableExists(conn, "MapInfo"))
    errAbort("no MapInfo table in %s\n", snpDb);
if(!sqlTableExists(conn, "ctgPos"))
    errAbort("no ctgPos table in %s\n", snpDb);
hFreeConn(&conn);


loadCtgPos();
if (ctgPosHash == NULL) 
    {
    verbose(1, "couldn't get ctgPos hash\n");
    return 1;
    }

loadContigCoords(contigGroup);
if (contigCoords == NULL) 
    {
    verbose(1, "couldn't get contigCoords hash\n");
    return 2;
    }

getWeight();
if (weightHash == NULL)
    {
    verbose(1, "couldn't get MapInfo weight hash\n");
    return 3;
    }

filterSnps();
createTable();
loadDatabase();

return 0;
}

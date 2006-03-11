/* snpContigLocFilter - first step in dbSNP processing.
 * Filter the ContigLoc table in 2 ways: remove contigs that aren't in the reference assembly,
   and remove SNPs that have weight = 10 */
/* Translate to N_random, using ctgPos table (must exist). */ 
/* Create ContigLocFilter table. */
#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpContigLocFilter.c,v 1.22 2006/03/11 03:59:44 heather Exp $";

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

void getPoorlyAlignedSNPs()
/* hash all snp IDs that have weight = 10 into global weightHash */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;

weightHash = newHash(16);
verbose(1, "getting weight = 10 from SNPMapInfo...\n");
safef(query, sizeof(query), "select snp_id from SNPMapInfo where weight = 10");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(weightHash, cloneString(row[0]), NULL);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "SNPs with weight 10 found = %d\n", count);
}


void filterSnps()
/* Do a serial read through ContigLoc. */
/* For each SNP, get loc_type (1-6), orientation (strand) and allele (refNCBI). */
/* Output to ContigLocFilter table (tableName hard-coded). */
/* Use ctg_id to filter: only save rows where ctg_id is in our hash. */
/* Also, skip whenever weight = 10 from MapInfo weightHash. */

/* phys_pos_from (start) is always an integer. */
/* If phys_pos_from is 0, this is a random contig and we generate lifted coords. */

/* Format of phys_pos is based on loc_type: */
/* 1: num..num */
/* 2: num */
/* 3: num^num */
/* 4-6: NULL */
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
char endString[32];
int endNum = 0;
int loc_type = 0;

f = hgCreateTabFile(".", "ContigLocFilter");

safef(query, sizeof(query), 
    "select snp_id, ctg_id, loc_type, phys_pos_from, phys_pos, asn_from, asn_to, orientation, allele from ContigLoc");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el1 = hashLookup(contigCoords, row[1]);
    if (el1 != NULL)
        {
	el2 = hashLookup(weightHash, row[0]);
	if (el2 != NULL) continue;

	cel = hashFindVal(contigCoords, row[1]);

	loc_type = sqlUnsigned(row[2]);

	start = sqlUnsigned(row[3]);
	safef(endString, sizeof(endString), "%s", row[4]);

        /* lift randoms */
	if (start == 0)
	    {
	    start = sqlUnsigned(row[5]) + cel->start;
	    endNum = sqlUnsigned(row[6]) + cel->start;
	    // do this for loc_type == 1 as well?
	    if (loc_type == 3)
		start++;
	    safef(endString, sizeof(endString), "%d", endNum);
	    }
	
	fprintf(f, "%s\t%s\t%s\t%s\t%d\t%s\t%s\t%s\n", 
	            row[0], row[1], cel->chrom, row[2], start, endString, row[7], row[8]);
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
if(!hTableExistsDb(snpDb, "SNPMapInfo"))
    errAbort("no SNPMapInfo table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "ctgPos"))
    errAbort("no ctgPos table in %s\n", snpDb);


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

getPoorlyAlignedSNPs();
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

/* snpLoad - create snp table from build125 database. */
#include "common.h"
#include "hdb.h"
#include "snp125.h"

static char const rcsid[] = "$Id: snpLoad.c,v 1.7 2005/11/22 00:31:14 heather Exp $";

char *snpDb = NULL;
char *targetDb = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpLoad - create snp table from build125 database\n"
    "usage:\n"
    "    snpLoad snpDb targetDb \n");
}

boolean setCoords(struct snp125 *el, int snpClass, char *startString, char *endString)
{
char *rangeString1, *rangeString2;

el->chromStart = atoi(startString);

if (snpClass == 1) 
    {
    el->class = cloneString("range");
    rangeString1 = cloneString(endString);
    rangeString2 = strstr(rangeString1, "..");
    if (rangeString2 == NULL) 
        {
        verbose(1, "error processing range snp %s with phys_pos %s\n", rangeString1);
        return FALSE;
        }
    rangeString2 = rangeString2 + 2;
    el->chromEnd = atoi(rangeString2);
    return TRUE;
    }

if (snpClass == 2)
    {
    el->class = cloneString("simple");
    el->chromEnd = atoi(endString);
    return TRUE;
    }

if (snpClass == 3)
    {
    el->class = cloneString("deletion");
    rangeString1 = cloneString(endString);
    rangeString2 = strstr(rangeString1, "^");
    if (rangeString2 == NULL) 
        {
        verbose(1, "error processing deletion snp %s with phys_pos %s\n", rangeString1);
        return FALSE;
	}
    rangeString2 = rangeString2++;
    el->chromEnd = atoi(rangeString2);
    return TRUE;
    }

if (snpClass == 4)
    {
    el->class = cloneString("insertion");
    el->chromEnd = atoi(endString);
    return TRUE;
    }
verbose(1, "skipping snp %s with loc type %d\n", el->name, snpClass);
return FALSE;
}

struct snp125 *readSnps()
/* query ContigLoc */
{
struct snp125 *list=NULL, *el = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int snpClass;
int pos;

safef(query, sizeof(query), "select snp_id, ctg_id, loc_type, phys_pos_from, "
      "phys_pos, orientation, allele from ContigLoc where snp_type = 'rs'");

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    pos = atoi(row[3]);
    if (pos == 0) 
        {
	verbose(3, "snp %s is unaligned\n", el->name);
	continue;
	}
    snpClass = atoi(row[2]);
    if (snpClass < 1 || snpClass > 4) 
        {
	verbose(1, "skipping snp %s with loc_type %d\n", el->name, snpClass);
	continue;
	}
    AllocVar(el);
    el->name = cloneString(row[0]);
    /* store ctg_id in chrom for now, substitute later */
    el->chrom = cloneString(row[1]);
    el->score = 0;
    if(!setCoords(el, snpClass, row[3], row[4]))
        {
        free(el);
	continue;
	}
    if (atoi(row[5]) == 0) 
        strcpy(el->strand, "+");
    else if (atoi(row[5]) == 1)
        strcpy(el->strand, "-");
    else
        strcpy(el->strand, "?");
    el->observed = cloneString(row[6]);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return list;
}

boolean confirmCoords(int start1, int end1, int start2, int end2)
/* return TRUE if start1/end1 contained within start2/end2 */
{
    if (start1 < start2) return FALSE;
    if (end1 > end2) return FALSE;
    return TRUE;
}

void lookupContigs(struct snp125 *list)
{
struct snp125 *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *actualChrom;
boolean errorFound = FALSE;

verbose(1, "looking up contig chrom...\n");
for (el = list; el != NULL; el = el->next)
    {
    safef(query, sizeof(query), "select contig_chr, contig_start, contig_end from ContigInfo "
      "where ctg_id = '%s'", el->chrom);
    sr = sqlGetResult(conn, query);
    /* have a joiner check rule that assumes the contig is unique */
    /* also the index forces ctg_id to be unique */
    row = sqlNextRow(sr);
    actualChrom = cloneString(row[0]);
    if(!confirmCoords(el->chromStart, el->chromEnd, atoi(row[1]), atoi(row[2])))
        {
        verbose(1, "unexpected coords contig = %s, snp = %s\n", el->chrom, el->name);
        /* mark as error */
	el->chrom = "ERROR";
        }
    else
        el->chrom = actualChrom;
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}

void writeSnpTable(FILE *f, struct snp125 *list)
{
struct snp125 *el;
int score = 0;
float avHet = 0.0;
float avHetSE = 0.0;

for (el = list; el != NULL; el = el->next)
    {
    fprintf(f, "0 \t");
    fprintf(f, "chr%s \t", el->chrom);
    fprintf(f, "%d \t", el->chromStart);
    fprintf(f, "%d \t", el->chromEnd);
    fprintf(f, "rs%s \t", el->name);
    fprintf(f, "%d \t", score);
    fprintf(f, "%s \t", el->strand);
    fprintf(f, "N \t");
    fprintf(f, "%s \t", el->observed);
    fprintf(f, "unknown \t");
    fprintf(f, "%s \t", el->class);
    fprintf(f, "unknown \t");
    fprintf(f, "%f \t", avHet);
    fprintf(f, "%f \t", avHetSE);
    fprintf(f, "unknown \t");
    fprintf(f, "unknown \t");
    fprintf(f, "dbSNP125 \t");
    fprintf(f, "0 \t");
    fprintf(f, "\n");
    }
}

void loadDatabase(struct snp125 *list)
{
struct sqlConnection *conn = sqlConnect(snpDb);
FILE *f = hgCreateTabFile(".", "snp125");

writeSnpTable(f, list);

// hGetMinIndexLength requires chromInfo
// snp125TableCreate(conn, hGetMinIndexLength());
snp125TableCreate(conn, 32);

hgLoadTabFile(conn, ".", "snp125", &f);

}

void dumpSnps(struct snp125 *list)
{
struct snp125 *el;

verbose(1, "DUMPING...\n");
for (el = list; el != NULL; el = el->next)
    {
    verbose(1, "snp = %s, ", el->name);
    verbose(1, "chrom = %s, ", el->chrom);
    verbose(1, "start = %d, ", el->chromStart);
    verbose(1, "end = %d, ", el->chromEnd);
    verbose(1, "strand = %s, ", el->strand);
    verbose(1, "class = %s, ", el->class);
    verbose(1, "observed = %s\n", el->observed);
    }
}

int main(int argc, char *argv[])
/* Check args; read and load . */
{
struct snp125 *list=NULL, *el;

if (argc != 3)
    usage();

/* TODO: look in jksql for existence check for non-hgcentral database */
snpDb = argv[1];
targetDb = argv[2];
if(!hDbExists(targetDb))
    errAbort("%s does not exist\n", targetDb);

hSetDb(snpDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLoc"))
    errAbort("no ContigLoc table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "ContigInfo"))
    errAbort("no ContigInfo table in %s\n", snpDb);

/* this will create a temporary table */
list = readSnps();
lookupContigs(list);
verbose(1, "sorting\n");
slSort(&list, snp125Cmp);
// dumpSnps(list);
// hSetDb(targetDb);
verbose(1, "loading\n");
loadDatabase(list);
slFreeList(&list);
return 0;
}

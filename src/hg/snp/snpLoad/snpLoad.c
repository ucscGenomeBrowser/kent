/* snpLoad - create snp table from build125 database. */
#include "common.h"
#include "hdb.h"
#include "snp125.h"

static char const rcsid[] = "$Id: snpLoad.c,v 1.4 2005/11/10 00:50:39 heather Exp $";

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

void setCoords(struct snp125 *el, int snpClass, char *startString, char *endString)
{
char *rangeString1, *rangeString2;

if (snpClass == 1) 
    {
    el->class = cloneString("range");
    el->chromStart = atoi(startString);
    rangeString1 = cloneString(endString);
    rangeString2 = strstr(rangeString1, "..");
    if (rangeString2 == NULL) 
        {
        verbose(1, "error processing range snp %s with phys_pos %s\n", rangeString1);
        free(el);
        return;
        }
    rangeString2 = rangeString2 + 2;
    el->chromEnd = atoi(rangeString2);
    return;
    }

if (snpClass == 2)
    {
    el->class = cloneString("simple");
    el->chromStart = atoi(startString);
    el->chromEnd = atoi(endString);
    return;
    }

if (snpClass == 3)
    {
    el->class = cloneString("deletion");
    el->chromStart = atoi(startString);
    rangeString1 = cloneString(endString);
    rangeString2 = strstr(rangeString1, "^");
    if (rangeString2 == NULL) 
        {
        verbose(1, "error processing deletion snp %s with phys_pos %s\n", rangeString1);
        free(el);
        return;
	}
    rangeString2 = rangeString2++;
    el->chromEnd = atoi(rangeString2);
    return;
    }

if (snpClass == 4)
    {
    el->class = cloneString("insertion");
    el->chromStart = atoi(startString);
    el->chromEnd = atoi(endString);
    return;
    }
verbose(1, "skipping snp %s with loc type %d\n", el->name, snpClass);
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
    setCoords(el, snpClass, row[3], row[4]);
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
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
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

int main(int argc, char *argv[])
/* Check args; read and load . */
{
struct snp125 *list=NULL, *el;

if (argc != 3)
    usage();

snpDb = argv[1];
targetDb = argv[2];
if(!hDbExists(targetDb))
    errAbort("%s does not exist\n", targetDb);

hSetDb(snpDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLoc"))
    errAbort("no ContigLoc table in %s\n", snpDb);

/* this will create a temporary table */
list = readSnps();
lookupContigs(list);
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

hSetDb(targetDb);
// loadSnps();
return 0;
}

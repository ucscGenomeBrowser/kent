/* snpLoad - create snp table from build125 database. */
#include "common.h"

#include "chromInfo.h"
#include "hash.h"
#include "hdb.h"
#include "snp125.h"

static char const rcsid[] = "$Id: snpLoad.c,v 1.12 2005/11/29 02:58:53 heather Exp $";

char *snpDb = NULL;
char *targetDb = NULL;
static struct hash *chromHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpLoad - create snp table from build125 database\n"
    "usage:\n"
    "    snpLoad snpDb targetDb \n");
}

/* Copied from hgLoadWiggle. */
static struct hash *loadAllChromInfo()
/* Load up all chromosome infos. */
{
struct chromInfo *el;
struct sqlConnection *conn = sqlConnect(targetDb);
struct sqlResult *sr = NULL;
struct hash *ret;
char **row;

ret = newHash(0);

sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chromInfoLoad(row);
    verbose(4, "Add hash %s value %u (%#lx)\n", el->chrom, el->size, (unsigned long)&el->size);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return ret;
}

/* also copied from hgLoadWiggle. */
static unsigned getChromSize(char *chrom)
/* Return size of chrom.  */
{
struct hashEl *el = hashLookup(chromHash,chrom);

if (el == NULL)
    errAbort("Couldn't find size of chrom %s", chrom);
    return *(unsigned *)el->val;
}


boolean setCoords(struct snp125 *el, int snpClass, char *startString, char *endString)
/* set coords and class */
/* can switch to using #define */
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

/* set chromEnd = chromStart for insertions */
if (snpClass == 4)
    {
    el->class = cloneString("insertion");
    el->chromEnd = el->chromStart;
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

void lookupFunction(struct snp125 *list)
/* get function from ContigLocusId table */
{
struct snp125 *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int functionValue = 0;

verbose(1, "looking up function...\n");
for (el = list; el != NULL; el = el->next)
    {
    safef(query, sizeof(query), "select fxn_class from ContigLocusId where snp_id = '%s'", el->name);
    sr = sqlGetResult(conn, query);
    /* need a joiner check rule for this */
    row = sqlNextRow(sr);
    if (row == NULL)
        {
        el->func = cloneString("unknown");
	continue;
	}
    functionValue = atoi(row[0]);
    switch (functionValue)
        {
	    case 1:
                el->func = cloneString("unknown");
	        break;
	    case 2:
                el->func = cloneString("unknown");
	        break;
	    case 3:
                el->func = cloneString("coding-synon");
	        break;
	    case 4:
                el->func = cloneString("coding-nonsynon");
	        break;
	    case 5:
                el->func = cloneString("untranslated");
	        break;
	    case 6:
                el->func = cloneString("intron");
	        break;
	    case 7:
                el->func = cloneString("splice-site");
	        break;
	    case 8:
                el->func = cloneString("coding");
	        break;
	    default:
                el->func = cloneString("unknown");
	        break;
	}
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}


void lookupHet(struct snp125 *list)
/* get heterozygosity from SNP table */
{
struct snp125 *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

verbose(1, "looking up heterozygosity...\n");
for (el = list; el != NULL; el = el->next)
    {
    safef(query, sizeof(query), "select avg_heterozygosity, het_se from SNP where snp_id = '%s'", el->name);
    sr = sqlGetResult(conn, query);
    /* need a joiner check rule for this */
    row = sqlNextRow(sr);
    if (row == NULL)
        {
        el->avHet = 0.0;
        el->avHetSE = 0.0;
	continue;
	}
    el->avHet = atof(cloneString(row[0]));
    el->avHetSE = atof(cloneString(row[1]));
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}

void lookupRefAllele(struct snp125 *list)
/* get reference allele from nib files */
{
struct snp125 *el = NULL;
struct dnaSeq *seq;
char fileName[HDB_MAX_PATH_STRING];
char chromName[64];
int chromSize = 0;

AllocVar(seq);
verbose(1, "looking up reference allele...\n");
for (el = list; el != NULL; el = el->next)
    {
    el->reference = cloneString("n/a");
    if (sameString(el->chrom, "ERROR"))
        continue;
    if (sameString(el->class, "simple") || sameString(el->class, "range"))
        {
	strcpy(chromName, "chr");
	strcat(chromName, el->chrom);
        chromSize = getChromSize(chromName);
	if (el->chromStart > chromSize || el->chromEnd > chromSize)
	    {
	    verbose(1, "unexpected coords rs%s %s:%d-%d\n",
	            el->name, chromName, el->chromStart, el->chromEnd);
            continue;
	    }
        hNibForChrom2(chromName, fileName);
        seq = hFetchSeq(fileName, chromName, el->chromStart, el->chromEnd);
	touppers(seq->dna);
	el->reference = cloneString(seq->dna);
	}
    }
}

void writeSnpTable(FILE *f, struct snp125 *list)
/* write tab separated file */
{
struct snp125 *el;
int score = 0;
int bin = 0;

for (el = list; el != NULL; el = el->next)
    {
    bin = hFindBin(el->chromStart, el->chromEnd);
    fprintf(f, "%d\t", bin);
    fprintf(f, "chr%s\t", el->chrom);
    fprintf(f, "%d\t", el->chromStart);
    fprintf(f, "%d\t", el->chromEnd);
    fprintf(f, "rs%s\t", el->name);
    fprintf(f, "%d\t", score);
    fprintf(f, "%s\t", el->strand);
    fprintf(f, "%s\t", el->reference);
    fprintf(f, "%s\t", el->observed);
    fprintf(f, "unknown\t");
    fprintf(f, "%s\t", el->class);
    fprintf(f, "unknown\t");
    fprintf(f, "%f\t", el->avHet);
    fprintf(f, "%f\t", el->avHetSE);
    fprintf(f, "%s\t", el->func);
    fprintf(f, "unknown\t");
    fprintf(f, "dbSNP125\t");
    fprintf(f, "0\t");
    fprintf(f, "\n");
    }
}




void loadDatabase(struct snp125 *list)
/* write the tab file, create the table and load the tab file */
{
struct sqlConnection *conn2 = hAllocConn2();

FILE *f = hgCreateTabFile(".", "snp125");
writeSnpTable(f, list);

// hGetMinIndexLength requires chromInfo
// could add hGetMinIndexLength2 to hdb.c
// snp125TableCreate(conn2, hGetMinIndexLength2());
snp125TableCreate(conn2, 32);

hgLoadTabFile(conn2, ".", "snp125", &f);
hFreeConn2(&conn2);

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
// if(!hDbExists(targetDb))
    // errAbort("%s does not exist\n", targetDb);

hSetDb(snpDb);
hSetDb2(targetDb);
chromHash = loadAllChromInfo();


/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLoc"))
    errAbort("no ContigLoc table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "ContigInfo"))
    errAbort("no ContigInfo table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "ContigLocusId"))
    errAbort("no ContigLocusId table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "SNP"))
    errAbort("no SNP table in %s\n", snpDb);

list = readSnps();
lookupContigs(list);
lookupFunction(list);
lookupHet(list);
lookupRefAllele(list);

verbose(1, "sorting\n");
slSort(&list, snp125Cmp);

// dumpSnps(list);
verbose(1, "loading\n");
loadDatabase(list);
slFreeList(&list);
return 0;
}

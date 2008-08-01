/* illuminaLookup1M - generate data for Illumina 1M SNP arrays track. */
/* use UCSC snpXXX if dbSNP ID is found */
/* use Illumina data if not found */
/* This program is adopted from Heather's illuminaLookup2.c, with substantial simplyfication */

#include "common.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: illuminaLookup1M.c,v 1.1 2008/08/01 23:14:23 fanhsu Exp $";

struct snpSubset 
    {
    char *chrom;
    int start;
    int end;
    char *strand;
    char *observed;
    char *class;
    char *locType;
    };

void usage()
/* Explain usage and exit. */
{
errAbort(
    "illuminaLookup1M - just use rsId/chrom/pos from illumina\n"
    "usage:\n"
    "    illuminaLookup db illuminaPrelimTable snpTable\n");
}

struct hash *storeSnps(char *tableName)
/* store subset data for SNPs in a hash */
/* exclude SNPs that align multiple places */
{
struct hash *ret = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct snpSubset *subsetElement = NULL;

verbose(1, "creating SNP hash...\n");
ret = newHash(16);
safef(query, sizeof(query), 
      "select name, chrom, chromStart, chromEnd, strand, observed, class, locType from %s", 
      tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(subsetElement);
    subsetElement->chrom = cloneString(row[1]);
    subsetElement->start = sqlUnsigned(row[2]);
    subsetElement->end = sqlUnsigned(row[3]);
    subsetElement->strand = cloneString(row[4]);
    subsetElement->observed = cloneString(row[5]);
    subsetElement->class = cloneString(row[6]);
    subsetElement->locType = cloneString(row[7]);
    hashAdd(ret, cloneString(row[0]), subsetElement);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

void processSnps(struct hash *snpHash, char *illuminaTable)
/* read illuminaTable */
/* lookup details in snpHash */
/* report and skip if SNP missing, class != single, locType != exact */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct hashEl *hel = NULL;
struct snpSubset *subsetElement = NULL;
struct snpSubset *snp = NULL;

char *SNPname, *dbSNPrefStrand, *ILMNrefStrand, *SNPalleles, *GenomeBuild, *Chromosome, *Coordinate;
			    
FILE *output = mustOpen("illuminaLookup.out", "w");
FILE *errors = mustOpen("illuminaLookup.err", "w");

verbose(1, "process SNPs...\n");
safef(query, sizeof(query), "select * from %s", illuminaTable);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    SNPname 		= row[0];
    dbSNPrefStrand 	= row[1];
    ILMNrefStrand	= row[2];
    SNPalleles		= row[3];
    GenomeBuild		= row[4];
    Chromosome		= row[5];
    Coordinate		= row[6];
    
    hel = hashLookup(snpHash, SNPname);
    if (hel == NULL) 
        {
	fprintf(errors, "chr%s\t%d\t%d\t%s\t0\t%s\t%s\n", Chromosome, 
		atoi(Coordinate)-1, atoi(Coordinate), SNPname, ILMNrefStrand, SNPalleles);
	continue;
	}

    /* include all alignments */
    for (hel = hashLookup(snpHash, SNPname); hel != NULL; hel = hashLookupNext(hel))
 	{
    	subsetElement = (struct snpSubset *)hel->val;
    	snp = (struct snpSubset *)hel->val;
    
    	fprintf(output, "%s\t%d\t%d\t%s\t0\t%s\t%s\n", 
    	    	snp->chrom, snp->start, snp->end, SNPname, snp->strand, snp->observed);
   	}
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
carefulClose(&errors);
carefulClose(&output);
}

int main(int argc, char *argv[])
/* load SNPs into hash */
{
char *snpDb = NULL;
char *illuminaTableName = NULL;
char *snpTableName = NULL;

struct hash *snpHash = NULL;

if (argc != 4)
    usage();

snpDb = argv[1];
illuminaTableName = argv[2];
snpTableName = argv[3];

/* process args */
hSetDb(snpDb);
if (!hTableExists(illuminaTableName))
    errAbort("no %s table in %s\n", illuminaTableName, snpDb);
if (!hTableExists(snpTableName))
    errAbort("no %s table in %s\n", snpTableName, snpDb);

snpHash = storeSnps(snpTableName);
processSnps(snpHash, illuminaTableName);

return 0;
}

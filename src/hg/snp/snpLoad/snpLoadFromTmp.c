/* snpLoadFromTmp - create snp table from build125 database, assuming split tables. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

#include "chromInfo.h"
#include "dystring.h"
#include "hash.h"
#include "hdb.h"
#include "jksql.h"
#include "snp125.h"


char *snpDb = NULL;
char *targetDb = NULL;
static struct hash *chromHash = NULL;
static struct slName *chromList = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpLoadFromTmp - create snp table from build125 database, assuming split tables\n"
    "usage:\n"
    "    snpLoadFromTmp snpDb targetDb\n");
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

sr = sqlGetResult(conn, NOSQLINJ "select * from chromInfo");
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



struct snp125 *readSnps(char *chromName)
/* slurp in chrN_snpTmp */
{
struct snp125 *list=NULL, *el = NULL;
char tableName[64];
char query[512];
// struct sqlConnection *conn = hAllocConn2();
struct sqlConnection *conn = sqlConnect(targetDb);
struct sqlResult *sr;
char **row;
int count = 0;

verbose(1, "reading snps...\n");
strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snpTmp");

if (!sqlTableExists(conn, tableName))
    return list;

/* not checking chrom */
sqlSafef(query, sizeof(query), "select chrom, chromStart, chromEnd, name, strand, refNCBI, locType, func from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(el);
    el->chrom = chromName;
    el->score = 0;
    el->chromStart = atoi(row[1]);
    el->chromEnd = atoi(row[2]);
    el->name = cloneString(row[3]);
    strcpy(el->strand, row[4]);
    el->refNCBI = cloneString(row[5]);
    el->locType = cloneString(row[6]);
    el->func = cloneString(row[7]);
    slAddHead(&list,el);
    count++;
    }
sqlFreeResult(&sr);
// hFreeConn2(&conn);
sqlDisconnect(&conn);
verbose(1, "%d snps found\n", count);
slReverse(&list);
return list;
}




char *validString(int validCode)
{
switch (validCode)
    {
    case 0:
        return "unknown";
    case 1:
        return "by-cluster";
    case 2:
        return "by-frequency";
    case 3:
        return "by-cluster,by-frequency";
    case 4:
        return "by-submitter";
    case 5:
        return "by-submitter,by-cluster";
    case 6:
        return "by-submitter,by-frequency";
    case 7:
        return "by-submitter";
    case 8:
        return "by-2hit-2allele";
    case 9:
        return "by-2hit-2allele,by-cluster";
    case 10:
        return "by-2hit-2allele,by-frequency";
    case 11:
        return "by-2hit-2allele,by-frequency,by-cluster";
    case 12:
        return "by-2hit-2allele,by-submitter";
    case 13:
        return "by-2hit-2allele,by-submitter,by-cluster";
    case 14:
        return "by-2hit-2allele,by-submitter,by-frequency";
    case 15:
        return "by-2hit-2allele,by-submitter,by-frequency,by-cluster";
    case 16:
        return "by-hapmap";
    case 17:
        return "by-hapmap,by-cluster";
    case 18:
        return "by-hapmap,by-frequency";
    case 19:
        return "by-hapmap,by-frequency,by-cluster";
    case 20:
        return "by-hapmap,by-submitter";
    case 21:
        return "by-hapmap,by-submitter,by-cluster";
    case 22:
        return "by-hapmap,by-submitter,by-frequency";
    case 23:
        return "by-hapmap,by-submitter,by-frequency,by-cluster";
    case 24:
        return "by-hapmap,by-2hit-2allele";
    case 25:
        return "by-hapmap,by-2hit-2allele,by-cluster";
    case 26:
        return "by-hapmap,by-2hit-2allele,by-frequency";
    case 27:
        return "by-hapmap,by-2hit-2allele,by-frequency,by-cluster";
    case 28:
        return "by-hapmap,by-2hit-2allele,by-submitter";
    case 29:
        return "by-hapmap,by-2hit-2allele,by-submitter,by-cluster";
    case 30:
        return "by-hapmap,by-2hit-2allele,by-submitter,by-frequency";
    case 31:
        return "by-hapmap,by-2hit-2allele,by-submitter,by-frequency,by-cluster";
    default:
        return "unknown";
    }
}

void lookupHetAndValid(struct snp125 *list)
/* get heterozygosity from SNP table */
{
struct snp125 *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char snpName[32];

verbose(1, "looking up heterozygosity and validation status...\n");
for (el = list; el != NULL; el = el->next)
    {
    strcpy(snpName, el->name);
    stripString(snpName, "rs");
    sqlSafef(query, sizeof(query), "select avg_heterozygosity, validation_status, het_se from SNP where snp_id = %s", snpName);
    sr = sqlGetResult(conn, query);
    /* need a joiner check rule for this */
    row = sqlNextRow(sr);
    if (row == NULL)
        {
        el->avHet = 0.0;
        el->avHetSE = 0.0;
        sqlFreeResult(&sr);
	continue;
	}
    el->avHet = atof(cloneString(row[0]));
    el->avHetSE = atof(cloneString(row[2]));
    el->valid = validString(atoi(row[1]));
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}

void readFromSNPFasta(struct snp125 *list)
/* get molType, class and observed from snpFasta table */
/* not including chrom in query */
{
struct snp125 *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

verbose(1, "query snpFasta for molType, class, observed...\n");
for (el = list; el != NULL; el = el->next)
    {
    sqlSafef(query, sizeof(query), "select molType, class, observed from snpFasta where rsId = '%s'", el->name);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row == NULL)
        {
        el->observed = cloneString("n/a");
        el->class = cloneString("unknown");
	el->molType = cloneString("unknown");
        sqlFreeResult(&sr);
	continue;
	}

    el->molType = cloneString(row[0]);
    el->class = cloneString(row[1]);
    el->observed = cloneString(row[2]);

    /* class: special handling for in-dels */
    /* split into classes of our own construction */
    if (sameString(el->class, "in-del"))
        {
	if (sameString(el->locType, "between"))
	    el->class = cloneString("insertion");
	else if (sameString(el->locType, "range") || sameString(el->locType, "exact"))
	    el->class = cloneString("deletion");
	}

    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}

void lookupRefAllele(struct snp125 *list)
/* get reference allele from nib files */
/* for locType exact and locType range */
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
    if (sameString(el->locType, "unknown") || sameString(el->locType, "between"))
        {
        el->refUCSC = cloneString("n/a");
        continue;
        }

    strcpy(chromName, "chr");
    strcat(chromName, el->chrom);
    chromSize = getChromSize(chromName);
    if (el->chromStart > chromSize || el->chromEnd > chromSize || el->chromEnd <= el->chromStart)
        {
        verbose(1, "unexpected coords %s %s:%d-%d\n", el->name, chromName, el->chromStart, el->chromEnd);
	el->refUCSC = cloneString("n/a");
        continue;
        }

    hNibForChrom2(chromName, fileName);
    seq = hFetchSeq(fileName, chromName, el->chromStart, el->chromEnd);
    touppers(seq->dna);
    if (sameString(el->strand, "-"))
        reverseComplement(seq->dna, strlen(seq->dna));
    el->refUCSC = cloneString(seq->dna);
    }
}


void loadDatabase(char *chromName)
{
struct sqlConnection *conn2 = hAllocConn2();
char fileName[64];
char tableName[64];
FILE *f;

strcpy(tableName, "chr");
strcat(tableName, chromName);
strcat(tableName, "_snp125");
strcpy(fileName, "chr");
strcat(fileName, chromName);
strcat(fileName, "_snp125.tab");
verbose(1, "calling snp125TableCreate for %s\n", tableName);
snp125TableCreate(conn2, tableName);
f = mustOpen(fileName, "r");
verbose(1, "loading %s...\n", tableName);
hgLoadTabFile(conn2, ".", tableName, &f);
verbose(1, "back from hgLoadTabFile\n");
hFreeConn2(&conn2);
}

void writeToTabFile(char *chromName, struct snp125 *list)
/* write tab separated file */
{
struct snp125 *el;
int score = 0;
int bin = 0;
int count = 0;
FILE *f;
char fileName[64];

strcpy(fileName, "chr");
strcat(fileName, chromName);
strcat(fileName, "_snp125");
f = hgCreateTabFile(".", fileName);

for (el = list; el != NULL; el = el->next)
    {
    bin = hFindBin(el->chromStart, el->chromEnd);
    fprintf(f, "%d\t", bin);
    fprintf(f, "chr%s\t", el->chrom);
    fprintf(f, "%d\t", el->chromStart);
    fprintf(f, "%d\t", el->chromEnd);
    fprintf(f, "%s\t", el->name);
    fprintf(f, "%d\t", score);
    fprintf(f, "%s\t", el->strand);
    fprintf(f, "%s\t", el->refNCBI);
    fprintf(f, "%s\t", el->refUCSC);
    fprintf(f, "%s\t", el->observed);
    fprintf(f, "%s\t", el->molType);
    fprintf(f, "%s\t", el->class);
    fprintf(f, "%s\t", el->valid);
    fprintf(f, "%f\t", el->avHet);
    fprintf(f, "%f\t", el->avHetSE);
    fprintf(f, "%s\t", el->func);
    fprintf(f, "%s\t", el->locType);
    fprintf(f, "dbSNP125");
    fprintf(f, "\n");
    count++;
    }
if (count > 0)
    verbose(1, "%d lines written\n", count);
fclose(f);
}




int main(int argc, char *argv[])
/* Check args; read and load . */
{
struct snp125 *list=NULL, *el;
struct slName *chromPtr, *contigList;

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
chromList = hAllChromNamesDb(targetDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLocusId"))
    errAbort("no ContigLocusId table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "SNP"))
    errAbort("no SNP table in %s\n", snpDb);

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    stripString(chromPtr->name, "chr");
    }

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    verbose(1, "chrom = %s\n", chromPtr->name);
    list = readSnps(chromPtr->name);

    if (list == NULL) 
        {
        verbose(1, "--------------------------------------\n");
        continue;
	}
    lookupHetAndValid(list);
    readFromSNPFasta(list);
    lookupRefAllele(list);
    writeToTabFile(chromPtr->name, list);
    slFreeList(&list);
    loadDatabase(chromPtr->name);
    verbose(1, "---------------------------------------\n");
    }

return 0;
}

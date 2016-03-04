/* snpSplit - split the ContigLoc table by chrom. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"

#include "chromInfo.h"
#include "dystring.h"
#include "hash.h"
#include "hdb.h"
#include "snpTmp.h"


/* these are described in b125_mapping.doc */
/* Also snpFixed.LocTypeCode */
char *locTypeStrings[] = {
    "range",
    "exact",
    "between",
    "rangeInsertion",
    "rangeSubstitution",
    "rangeDeletion",
};

char *functionStrings[] = {
/* From snpFixed.SnpFunctionCode. */
    "unknown", 
    "locus", 
    "coding", 
    "coding-synon", 
    "coding-nonsynon", 
    "untranslated", 
    "intron", 
    "splice-site", 
    "cds-reference", 
};

char *snpDb = NULL;
char *targetDb = NULL;
char *contigGroup = NULL;
static struct hash *chromHash = NULL;
static struct slName *chromList = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpSplit - split the ContigLoc table by chrom\n"
    "usage:\n"
    "    snpSplit snpDb targetDb contigGroup\n");
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


boolean setCoordsAndLocType(struct snpTmp *el, int locType, int lc_ngbr, int rc_ngbr,
                            char *startString, char *endString, int alleleSize)
/* set coords and locType */
{
char *rangeString1, *rangeString2;
int rangeInContig = 0;

el->chromStart = atoi(startString);

/* range */
if (locType == 1) 
    {
    el->locType = locTypeStrings[0];
    rangeString1 = cloneString(endString);
    rangeString2 = strstr(rangeString1, "..");
    if (rangeString2 == NULL) 
        {
        verbose(1, "error processing range snp %s with phys_pos %s\n", rangeString1);
        return FALSE;
        }
    rangeString2 = rangeString2 + 2;
    el->chromEnd = atoi(rangeString2);
    if (el->chromEnd <= el->chromStart)
        {
        verbose(1, "unexpected range coords (end <= start) %s\n", el->name);
	return FALSE;
	}
    return TRUE;
    }

/* exact */
if (locType == 2)
    {
    el->locType = locTypeStrings[1];
    el->chromEnd = atoi(endString);
    if (el->chromEnd != el->chromStart + 1)
        {
	verbose(1, "unexpected exact coords (end != start + 1) %s\n", el->name);
	return FALSE;
	}
    return TRUE;
    }

/* between */
if (locType == 3)
    {
    el->locType = locTypeStrings[2];
    // rangeString1 = cloneString(endString);
    // rangeString2 = strstr(rangeString1, "^");
    // if (rangeString2 == NULL) 
        // {
        // verbose(1, "error processing snp %s with phys_pos %s\n", rangeString1);
        // return FALSE;
	// }
    el->chromEnd = el->chromStart;
    return TRUE;
    }

/* rangeInsertion */
if (locType == 4)
    {
    el->locType = locTypeStrings[3];
    rangeInContig = rc_ngbr - lc_ngbr - alleleSize;
    el->chromEnd = el->chromStart + rangeInContig;
    return TRUE;
    }

/* rangeSubstitution */
if (locType == 5)
    {
    el->locType = locTypeStrings[4];
    el->chromEnd = el->chromStart + alleleSize;
    return TRUE;
    }

/* rangeDeletion */
if (locType == 6)
    {
    el->locType = locTypeStrings[5];
    rangeInContig = rc_ngbr - lc_ngbr - alleleSize;
    el->chromEnd = el->chromStart + rangeInContig;
    return TRUE;
    }

verbose(1, "skipping snp %s with loc type %d\n", el->name, locType);
return FALSE;
}

void lookupFunction(struct snpTmp *list)
/* get function from ContigLocusId table */
/* can be multiple matches */
/* function values are defined in snpFixed.SnpFunctionCode */
{
struct snpTmp *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int functionValue = 0;
char snpName[32];
struct dyString *dy = NULL;
int funcCount = 0;
int newSize = 0;

for (el = list; el != NULL; el = el->next)
{
    dy = newDyString(256);
    strcpy(snpName, el->name);
    stripString(snpName, "rs");

    sqlSafef(query, sizeof(query), "select count(*) from ContigLocusId where snp_id = %s and ctg_id = %s", snpName, el->contigName);
    funcCount = sqlQuickNum(conn, query);
    if (funcCount == 0)
        {
        el->func = cloneString("unknown");
        continue;
        }

    sqlSafef(query, sizeof(query), "select fxn_class from ContigLocusId where snp_id = %s and ctg_id = %s", snpName, el->contigName);
    sr = sqlGetResult(conn, query);

    while ((row = sqlNextRow(sr)) != NULL)
        {
        functionValue = atoi(row[0]);
        if (functionValue > 8)
            {
            verbose(1, "unexpected function value %d for %s; setting to 0 (zero)", functionValue, snpName);
            functionValue = 0;
            }
        dyStringPrintf(dy, "%s,", functionStrings[functionValue]);
        }

    // chop off the last character
    newSize = dy->stringSize - 1;
    dyStringResize(dy, newSize);
    el->func = cloneString(dy->string);
    sqlFreeResult(&sr);
    freeDyString(&dy);
    }
hFreeConn(&conn);
}

struct snpTmp *readSnps(char *chromName, struct slName *contigList)
/* query ContigLoc for all snps in contig */
{
struct snpTmp *list=NULL, *el = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int locType;
int pos;
int snpCount = 0;
int contigCount = 0;
struct slName *contigPtr;

verbose(1, "reading snps...\n");
for (contigPtr = contigList; contigPtr != NULL; contigPtr = contigPtr->next)
    {
    contigCount++;
    verbose(1, "contig count = %d\n", contigCount);
    sqlSafef(query, sizeof(query), "select snp_id, ctg_id, loc_type, lc_ngbr, rc_ngbr, phys_pos_from, "
      "phys_pos, orientation, allele from ContigLoc where snp_type = 'rs' and ctg_id = '%s'", contigPtr->name);
    verbose(3, "ctg_id = %d\n", contigPtr->name);

    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        pos = atoi(row[5]);
        if (pos == 0) 
            {
	    verbose(5, "snp %s is unaligned\n", row[0]);
	    continue;
	    }
        locType = atoi(row[2]);
        if (locType < 1 || locType > 6) 
            {
	    verbose(1, "skipping snp %s with loc_type %d\n", el->name, locType);
	    continue;
	    }
        AllocVar(el);
        el->name = cloneString(row[0]);
        el->chrom = chromName;
        if(!setCoordsAndLocType(el, locType, atoi(row[3]), atoi(row[4]), row[5], row[6], strlen(row[8])))
            {
            free(el);
	    continue;
	    }
        if (atoi(row[7]) == 0) 
            strcpy(el->strand, "+");
        else if (atoi(row[7]) == 1)
            strcpy(el->strand, "-");
        else
            strcpy(el->strand, "?");
        el->refNCBI = cloneString(row[8]);
	// should this be reverseComplemented for negative strand??!!
	if (sameString(el->locType, "between") && !sameString(el->refNCBI, "-"))
	    verbose(1, "unexpected between allele %s for %s\n", el->refNCBI, el->name);
	el->contigName = cloneString(contigPtr->name);
        slAddHead(&list,el);
	snpCount++;
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
verbose(1, "%d snps found\n", snpCount);
return list;
}

struct slName *getContigs(char *chromName)
/* get list of contigs that are from chrom */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct slName *contigList = NULL;
struct slName *el = NULL;
int count = 0;

verbose(1, "getting contigs...\n");
sqlSafef(query, sizeof(query), "select ctg_id from ContigInfo where contig_chr = '%s' and group_term = '%s'", chromName, contigGroup);
verbose(5, "query = %s\n", query);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    verbose(5, "contig = %s\n", row[0]);
    el = newSlName(row[0]);
    slAddHead(&contigList, el);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "contigs found = %d\n", count);
return(contigList);
}


void writeToTabFile(FILE *f, struct snpTmp *list)
/* write tab separated file */
{
struct snpTmp *el;
int count = 0;

verbose(1, "writing to tab file...\n");
for (el = list; el != NULL; el = el->next)
    {
    if (sameString(el->chrom, "ERROR")) continue;
    fprintf(f, "chr%s\t", el->chrom);
    fprintf(f, "%d\t", el->chromStart);
    fprintf(f, "%d\t", el->chromEnd);
    fprintf(f, "rs%s\t", el->name);
    fprintf(f, "%s\t", el->strand);
    fprintf(f, "%s\t", el->refNCBI);
    fprintf(f, "%s\t", el->locType);
    fprintf(f, "%s\t", el->func);
    /* don't need this going forward, but easier to write it */
    /* since using snpTmp.h */
    fprintf(f, "%s\t", el->contigName);
    fprintf(f, "\n");
    count++;
    }
verbose(1, "%d lines written\n", count);
}




void loadDatabase(char *chromName, struct snpTmp *list)
{
FILE *f;
struct sqlConnection *conn2 = hAllocConn2();
char fileName[64];

strcpy(fileName, "chr");
strcat(fileName, chromName);
strcat(fileName, "_snpTmp");
f = hgCreateTabFile(".", fileName);
writeToTabFile(f, list);
snpTmpTableCreate(conn2, fileName);
verbose(1, "loading...\n");
hgLoadTabFile(conn2, ".", fileName, &f);
hFreeConn2(&conn2);
}

int main(int argc, char *argv[])
/* Check args; read and load . */
{
struct snpTmp *list=NULL, *el;
struct slName *chromPtr, *contigList;
char tableName[64];
struct sqlConnection *conn;

if (argc != 4)
    usage();

/* TODO: look in jksql for existence check for non-hgcentral database */
snpDb = argv[1];
targetDb = argv[2];
contigGroup = argv[3];
// if(!hDbExists(targetDb))
    // errAbort("%s does not exist\n", targetDb);

hSetDb(snpDb);
hSetDb2(targetDb);
chromHash = loadAllChromInfo();
chromList = hAllChromNamesDb(targetDb);

/* check for needed tables */
if(!hTableExistsDb(snpDb, "ContigLoc"))
    errAbort("no ContigLoc table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "ContigInfo"))
    errAbort("no ContigInfo table in %s\n", snpDb);
if(!hTableExistsDb(snpDb, "ContigLocusId"))
    errAbort("no ContigLocusId table in %s\n", snpDb);

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    stripString(chromPtr->name, "chr");
    }

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {

    conn = sqlConnect(targetDb);
    strcpy(tableName, "chr");
    strcat(tableName, cloneString(chromPtr->name));
    strcat(tableName, "_snpTmp");
    if (sqlTableExists(conn, tableName)) continue;
    sqlDisconnect(&conn);

    verbose(1, "chrom = %s\n", chromPtr->name);
    contigList = getContigs(chromPtr->name);
    if (contigList == NULL) continue;
    list = readSnps(chromPtr->name, contigList);
    slFreeList(&contigList);
    if (list == NULL)
        {
	verbose(1, "no SNPs found\n");
	continue;
	}
    lookupFunction(list);
    slSort(&list, snpTmpCmp);
    loadDatabase(chromPtr->name, list);
    slFreeList(&list);
    }

return 0;
}

/* snpSort - fifth step in dbSNP processing.
 * Read the chrN_snpTmp tables into memory and sort by position.
 * Rewrite to chrN_snpTmp tables.  
 * Get chromInfo from ContigInfo. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"

#include "hdb.h"


struct snpTmp
    {
    struct snpTmp *next;  	        
    int snp_id;			
    int ctg_id;			
    int start;            
    int end;
    int loc_type;
    int orientation;
    char *allele;	
    int weight;
    };

static char *snpDb = NULL;
static char *contigGroup = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpSort - sort chrN_snpTmp by chromStart\n"
    "usage:\n"
    "    snpSort snpDb contigGroup\n");
}


struct snpTmp *snpTmpLoad(char **row)
/* Load a snpTmp from row fetched from snp table
 * in database.  Dispose of this with snpTmpFree(). */
{
struct snpTmp *ret;

AllocVar(ret);
ret->snp_id = sqlUnsigned(row[0]);
ret->ctg_id = sqlUnsigned(row[1]);
ret->start = sqlUnsigned(row[2]);
ret->end = sqlUnsigned(row[3]);
ret->loc_type = sqlUnsigned(row[4]);
ret->orientation = sqlUnsigned(row[5]);
ret->allele   = cloneString(row[6]);
ret->weight   = sqlUnsigned(row[7]);

return ret;

}

int snpTmpCmp(const void *va, const void *vb)
/* comparison function. */
{
const struct snpTmp *a = *((struct snpTmp **)va);
const struct snpTmp *b = *((struct snpTmp **)vb);
int dif = a->start - b->start;
return dif;
}


void snpTmpFree(struct snpTmp **pEl)
/* Free a single dynamically allocated snpTmp such as created with snpTmpLoad(). */
{
struct snpTmp *el;

if ((el = *pEl) == NULL) return;
freeMem(el->allele);
freez(pEl);
}

void snpTmpFreeList(struct snpTmp **pList)
/* Free a list of dynamically allocated snpTmp's */
{
struct snpTmp *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    snpTmpFree(&el);
    }
*pList = NULL;
}

struct slName *getChromListFromContigInfo(char *contigGroup)
/* get all chromNames that match contigGroup */
{
struct slName *ret = NULL;
struct slName *el = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char chromName[64];

sqlSafef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s' and contig_end != 0", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(chromName, sizeof(chromName), "%s", row[0]);
    el = slNameNew(chromName);
    slAddHead(&ret, el);
    }
sqlFreeResult(&sr);

sqlSafef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s' and contig_end = 0", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(chromName, sizeof(chromName), "%s_random", row[0]);
    el = slNameNew(chromName);
    slAddHead(&ret, el);
    }
sqlFreeResult(&sr);

hFreeConn(&conn);
return ret;
}


struct snpTmp *readSnps(char *chromName)
/* slurp in all rows for this chrom */
{
struct snpTmp *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];

safef(tableName, ArraySize(tableName), "chr%s_snpTmp", chromName);
sqlSafef(query, sizeof(query), "select snp_id, ctg_id, chromStart, chromEnd, loc_type, orientation, allele, weight from %s ", tableName);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = snpTmpLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
slReverse(&list);
hFreeConn(&conn);
return list;
}


void writeSnps(char *chromName, struct snpTmp *list)
/* dump to tab file */
{
struct snpTmp *el = NULL;
char fileName[64];
FILE *f = NULL;

safef(fileName, ArraySize(fileName), "chr%s_snpTmp.tab", chromName);
f = mustOpen(fileName, "w");

for (el = list; el != NULL; el = el->next)
    fprintf(f, "%d\t%d\t%d\t%d\t%d\t%d\t%s\t%d\n", 
            el->snp_id, el->ctg_id, el->start, el->end, el->loc_type, el->orientation, el->allele, el->weight);

carefulClose(&f);
}


void cleanDatabaseTable(char *chromName)
/* do a 'delete from tableName' */
{
struct sqlConnection *conn = hAllocConn();
char query[256];

sqlSafef(query, ArraySize(query), "delete from chr%s_snpTmp", chromName);
sqlUpdate(conn, query);
hFreeConn(&conn);
}


void loadDatabase(char *chromName)
/* load one table into database */
{
FILE *f;
struct sqlConnection *conn = hAllocConn();
char tableName[64], fileName[64];

safef(tableName, ArraySize(tableName), "chr%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "chr%s_snpTmp.tab", chromName);

f = mustOpen(fileName, "r");
/* hgLoadTabFile closes the file handle */
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, sort, rewrite to individual chrom tables */
{
struct slName *chromList, *chromPtr;
struct snpTmp *snpList = NULL;

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

chromList = getChromListFromContigInfo(contigGroup);
if (chromList == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    verbose(1, "chrom = %s\n", chromPtr->name);
    verbose(5, "reading snps...\n");
    snpList = readSnps(chromPtr->name);
    verbose(5, "sorting...\n");
    slSort(&snpList, snpTmpCmp);
    verbose(5, "dumping...\n");
    writeSnps(chromPtr->name, snpList);
    /* snpSort doesn't change the table format, so just delete old rows */
    verbose(5, "delete from table...\n");
    cleanDatabaseTable(chromPtr->name);
    verbose(5, "load...\n");
    loadDatabase(chromPtr->name);
    verbose(5, "free...\n");
    snpTmpFreeList(&snpList);
    verbose(5, "------------------------\n");
    }

return 0;
}

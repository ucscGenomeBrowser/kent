/* snpSort - fifth step in dbSNP processing.
 * Read the chrN_snpTmp tables and sort.
 * Rewrite to chrN_snpTmp tables.  
 * Get chromInfo from ContigInfo. */

#include "common.h"

#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpSort.c,v 1.2 2006/02/10 22:08:00 heather Exp $";

struct snpTmp
    {
    struct snpTmp *next;  	        
    int snp_id;			
    int start;            
    int end;
    int loc_type;
    int orientation;
    char *allele;	
    };

static char *snpDb = NULL;
static char *contigGroup = NULL;
static struct hash *chromHash = NULL;

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
ret->snp_id = atoi(row[0]);
ret->start = atoi(row[1]);
ret->end = atoi(row[2]);
ret->loc_type = atoi(row[3]);
ret->orientation = atoi(row[4]);
ret->allele   = cloneString(row[5]);

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

struct hash *loadChroms(char *contigGroup)
/* hash all chromNames that match contigGroup */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *chromName;

ret = newHash(0);
safef(query, sizeof(query), "select distinct(contig_chr) from ContigInfo where group_term = '%s'", contigGroup);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chromName = cloneString(row[0]);
    hashAdd(ret, chromName, NULL);
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
safef(query, sizeof(query), "select snp_id, chromStart, chromEnd, loc_type, orientation, allele from %s ", tableName);
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
    fprintf(f, "%d\t%d\t%d\t%d\t%d\t%s\n", 
            el->snp_id, el->start, el->end, el->loc_type, el->orientation, el->allele);

carefulClose(&f);
}


void cleanDatabaseTable(char *chromName)
/* do a 'delete from tableName' */
{
struct sqlConnection *conn = hAllocConn();
char query[256];

safef(query, ArraySize(query), "delete from chr%s_snpTmp", chromName);
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
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;
struct snpTmp *snpList = NULL;

if (argc != 3)
    usage();

snpDb = argv[1];
contigGroup = argv[2];
hSetDb(snpDb);

chromHash = loadChroms(contigGroup);
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    verbose(1, "chrom = %s\n", chromName);
    verbose(5, "reading snps...\n");
    snpList = readSnps(chromName);
    verbose(5, "sorting...\n");
    slSort(&snpList, snpTmpCmp);
    verbose(5, "dumping...\n");
    writeSnps(chromName, snpList);
    /* snpSort doesn't change the table format, so just delete old rows */
    verbose(5, "delete from table...\n");
    cleanDatabaseTable(chromName);
    verbose(5, "load...\n");
    loadDatabase(chromName);
    verbose(5, "free...\n");
    snpTmpFreeList(&snpList);
    verbose(5, "------------------------\n");
    }

return 0;
}

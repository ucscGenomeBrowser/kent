/* snpMultiple - eleventh step in dbSNP processing.
 * Run on hgwdev.
 * Read snp125.
 * Load named, chrom, chromStart, chromEnd into memory.
 * Check count per name. */

#include "common.h"

#include "hdb.h"

static char const rcsid[] = "$Id: snpMultiple.c,v 1.1 2006/02/23 06:24:30 heather Exp $";

struct snpTmp
    {
    struct snpTmp *next;  	        
    char *name;			
    };

static char *snpDb = NULL;
FILE *outputFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpMultiple - read snp125 for rsIds that appear more than once\n"
    "usage:\n"
    "    snpMultiple snpDb \n");
}


struct snpTmp *snpTmpLoad(char **row)
/* Load a snpTmp from row fetched from snp table
 * in database.  Dispose of this with snpTmpFree(). */
{
struct snpTmp *ret;

AllocVar(ret);
ret->name = cloneString(row[0]);

return ret;

}


void snpTmpFree(struct snpTmp **pEl)
/* Free a single dynamically allocated snpTmp such as created with snpTmpLoad(). */
{
struct snpTmp *el;

if ((el = *pEl) == NULL) return;
freeMem(el->name);
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


struct snpTmp *readSnps()
/* slurp in all names */
{
struct snpTmp *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

verbose(1, "getting distinct(name)...\n");
safef(query, sizeof(query), "select distinct(name) from snp125");
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


void getCount(struct snpTmp *list)
/* get count */
{
struct snpTmp *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;

verbose(1, "checking counts...\n");
for (el = list; el != NULL; el = el->next)
    {
    safef(query, sizeof(query), "select count(*) from snp125 where name = %s", el->name);
    count = sqlQuickNum(conn, query);
    if (count == 0) continue;
    safef(query, sizeof(query), "select chrom, chromStart, chromEnd from snp125 where name = %s", el->name);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	fprintf(outputFileHandle, "%s\t%s\t%s\t%s\n", el->name, row[0], row[1], row[2]);
	verbose(1, "%s\t%s\t%s\t%s\n", el->name, row[0], row[1], row[2]);
	}
    verbose(1, "----------------------------\n");
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* get distinct names from snp125, record count > 1 */
{
struct snpTmp *snpList = NULL;
char tableName[64];

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);
outputFileHandle = mustOpen("snpMultiple.tab", "w");
snpList = readSnps();
getCount(snpList);
snpTmpFreeList(&snpList);
carefulClose(&outputFileHandle);

return 0;
}

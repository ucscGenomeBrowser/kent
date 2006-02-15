/* snpReadFasta - eighth step in dbSNP processing.
 * Read the chrN_snpTmp tables into memory.
 * Do lookups into snpFasta for class, observed, molType. 
 * Use UCSC chromInfo. */

/* Could drop refUCSCReverseComp here */

/* This program is fairly slow.  Takes about an hour.
   It would probably be good to use a hash.
   It might also be somewhat faster to sort the snpTmp list by snp_id.
   The rs_fasta files are sorted by rsId. */


#include "common.h"

#include "chromInfo.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpReadFasta.c,v 1.4 2006/02/15 21:15:03 heather Exp $";

struct snpTmp
    {
    struct snpTmp *next;  	        
    int snp_id;			
    int start;            
    int end;
    int loc_type;
    int orientation;
    char *allele;	
    char *refUCSC;
    char *refUCSCReverseComp;
    };

static char *snpDb = NULL;
static struct hash *chromHash = NULL;
FILE *errorFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpReadFasta - lookup molType, class and observed in chrN_snpFasta and chrMulti_snpFasta\n"
    "check for errors in observed\n"
    "recreate chrN_snpTmp\n"
    "usage:\n"
    "    snpReadFasta snpDb \n");
}


struct snpTmp *snpTmpLoad(char **row)
/* Load a snpTmp from row fetched from snp table
 * in database.  Dispose of this with snpTmpFree(). */
{
struct snpTmp *ret;

AllocVar(ret);
ret->snp_id = sqlUnsigned(row[0]);
ret->start = sqlUnsigned(row[1]);
ret->end = sqlUnsigned(row[2]);
ret->loc_type = sqlUnsigned(row[3]);
ret->orientation = sqlUnsigned(row[4]);
ret->allele   = cloneString(row[5]);
ret->refUCSC   = cloneString(row[6]);
ret->refUCSCReverseComp   = cloneString(row[7]);

return ret;

}


void snpTmpFree(struct snpTmp **pEl)
/* Free a single dynamically allocated snpTmp such as created with snpTmpLoad(). */
{
struct snpTmp *el;

if ((el = *pEl) == NULL) return;
freeMem(el->allele);
freeMem(el->refUCSC);
freeMem(el->refUCSCReverseComp);
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


struct hash *loadChroms()
/* hash from UCSC chromInfo */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *randomSubstring = NULL;
struct chromInfo *el;
char tableName[64];

ret = newHash(0);
safef(query, sizeof(query), "select chrom, size from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    randomSubstring = strstr(row[0], "random");
    if (randomSubstring != NULL) continue;
    safef(tableName, ArraySize(tableName), "%s_snpTmp", row[0]);
    if (!hTableExists(tableName)) continue;
    el = chromInfoLoad(row);
    hashAdd(ret, el->chrom, (void *)(& el->size));
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

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);

safef(query, sizeof(query), 
     "select snp_id, chromStart, chromEnd, loc_type, orientation, allele, refUCSC, refUCSCReverseComp from %s ", tableName);
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


void readFasta(char *chromName, struct snpTmp *list)
/* get molType, class and observed from snpFasta table */
{
struct snpTmp *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char fileName[64];
FILE *f;
boolean gotMatch = FALSE;
char *classString = NULL;
char fastaTableName[64];

safef(fastaTableName, ArraySize(fastaTableName), "%s_snpFasta", chromName);

safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);
f = mustOpen(fileName, "w");

verbose(5, "query snpFasta for molType, class, observed...\n");
for (el = list; el != NULL; el = el->next)
    {
    safef(query, sizeof(query), 
          "select molType, class, observed from %s where rsId = 'rs%d'", fastaTableName, el->snp_id);
    sr = sqlGetResult(conn, query);
    gotMatch = FALSE;
    while ((row = sqlNextRow(sr)) != NULL)
        {
	gotMatch = TRUE;
	/* special handling for class = in-del; split into classes of our own construction */
	classString = cloneString(row[1]);
	if (sameString(classString, "in-del"))
	    {
	    if (el->loc_type == 3) 
	        classString = cloneString("insertion");
	    else 
	        classString = cloneString("deletion");
	    }

        fprintf(f, "%d\t%d\t%d\t%d\t%s\t%d\t%s\t%s\t%s\t%s\t%s\n", 
            el->snp_id, el->start, el->end, el->loc_type, classString, el->orientation, row[0],
	    el->allele, el->refUCSC, el->refUCSCReverseComp, row[2]);
	}
    sqlFreeResult(&sr);
    if (gotMatch) continue;
    safef(query, sizeof(query), 
      "select molType, class, observed from chrMulti_snpFasta where rsId = 'rs%d'", el->snp_id);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	gotMatch = TRUE;
	/* special handling for class = in-del; split into classes of our own construction */
	classString = cloneString(row[1]);
	if (sameString(classString, "in-del"))
	    {
	    if (el->loc_type == 3) 
	        classString = cloneString("insertion");
	    else 
	        classString = cloneString("deletion");
	    }

        fprintf(f, "%d\t%d\t%d\t%d\t%s\t%d\t%s\t%s\t%s\t%s\t%s\n", 
            el->snp_id, el->start, el->end, el->loc_type, classString, el->orientation, row[0],
	    el->allele, el->refUCSC, el->refUCSCReverseComp, row[2]);
	}

    /* log missing fasta, preserve SNP */
    if (!gotMatch)
        {
        fprintf(errorFileHandle, "no snpFasta for %d on %s\n", el->snp_id, chromName);
        fprintf(f, "%d\t%d\t%d\t%d\t%s\t%d\t%s\t%s\t%s\t%s\t%s\n", 
            el->snp_id, el->start, el->end, el->loc_type, "unknown", el->orientation, "unknown",
	    el->allele, el->refUCSC, el->refUCSCReverseComp, "n/a");
	}
    }
hFreeConn(&conn);
carefulClose(&f);
}




void recreateDatabaseTable(char *chromName)
/* create a new chrN_snpTmp table with new definition */
{
struct sqlConnection *conn = hAllocConn();
char tableName[64];
char *createString =
"CREATE TABLE %s (\n"
"    snp_id int(11) not null,\n"
"    chromStart int(11) not null,\n"
"    chromEnd int(11) not null,\n"
"    loc_type tinyint(4) not null,\n"
"    class varchar(255) not null,\n"
"    orientation tinyint(4) not null,\n"
"    molType varchar(255) not null,\n"
"    allele blob,\n"
"    refUCSC blob,\n"
"    refUCSCReverseComp blob,\n"
"    observed blob\n"
");\n";

struct dyString *dy = newDyString(1024);

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
dyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
hFreeConn(&conn);
}


void loadDatabase(char *chromName)
/* load one table into database */
{
FILE *f;
struct sqlConnection *conn = hAllocConn();
char tableName[64], fileName[64];

safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);

f = mustOpen(fileName, "r");
/* hgLoadTabFile closes the file handle */
hgLoadTabFile(conn, ".", tableName, &f);

hFreeConn(&conn);
}



int main(int argc, char *argv[])
/* read chrN_snpTmp, lookup in snpFasta, rewrite to individual chrom tables */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;
struct snpTmp *snpList = NULL;
char tableName[64];

if (argc != 2)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

errorFileHandle = mustOpen("snpReadFasta.errors", "w");

chromHash = loadChroms();
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
    if (!hTableExists(tableName)) continue;

    verbose(1, "chrom = %s\n", chromName);

    snpList = readSnps(chromName);
    readFasta(chromName, snpList);
    snpTmpFreeList(&snpList);
    }

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
    if (!hTableExists(tableName)) continue;
    recreateDatabaseTable(chromName);
    verbose(1, "loading chrom = %s\n", chromName);
    loadDatabase(chromName);
    }

carefulClose(&errorFileHandle);

return 0;
}

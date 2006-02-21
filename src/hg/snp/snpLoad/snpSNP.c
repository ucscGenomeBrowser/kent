/* snpSNP - tenth step in dbSNP processing.
 * Read the chrN_snpTmp tables into memory.
 * Do lookups into SNP for validation status and heterozygosity.
 * Use UCSC chromInfo. */

#include "common.h"

#include "chromInfo.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: snpSNP.c,v 1.1 2006/02/21 05:52:17 heather Exp $";

struct snpTmp
    {
    struct snpTmp *next;  	        
    int snp_id;			
    int start;            
    int end;
    int loc_type;
    char *class;
    int orientation;
    char *molType;
    char *fxn_class;
    char *allele;	
    char *refUCSC;
    char *refUCSCReverseComp;
    char *observed;
    };

static char *snpDb = NULL;
static struct hash *chromHash = NULL;
FILE *errorFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpSNP - lookup validation status and heterozygosity in SNP\n"
    "recreate chrN_snpTmp\n"
    "usage:\n"
    "    snpSNP snpDb \n");
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
ret->class = cloneString(row[4]);
ret->orientation = sqlUnsigned(row[5]);
ret->molType = cloneString(row[6]);
ret->fxn_class = cloneString(row[7]);
ret->allele   = cloneString(row[8]);
ret->refUCSC   = cloneString(row[9]);
ret->refUCSCReverseComp   = cloneString(row[10]);
ret->observed   = cloneString(row[11]);

return ret;

}


void snpTmpFree(struct snpTmp **pEl)
/* Free a single dynamically allocated snpTmp such as created with snpTmpLoad(). */
{
struct snpTmp *el;

if ((el = *pEl) == NULL) return;
freeMem(el->class);
freeMem(el->molType);
freeMem(el->fxn_class);
freeMem(el->allele);
freeMem(el->refUCSC);
freeMem(el->refUCSCReverseComp);
freeMem(el->observed);
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
     "select snp_id, chromStart, chromEnd, loc_type, class, orientation, molType, "
     "fxn_class, allele, refUCSC, refUCSCReverseComp, observed from %s ", tableName);

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


void readSNP(char *chromName, struct snpTmp *list)
/* get validation status and heterozygosity from SNP table */
/* expecting a one-to-one correspondence */
/* should check with joinerCheck */
{
struct snpTmp *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char fileName[64];
FILE *f;
int validInt = 0;
boolean gotMatch = FALSE;

safef(fileName, ArraySize(fileName), "%s_snpTmp.tab", chromName);
f = mustOpen(fileName, "w");

for (el = list; el != NULL; el = el->next)
    {
    gotMatch = FALSE;
    safef(query, sizeof(query), "select validation_status, avg_heterozygosity, het_se from SNP where snp_id = %d", el->snp_id);

    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	gotMatch = TRUE;
        validInt = sqlUnsigned(row[0]);
        if (validInt > 31)
            {
            fprintf(errorFileHandle, "unexpected validation_status %s for snp_id %d\n", row[0], el->snp_id);
	    validInt = 0;
	    }
        fprintf(f, "%d\t %d\t %d\t %d\t %s\t %d\t %s\t %s\t %d\t %s\t %s\t %s\t %s\t %s\t %s\t\n", 
            el->snp_id, el->start, el->end, el->loc_type, el->class, el->orientation, el->molType, el->fxn_class,
	    validInt, row[1], row[2],
	    el->allele, el->refUCSC, el->refUCSCReverseComp, el->observed);
	}
    if (!gotMatch)
        fprintf(errorFileHandle, "no match for snp_id %d\n", el->snp_id);

    sqlFreeResult(&sr);
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
"    fxn_class varchar(255) not null,\n"
"    validation_status tinyint(4) not null,\n"
"    avHet float not null, \n"
"    avHetSE float not null, \n"
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

chromHash = loadChroms();
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

errorFileHandle = mustOpen("snpSNP.errors", "w");

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
    if (!hTableExists(tableName)) continue;

    verbose(1, "chrom = %s\n", chromName);

    snpList = readSnps(chromName);
    readSNP(chromName, snpList);
    snpTmpFreeList(&snpList);
    }

carefulClose(&errorFileHandle);

// cookie = hashFirst(chromHash);
// while ((chromName = hashNextName(&cookie)) != NULL)
    // {
    // safef(tableName, ArraySize(tableName), "%s_snpTmp", chromName);
    // if (!hTableExists(tableName)) continue;
    // recreateDatabaseTable(chromName);
    // verbose(1, "loading chrom = %s\n", chromName);
    // loadDatabase(chromName);
    // }


return 0;
}

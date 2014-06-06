/* affyCheck - Check Affy SNPs. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "hdb.h"


char *database = NULL;
char *affyTable = NULL;
char *lookupTable = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "affyCheck - check Affy snps\n"
    "usage:\n"
    "    affyCheck database affyTable lookupTable\n");
}

struct snpSimple
/* Get only what is needed from snp. */
/* Assuming observed string is always in alphabetical order. */
/* Assuming observed string is upper case. */
    {
    struct snpSimple *next;  	        
    char *name;			/* rsId  */
    char *chrom;
    int chromStart;            
    int chromEnd;
    char strand;
    char *observed;	
    };

struct snpSimple *snpSimpleLoad(char **row)
/* Load a snpSimple from row fetched from snp table
 * in database.  Dispose of this with snpSimpleFree().
   Complement observed if negative strand, preserving alphabetical order. */
{
struct snpSimple *ret;
int obsLen, i;
char *obsComp;

AllocVar(ret);
ret->name = cloneString(row[0]);
ret->chrom = cloneString(row[1]);
ret->chromStart = atoi(row[2]);
ret->chromEnd = atoi(row[3]);
// use cloneString rather than strcpy
strcpy(&ret->strand, row[4]);
ret->observed   = cloneString(row[5]);

if (ret->strand == '+') return ret;

obsLen = strlen(ret->observed);
obsComp = needMem(obsLen + 1);
obsComp = cloneString(ret->observed);
for (i = 0; i < obsLen; i = i+2)
    {
    char c = ret->observed[i];
    obsComp[obsLen-i-1] = ntCompTable[c];
    }

verbose(2, "negative strand detected for snp %s\n", ret->name);
verbose(2, "original observed string = %s\n", ret->observed);
verbose(2, "complemented observed string = %s\n", obsComp);

freeMem(ret->observed);
ret->observed=obsComp;
return ret;
}

void snpSimpleFree(struct snpSimple **pEl)
/* Free a single dynamically allocated snpSimple such as created with snpSimpleLoad(). */
{
struct snpSimple *el;

if ((el = *pEl) == NULL) return;
freeMem(el->name);
freeMem(el->observed);
freez(pEl);
}

void snpSimpleFreeList(struct snpSimple **pList)
/* Free a list of dynamically allocated snpSimple's */
{
struct snpSimple *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    snpSimpleFree(&el);
    }
*pList = NULL;
}


struct snpSimple *readSnps()
/* Slurp in the snpSimple rows */
{
struct snpSimple *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;

verbose(1, "reading in from %s...\n", affyTable);
sqlSafef(query, sizeof(query), "select name, chrom, chromStart, chromEnd, strand, observed from %s ", affyTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    count++;
    el = snpSimpleLoad(row);
    slAddHead(&list,el);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
return list;
}



void affyCheck()
/* affyCheck - read in all Affy SNPs, compare to lookupTable. */
{
struct snpSimple *snps = NULL;
struct snpSimple *snp = NULL;
struct snpSimple *newSnp = NULL;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;
int obsLen = 0;

snps = readSnps();

verbose(1, "checking....\n");

for (snp = snps; snp != NULL; snp = snp->next)
    {
    count++;
    verbose(2, "----------------------\n");
    verbose(2, "%d: %s:%d-%d\n", count, snp->chrom, snp->chromStart, snp->chromEnd);
    sqlSafef(query, sizeof(query), "select name, chrom, chromStart, chromEnd, strand, observed from %s"
          " where chrom = '%s' and chromStart = %d and chromEnd = %d", lookupTable, snp->chrom, snp->chromStart, snp->chromEnd);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row == NULL) 
        {
	verbose(2, "no matches for %s %s\n", affyTable, snp->name);
	continue;
	}
    newSnp = snpSimpleLoad(row);
    verbose(2, "comparing %s %s to %s %s\n", affyTable, snp->name, lookupTable, newSnp->name);

    if (sameString(newSnp->observed, "n/a")) 
        {
        while ((row = sqlNextRow(sr)) != NULL) { }
        continue;
	}

    if (sameString(newSnp->observed, "t/n")) 
        {
        while ((row = sqlNextRow(sr)) != NULL) { }
        continue;
	}

    if (sameString(snp->observed, newSnp->observed)) 
        {
        while ((row = sqlNextRow(sr)) != NULL) { }
        continue;
	}

    obsLen = strlen(newSnp->observed);
    if (obsLen > 3)
        {
	verbose(2, "%s is not bi-allelic (%s)\n", newSnp->name, newSnp->observed);
        while ((row = sqlNextRow(sr)) != NULL)
            {
	    }
        continue;
	}

    verbose(1, "----------------------\n");
    verbose(1, "%d: %s:%d-%d\n", count, snp->chrom, snp->chromStart, snp->chromEnd);
    verbose(1, "comparing %s %s to %s %s\n", affyTable, snp->name, lookupTable, newSnp->name);
    verbose(1, "observed difference\n");
    verbose(1, "%s observed = %s, %s observed = %s\n", affyTable, snp->observed, lookupTable, newSnp->observed);

    if (snp->strand == '-' && newSnp->strand == '+')
        {
	verbose(1, "strand difference\n");
	verbose(1, "%s strand = %c, %s strand = %c\n", affyTable, snp->strand, lookupTable, newSnp->strand);
	}
    if (snp->strand == '+' && newSnp->strand == '-')
        {
	verbose(1, "strand difference\n");
	verbose(1, "%s strand = %c, %s strand = %c\n", affyTable, snp->strand, lookupTable, newSnp->strand);
	}
    /* check here for multiple matches */
    while ((row = sqlNextRow(sr)) != NULL)
        {
	}
    /* free newSnp */
    }

snpSimpleFreeList(&snps);

}


int main(int argc, char *argv[])
/* Check args and call affyCheck. */
{
if (argc != 4)
    usage();
database = argv[1];
affyTable = argv[2];
lookupTable = argv[3];
hSetDb(database);

/* initialize ntCompTable[] */
dnaUtilOpen();
affyCheck();
return 0;
}

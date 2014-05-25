/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


/* snpMapToExon - Find single-base SNPs associated with exons. */
#include "common.h"
#include "binRange.h"
#include "hdb.h"


char *database = NULL;
char *geneTable = NULL;
char *snpTable = NULL;
char *outputFile = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "snpMapToExon - find single-base SNPs inside exons.\n"
    "usage:\n"
    "    snpMapToExon database snpTable geneTable outputFile\n");
}



// change to using genePredFreeList in genePred.h
void geneFreeList(struct genePred **gList)
/* Free a list of dynamically allocated genePred's */
{
struct genePred *el, *next;

for (el = *gList; el != NULL; el = next)
    {
    next = el->next;
    genePredFree(&el);
    }
*gList = NULL;
}

struct binKeeper *readSnps(char *chrom)
/* read snps for chrom into a binKeeper */
{
int chromSize = hChromSize(chrom);
struct binKeeper *ret = binKeeperNew(0, chromSize);
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *name = NULL;
int start = 0;
int end = 0;
int count = 0;
char *class = NULL;
char *locType = NULL;

sqlSafef(query, sizeof(query), "select name, chromStart, chromEnd, class, locType from %s where chrom='%s' ", snpTable, chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    class = cloneString(row[3]);
    if (!sameString(class, "snp")) continue;
    locType = cloneString(row[4]);
    if (!sameString(locType, "exact")) continue;
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    if (end != start + 1) continue;
    count++;
    name = cloneString(row[0]);
    binKeeperAdd(ret, start, end, name);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
verbose(1, "Count of snps found = %d\n", count);
return ret;
}


struct genePred *readGenes(char *chrom)
/* Slurp in the genes for one chrom */
{
struct genePred *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;

sqlSafef(query, sizeof(query), "select * from %s where chrom='%s' ", geneTable, chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = genePredLoad(row);
    slAddHead(&list,el);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
verbose(1, "Count of genes found = %d\n", count);
return list;
}


void snpMapToExon(char *chromName)
{
struct genePred *genes = NULL, *gene = NULL;
int exonPos = 0, exonStart = 0, exonEnd = 0, exonSize = 0;
struct binKeeper *snps = NULL;
struct binElement *el, *elList = NULL;
char fileName[64];
// for short circuit
// int geneCount = 0;
FILE *fileHandle = NULL;

safef(fileName, sizeof(fileName), "%s.%s", outputFile, chromName);
fileHandle = mustOpen(fileName, "w");

snps = readSnps(chromName);
genes = readGenes(chromName);

for (gene = genes; gene != NULL; gene = gene->next)
    {
    // short circuit goes here
    // if (geneCount == 10) return;
    // geneCount++;

    for (exonPos = 0; exonPos < gene->exonCount; exonPos++)
        {
        exonStart = gene->exonStarts[exonPos];
        exonEnd   = gene->exonEnds[exonPos];
        exonSize = exonEnd - exonStart;
        assert (exonSize > 0);

	elList = binKeeperFind(snps, exonStart, exonEnd);
	for (el = elList; el != NULL; el = el->next)
	    fprintf(fileHandle, "%s\t%s\t%d\t%d\t%s\n", chromName, gene->name, exonStart, exonEnd, (char *)el->val);
        }

    }

// verbose(1, "freeing genelist\n");
geneFreeList(&genes);
carefulClose(&fileHandle);
}


int main(int argc, char *argv[])
/* Check args and call snpMapToExon. */
{
struct slName *chromList, *chromPtr;
if (argc != 5)
    usage();
database = argv[1];
if(!hDbExists(database))
    errAbort("%s does not exist\n", database);
hSetDb(database);
snpTable = argv[2];
if(!hTableExistsDb(database, snpTable))
    errAbort("no %s in %s\n", snpTable, database);
geneTable = argv[3];
outputFile = argv[4];
chromList = hAllChromNamesDb(database);
for (chromPtr = chromList; chromPtr != NULL; chromPtr = chromPtr->next)
    {
    verbose(1, "chrom = %s\n", chromPtr->name);
    snpMapToExon(chromPtr->name);
    verbose(1, "---------------\n");
    }
return 0;
}


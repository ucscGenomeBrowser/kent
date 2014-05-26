/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


/* fixedDiffs - look for bases that are different between human and neanderthal, and not a known SNP in human. */
#include "common.h"
#include "binRange.h"
#include "hdb.h"


char *database = NULL;
char *snpTable = NULL;
char *chainTable = NULL;
char *humanNib = NULL;
char *neanderNib = NULL;

struct myChainInfo
/* for storing data from chainHomNea0 */
    {
    struct myChainInfo *next;
    char *tName;
    int tStart;
    int tEnd;
    char *qName;
    int qStart;
    int qEnd;
    char qStrand;
    int id;
    };


void usage()
/* Explain usage and exit. */
{
errAbort(
    "fixedDiffs - look for bases that are different between human and neanderthal, and not a known SNP in human.\n"
    "usage:\n"
    "    fixedDiffs database chainTable snpTable humanNib neanderthalNib\n");
}


struct myChainInfo *chainLoad(char **row)
{
struct myChainInfo *ret;

AllocVar(ret);
ret->tName = cloneString(row[0]);
ret->tStart = sqlUnsigned(row[1]);
ret->tEnd = sqlUnsigned(row[2]);
ret->qName = cloneString(row[3]);
ret->qStart = sqlUnsigned(row[4]);
ret->qEnd = sqlUnsigned(row[5]);
strcpy(&ret->qStrand, row[6]);
ret->id = sqlUnsigned(row[7]);
return ret;
}

struct binKeeper *readSnps(char *chrom, int start, int end)
/* read simple snps from a human chain into a binKeeper */
/* can the binKeeper be smaller than chromSize? */
{
int chromSize = hChromSize(chrom);
struct binKeeper *ret = binKeeperNew(0, chromSize);
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char *name = NULL;
int chromStart = 0;
int chromEnd = 0;
int count = 0;
char *class = NULL;
char *locType = NULL;

sqlSafef(query, sizeof(query), 
    "select name, chromStart, chromEnd, class, locType from %s where chrom='%s' and chromStart >= %d and chromEnd <= %d", 
    snpTable, chrom, start, end);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    class = cloneString(row[3]);
    if (!sameString(class, "single")) continue;
    locType = cloneString(row[4]);
    if (!sameString(locType, "exact")) continue;
    chromStart = sqlUnsigned(row[1]);
    chromEnd = sqlUnsigned(row[2]);
    if (chromEnd != chromStart + 1) continue;
    count++;
    name = cloneString(row[0]);
    binKeeperAdd(ret, chromStart, chromEnd, name);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printf("Count of snps found = %d\n", count);
return ret;
}


struct myChainInfo *readChains()
/* Slurp in the chains */
{
struct myChainInfo *list=NULL, *el;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int count = 0;

sqlSafef(query, sizeof(query), 
    "select tName, tStart, tEnd, qName, qStart, qEnd, qStrand, id from %s", chainTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = chainLoad(row);
    slAddHead(&list,el);
    count++;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);  /* could possibly skip if it made much difference in speed. */
printf("Count of chains found = %d\n", count);
return list;
}


void fixedDiffs()
{
struct myChainInfo *chains = NULL, *chain = NULL;
struct binKeeper *snps = NULL;
struct binElement *el = NULL;
struct dnaSeq *humanSeq = NULL;
struct dnaSeq *neanderSeq = NULL;
char *humanPtr = NULL;
char *neanderPtr = NULL;
int pos = 0;
int size = 0;
int count = 0;

chains = readChains();

for (chain = chains; chain != NULL; chain = chain->next)
    {
    pos = 0;
    count++;
    /* get snps */
    printf("chain id=%d human=%s:%d-%d\n", chain->id, chain->tName, chain->tStart, chain->tEnd);
    snps = readSnps(chain->tName, chain->tStart, chain->tEnd);
    /* get sequences */
    /* check that sequences are the same size? */
    humanSeq = hFetchSeq(humanNib, chain->tName, chain->tStart, chain->tEnd);
    if (chain->qStrand == '-')
        reverseComplement(humanSeq->dna, humanSeq->size);
    humanPtr = humanSeq->dna;
    printf("human DNA =   %s\n", humanPtr);
    neanderSeq = hFetchSeq(neanderNib, chain->qName, chain->qStart, chain->qEnd);
    neanderPtr = neanderSeq->dna;
    printf("neander DNA = %s\n", neanderPtr);

    /* read through */
    size = min(chain->tEnd - chain->tStart, chain->qEnd - chain->qStart);
    while (pos < size)
        {
        if (humanPtr[pos] == neanderPtr[pos])
	    {
	    pos++;
	    continue;
	    }
        el = binKeeperFind(snps, chain->tStart + pos, chain->tStart + pos + 1);
	if (el)
	    {
	    pos++;
	    continue;
	    }
        printf("FIXED DIFF chain %d: human %s:%d, neanderthal %s:%d ", chain->id, chain->tName, chain->tStart + pos, 
                   chain->qName, chain->qStart + pos);
        printf("snp: %c/%c\n", humanPtr[pos], neanderPtr[pos]);
	pos++;
	}
	printf("----------------------------------\n");
    /* free binKeeper */
    }
}


int main(int argc, char *argv[])
/* Check args and call fixedDiffs. */
{
if (argc != 6)
    usage();

database = argv[1];
if(!hDbExists(database))
    errAbort("%s does not exist\n", database);
hSetDb(database);

chainTable = argv[2];
if(!hTableExistsDb(database, chainTable))
    errAbort("no %s in %s\n", chainTable, database);
snpTable = argv[3];
if(!hTableExistsDb(database, snpTable))
    errAbort("no %s in %s\n", snpTable, database);

humanNib = argv[4];
if (!fileExists(humanNib))
    errAbort("Can't find file %s\n", humanNib);

neanderNib = argv[5];
if (!fileExists(neanderNib))
    errAbort("Can't find file %s\n", neanderNib);

fixedDiffs();

return 0;
}


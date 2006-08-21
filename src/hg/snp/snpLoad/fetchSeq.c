/* fetchSeq -- get sequences for ortho pipeline. */
/* Write to chrN_snp126hg18orthoAllele.tab. */

#include "common.h"

#include "chromInfo.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: fetchSeq.c,v 1.4 2006/08/21 22:44:42 heather Exp $";

static char *snpDb = NULL;
static struct hash *chromHash = NULL;
FILE *errorFileHandle = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "fetchSeq - get sequences for ortho pipeline\n"
    "usage:\n"
    "    fetchSeq database sequence\n");
}


struct hash *loadChroms()
/* hash from UCSC chromInfo */
{
struct hash *ret;
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct chromInfo *el;
char tableName[64];

ret = newHash(0);
safef(query, sizeof(query), "select chrom, size from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    safef(tableName, ArraySize(tableName), "%s_snp126hg18ortho", row[0]);
    if (!hTableExists(tableName)) continue;
    el = chromInfoLoad(row);
    hashAdd(ret, el->chrom, (void *)(& el->size));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
return ret;
}

unsigned getChromSize(char *chrom)
/* Return size of chrom.  */
{
struct hashEl *el = hashLookup(chromHash,chrom);

if (el == NULL)
    errAbort("Couldn't find size of chrom %s", chrom);
return *(unsigned *)el->val;
}


void doFetch(char *sequenceFile, char *chromName)
/* get sequence for each row */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char tableName[64];
char fileName[64];
FILE *f;
struct dnaSeq *seq;
char *seqPtr = NULL;
int start = 0;
int end = 0;
int bin = 0;
int chromSize = 0;
char allele[2];
int count = 0;
char *snpId = NULL;

safef(tableName, sizeof(tableName), "%s_snp126hg18ortho", chromName);
if (!hTableExists(tableName)) return;
safef(fileName, ArraySize(fileName), "%s_snp126hg18orthoAllele.tab", chromName);
chromSize = getChromSize(chromName);

f = mustOpen(fileName, "w");
seq = hFetchSeq(sequenceFile, chromName, 0, chromSize-1);
touppers(seq->dna);
seqPtr = seq->dna;

// not getting humanAllele or humanObserved
safef(query, sizeof(query), "select chromStart, chromEnd, name, strand from %s", tableName);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    // short-circuit
    // count++;
    // if (count == 10) return;
    start = sqlUnsigned(row[0]);
    end = sqlUnsigned(row[1]);
    snpId = cloneString(row[2]);
    if (end != start + 1)
	{
        fprintf(errorFileHandle, "rs%s has size %d\n", row[0], end - start);
        continue;
	}
    if (start > chromSize)
        {
        fprintf(errorFileHandle, "rs%s at %s:%d-%d; chromSize=%d\n", row[0], chromName, start, end, chromSize);
	continue;
	}

    bin = hFindBin(start, end);
    safef(allele, sizeof(allele), "%c", seqPtr[start]);
    if (sameString(row[3], "-"))
        reverseComplement(allele, 1);
    fprintf(f, "%d\t%s\t%d\t%d\t%s\t%s\t%s\t%s\n", 
        bin, chromName, start, end, snpId, snpDb, row[3], allele);

    }
sqlFreeResult(&sr);
hFreeConn(&conn);
sqlDisconnect(&conn);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* read chrN_snp126hg18ortho, look up sequence */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;
char tableName[64];

if (argc != 3)
    usage();

snpDb = argv[1];
hSetDb(snpDb);

chromHash = loadChroms();
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

errorFileHandle = mustOpen("fetchSeq.errors", "w");

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    safef(tableName, ArraySize(tableName), "%s_snp126hg18ortho", chromName);
    if (!hTableExists(tableName)) continue;
    verbose(1, "chrom = %s\n", chromName);
    doFetch(argv[2], chromName);
    }

carefulClose(&errorFileHandle);
return 0;
}

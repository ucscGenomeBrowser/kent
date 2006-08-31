/* seqWithInsertions -- for each chrom, generate "fat" sequence: that is, include insertions. */
/* sequence file is separate input -- could also get from chromInfo */
/* write to chrom.fat */

#include "common.h"

#include "chromInfo.h"
#include "hash.h"
#include "hdb.h"

static char const rcsid[] = "$Id: seqWithInsertions.c,v 1.3 2006/08/31 00:42:59 heather Exp $";

static char *database = NULL;
static char *snpTable = NULL;
static struct hash *chromHash = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
    "seqWithInsertions - generate sequence that includes insertions\n"
    "usage:\n"
    "    seqWithInsertions database sequenceFile snpTable\n");
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

ret = newHash(0);
safef(query, sizeof(query), "select chrom, size from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
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

char *getSubstring(int startPos, int endPos, char *sequence)
/* I'm hoping this is a safe and fast strncat */
{
char *newSequence = NULL;
int size = endPos - startPos + 1;
int pos = 0;

assert (size > 0);
verbose(5, "getSubstring from %d to %d\n", startPos, endPos);
newSequence = needMem(size + 1);
for (pos = 0; pos <= size - 1; pos++)
    newSequence[pos] = sequence[pos+startPos];
newSequence[size] = '\0';
verbose(5, "substring = %s\n", newSequence);
return newSequence;
}


void getSeq(char *sequenceFile, char *chromName)
/* get sequence for each chrom */
/* assumes insertions are sorted by position -- errAbort if not true */
{
char query[512];
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;

char fileName[64];
FILE *f;

struct dnaSeq *seq;
char *leftFlank = NULL;
char *rightFlank = NULL;

int start = 0;
int end = 0;
int chromSize = 0;

char *snpChrom = NULL;
char *strand = NULL;
char *snpClass = NULL;
char *locType = NULL;
char *observed = NULL;
char *rsId = NULL;

char *subString = NULL;

int slashCount = 0;
int seqPos = 0;
int snpCount = 0;

chromSize = getChromSize(chromName);
verbose(1, "chromSize = %d\n", chromSize);
seq = hFetchSeq(sequenceFile, chromName, 0, chromSize);
touppers(seq->dna);

safef(fileName, ArraySize(fileName), "%s.fat", chromName);
f = mustOpen(fileName, "w");

safef(query, sizeof(query), "select chrom, chromStart, chromEnd, strand, class, locType, observed, name from %s", snpTable);

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    snpChrom = cloneString(row[0]);
    start = sqlUnsigned(row[1]);
    end = sqlUnsigned(row[2]);
    strand = cloneString(row[3]);
    snpClass = cloneString(row[4]);
    locType = cloneString(row[5]);
    observed = cloneString(row[6]);
    rsId = cloneString(row[7]);

    /* input filters */
    if (!sameString(snpClass, "insertion")) continue;
    if (!sameString(snpChrom, chromName)) continue;
    if (end != start) continue;
    if (start > chromSize) continue;
    if (strlen(observed) < 2) continue;
    /* First char should be dash, second char should be forward slash. */
    if (observed[0] != '-') continue;
    if (observed[1] != '/') continue;
    /* Only one forward slash. */
    int slashCount = chopString(observed, "/", NULL, 0);
    if (slashCount > 2) continue;
    /* skip clustering errors */
    if (start == seqPos) continue;
    if (start < seqPos)
        errAbort("candidate SNPs are not in sorted order.\n");

    snpCount++;

    subString = cloneString(observed);
    subString = subString + 2;
    if (sameString(strand, "-"))
        reverseComplement(subString, strlen(subString));

    /* add the left flank */
    leftFlank = getSubstring(seqPos, start-1, seq->dna);
    writeSeqWithBreaks(f, leftFlank, strlen(leftFlank), 50);

    /* add the observed string */
    writeSeqWithBreaks(f, subString, strlen(subString), 50);

    /* increment the position */
    seqPos = start;
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);

if (snpCount == 0)
   {
   verbose(1, "no matching SNPs\n");
   return;
   }

/* add the final (right) flank */
rightFlank = getSubstring(seqPos, chromSize + 1, seq->dna);
writeSeqWithBreaks(f, rightFlank, strlen(rightFlank), 50);

carefulClose(&f);

}


int main(int argc, char *argv[])
/* read snpTable, generate fat sequence for each chrom */
{
struct hashCookie cookie;
struct hashEl *hel;
char *chromName;

if (argc != 4)
    usage();

database = argv[1];
hSetDb(database);

snpTable = argv[3];
if (!hTableExists(snpTable)) 
    errAbort("no %s table\n", snpTable);

if (!hTableExists("chromInfo")) 
    errAbort("no chromInfo table\n");
chromHash = loadChroms();
if (chromHash == NULL) 
    {
    verbose(1, "couldn't get chrom info\n");
    return 1;
    }

cookie = hashFirst(chromHash);
while ((chromName = hashNextName(&cookie)) != NULL)
    {
    verbose(1, "chrom = %s\n", chromName);
    getSeq(argv[2], chromName);
    }

return 0;
}

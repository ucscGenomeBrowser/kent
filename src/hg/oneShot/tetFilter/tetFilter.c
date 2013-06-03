/* tetFilter - Makes a SQL file that removes tet alignments to simple repeats. */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "rmskOut.h"
#include "chromInfo.h"
#include "bits.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "tetFilter - Makes a SQL file that removes tet alignments to simple repeats\n"
  "usage:\n"
  "   tetFilter database output.sql\n");
}

struct wabaChromHit
/* Records where waba alignment hits chromosome. */
    {
    struct wabaChromHit *next;	/* Next in list. */
    char *query;	  	/* Query name. */
    int chromStart, chromEnd;   /* Chromosome position. */
    char strand;                /* + or - for strand. */
    int milliScore;             /* Parts per thousand */
    char *squeezedSym;          /* HMM Symbols */
    };

struct wabaChromHit *wchLoad(char *row[])
/* Create a wabaChromHit from database row. 
 * Since squeezedSym autoSql can't generate this,
 * alas. */
{
int size;
char *sym;
struct wabaChromHit *wch;

AllocVar(wch);
wch->query = cloneString(row[0]);
wch->chromStart = sqlUnsigned(row[1]);
wch->chromEnd = sqlUnsigned(row[2]);
wch->strand = row[3][0];
wch->milliScore = sqlUnsigned(row[4]);
size = wch->chromEnd - wch->chromStart;
wch->squeezedSym = sym = needLargeMem(size+1);
memcpy(sym, row[5], size);
sym[size] = 0;
return wch;
}


void wchFree(struct wabaChromHit **pWch)
/* Free a singlc wabaChromHit. */
{
struct wabaChromHit *wch = *pWch;
if (wch != NULL)
    {
    freeMem(wch->squeezedSym);
    freeMem(wch->query);
    freez(pWch);
    }
}

void wchFreeList(struct wabaChromHit **pList)
/* Free list of wabaChromHits. */
{
struct wabaChromHit *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wchFree(&el);
    }
*pList = NULL;
}

struct chromInfo *readChroms(struct hash *chromHash, struct sqlConnection *conn)
/* Return chromosomes in list/hash. */
{
struct chromInfo *chrom, *chromList = NULL;
char query[512];
char **row;
struct sqlResult *sr;

sqlSafef(query, sizeof query, "select * from chromInfo");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    chrom = chromInfoLoad(row);
    hashAddUnique(chromHash, chrom->chrom, chrom);
    slAddHead(&chromList, chrom);
    }
sqlFreeResult(&sr);
slReverse(&chromList);
return chromList;
}

struct wabaChromHit *wchLoadAll(struct sqlConnection *conn, char *chromName)
/* Load all waba chromosome hits from one chromosome. */
{
char query[512];
char **row;
struct sqlResult *sr;
char table[128];
struct wabaChromHit *wchList = NULL, *wch;

sprintf(table, "%s_tet_waba", chromName);
if (!sqlTableExists(conn, table))
    return NULL;
sqlSafef(query, sizeof query, "select * from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    wch = wchLoad(row);
    slAddHead(&wchList, wch);
    }
sqlFreeResult(&sr);
slReverse(&wchList);
return wchList;
}

Bits *getMaskedBits(struct sqlConnection *conn, struct chromInfo *chrom)
/* Get bit array with parts that are masked by simple repeats etc. masked
 * out. */
{
char query[512];
char **row;
struct sqlResult *sr;
char table[128];
struct wabaChromHit *wchList = NULL, *wch;
struct rmskOut ro;
Bits *b = bitAlloc(chrom->size);
int allCount = 0;
int simpCount = 0;

sqlSafef(query, sizeof query, "select * from %s_rmsk",
    chrom->chrom);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ++allCount;
    rmskOutStaticLoad(row, &ro);
    if (sameString(ro.repClass, "Simple_repeat") || sameString(ro.repClass, "Low_complexity"))
	{
	++simpCount;
	assert(ro.genoEnd <= chrom->size);
	bitSetRange(b, ro.genoStart, ro.genoEnd - ro.genoStart);
	}
    }
printf("Got %d repeats, %d simple/low complexity\n", allCount, simpCount);
sqlFreeResult(&sr);
return b;
}

void makeDelete(char *chrom, struct wabaChromHit *wch, FILE *f)
/* Generate SQL to kill alignment in chrN_tet_waba and
 * overall waba_tet table. */
{
fprintf(f, "delete from waba_tet where chrom = '%s' and chromStart = %d and chromEnd = %d and query = '%s';\n",
	chrom, wch->chromStart, wch->chromEnd, wch->query);
fprintf(f, "delete from %s_tet_waba where chromStart = %d and chromEnd = %d and query = '%s';\n",
	chrom, wch->chromStart, wch->chromEnd, wch->query);
}

void makeDeletes(struct sqlConnection *conn, struct chromInfo *chrom, FILE *f)
/* Generate SQL that kills tet alignments on simple repeats. */
{
struct wabaChromHit *wchList = NULL, *wch;
struct rmskOut ro;
int tetSize;
int repSize;
int start, end;
int delCount = 0;
int totCount = 0;
Bits *b = NULL;

printf("  Loading all tet alignments on %s...\n", chrom->chrom);
wchList = wchLoadAll(conn, chrom->chrom);
printf("  Got %d alignments\n", slCount(wchList));
b = getMaskedBits(conn, chrom);
for (wch = wchList; wch != NULL; wch = wch->next)
    {
    tetSize = wch->chromEnd - wch->chromStart;
    repSize = bitCountRange(b, wch->chromStart, tetSize);
    ++totCount;
    if (repSize * 2 > tetSize)
	{
	++delCount;
	makeDelete(chrom->chrom, wch, f);
	}
    }
bitFree(&b);
if (totCount > 0)
    printf("Deleted %d of %d (%4.2f%%)\n", delCount, totCount, (100.0)*delCount/(double)totCount);
wchFreeList(&wchList);
}

void tetFilter(char *database, char *sqlOut)
/* tetFilter - Makes a SQL file that removes tet alignments to simple repeats. */
{
struct sqlConnection *conn = sqlConnect(database);
struct chromInfo *chromList, *chrom;
struct hash *chromHash = newHash(7);
FILE *f = mustOpen(sqlOut, "w");

chromList = readChroms(chromHash, conn);
printf("Read %d 'chromosomes'\n", slCount(chromList));

for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    printf("Processing %s\n", chrom->chrom);
    makeDeletes(conn, chrom, f);
    }
sqlDisconnect(&conn);
fclose(f);
}
int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
tetFilter(argv[1], argv[2]);
return 0;
}

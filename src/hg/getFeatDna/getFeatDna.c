/* getFeatDna - Get dna for a type of feature. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "dnaseq.h"
#include "fa.h"
#include "dystring.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"

/* Variables set explicitly or optionally from command line. */
char *database = NULL;		/* Which database to use. */
char *table = NULL;		/* Which feature table to use. */
char *outName = NULL;		/* Output file name. */
char *chrom = NULL;		/* Chromosome to restrict to. */
int start = 0;			/* Start of range to select from. */
int end = BIGNUM;               /* End of range. */
char *where = NULL;		/* Extra selection info. */
boolean breakUp = FALSE;	/* Break up things? */
int merge = 0;			/* Merge close blocks? */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getFeatDna - Get dna for a type of feature\n"
  "usage:\n"
  "   getFeatDna database table chrom output.fa\n"
  "options:\n"
  "   -start=NNN   restrict to features that start after NNN\n"
  "   -end=NNN     restrict to features that end after NNN\n"
  "   '-where=some sql pattern'  restrict to features matching some sql pattern\n"
  "   -breakUp     break up alignments into pieces\n"
  "   -merge=N     merge blocks separated by N or less\n"
  );
}

static boolean fitFields(struct hash *hash, char *chrom, char *start, char *end,
	char retChrom[32], char retStart[32], char retEnd[32])
/* Return TRUE if chrom/start/end are in hash.  
 * If so copy them to retChrom, retStart, retEnd. 
 * Helper routine for findChromStartEndFields below. */
{
if (hashLookup(hash, chrom) && hashLookup(hash, start) && hashLookup(hash, end))
    {
    strcpy(retChrom, chrom);
    strcpy(retStart, start);
    strcpy(retEnd, end);
    return TRUE;
    }
else
    return FALSE;
}

void findChromStartEndFields(struct sqlConnection *conn, char *table, 
	char retChrom[32], char retStart[32], char retEnd[32])
/* Given a table return the fields for selecting chromosome, start, and end. */
{
char query[256];
struct sqlResult *sr;
char **row;
struct hash *hash = newHash(5);

/* Read table description into hash. */
sprintf(query, "describe %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(hash, row[0], NULL);
    }
sqlFreeResult(&sr);

/* Look for bed-style names. */
if (fitFields(hash, "chrom", "chromStart", "chromEnd", retChrom, retStart, retEnd))
    ;
/* Look for psl-style names. */
else if (fitFields(hash, "tName", "tStart", "tEnd", retChrom, retStart, retEnd))
    ;
/* Look for gene prediction names. */
else if (fitFields(hash, "chrom", "txStart", "txEnd", retChrom, retStart, retEnd))
    ;
/* Look for repeatMasker names. */
else if (fitFields(hash, "genoName", "genoStart", "genoEnd", retChrom, retStart, retEnd))
    ;
else
    errAbort("Couldn't find chrom/start/end fields in table %s", table);
freeHash(&hash);
}

void getFeatDna()
/* getFeatDna - Get dna for a type of feature. */
{
/* Get chromosome in lower case case.  Set bits that are part of
 * feature of interest to upper case, and then scan for blocks
 * of lower upper case and output them. */
struct dyString *query = newDyString(512);
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char chromField[32], startField[32], endField[32];
struct dnaSeq *seq;
DNA *dna;
int s,e,sz,i,size;
int blockIx = 0;
char faName[64], faStartLine[256];
boolean lastIsUpper = FALSE, isUpper = FALSE;
FILE *f = mustOpen(outName, "w");
int last;

findChromStartEndFields(conn, table, chromField, startField, endField);

seq = hLoadChrom(chrom);
dna = seq->dna;
size = seq->size;

if (breakUp)
    {
    if (!sameString(startField, "tStart") )
        errAbort("Can only use breakUp parameter with psl formatted tables");
    dyStringPrintf(query, "select * from %s where tStart >= %d and tEnd < %d",
    	table, start, end);
    dyStringPrintf(query, " and %s = '%s'", chromField, chrom);
    if (where != NULL)
	dyStringPrintf(query, " and %s", where);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	struct psl *psl = pslLoad(row);
	int i;
	if (psl->strand[1] == '-')	/* Minus strand on target */
	    {
	    int tSize = psl->tSize;
	    for (i=0; i<psl->blockCount; ++i)
		 {
		 sz = psl->blockSizes[i];
		 s = tSize - (psl->tStarts[i] + sz);
		 toUpperN(seq->dna + s, sz);
		 }
	    }
	else
	    {
	    for (i=0; i<psl->blockCount; ++i)
		 toUpperN(seq->dna + psl->tStarts[i], psl->blockSizes[i]);
	    }
	pslFree(&psl);
	}
    }
else
    {
    dyStringPrintf(query, "select %s,%s from %s where %s >= %d and %s < %d", 
	    startField, endField, table,
	    startField, start, endField, end);
    dyStringPrintf(query, " and %s = '%s'", chromField, chrom);
    if (where != NULL)
	dyStringPrintf(query, " and %s", where);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	s = sqlUnsigned(row[0]);
	e = sqlUnsigned(row[1]);
	sz = e - s;
	if (sz < 0 || e >= size)
	    errAbort("Coordinates out of range %d %d (%s size is %d)", s, e, chrom, size);
	toUpperN(seq->dna + s, sz);
	}
    }

/* Merge nearby upper case blocks. */
e = -merge-10;		/* Don't merge initial block. */
if (merge != 0)
    {
    for (i=0; i<=size; ++i)
	{
	isUpper = isupper(dna[i]);
	if (isUpper && !lastIsUpper)
	    {
	    s = i;
	    if (s - e <= merge)
	        {
		toUpperN(dna+e, s-e);
		}
	    }
	else if (!isUpper && lastIsUpper)
	    {
	    e = i;
	    }
	lastIsUpper = isUpper;
	}
    }

for (i=0; i<=size; ++i)
    {
    isUpper = isupper(dna[i]);
    if (isUpper && !lastIsUpper)
        s = i;
    else if (!isUpper && lastIsUpper)
        {
	e = i;
	++blockIx;
	if (startsWith("chr", table))
	    sprintf(faName, "%s_%d", table, blockIx);
	else
	    sprintf(faName, "%s_%s_%d", chrom, table, blockIx);
	sprintf(faStartLine, "%s %s %s %d %d", 
		faName, database, chrom, s, e);
	faWriteNext(f, faStartLine, dna+s, e-s);
	}
    lastIsUpper = isUpper;
    }
hFreeConn(&conn);
if (!sameString(outName, "stdout"))
    printf("%d features extracted to %s\n", blockIx, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char chrTable[256];

cgiSpoof(&argc, argv);
if (argc != 5)
    usage();
database = argv[1];
table = argv[2];
chrom = argv[3];
outName = argv[4];
start = cgiUsualInt("start", start);
end = cgiUsualInt("end", end);
where = cgiOptionalString("where");
breakUp = cgiBoolean("breakUp");
merge = cgiUsualInt("merge", merge);
hSetDb(database);
if (!hTableExists(table))
    {
    sprintf(chrTable, "%s_%s", chrom, table);
    if (!hTableExists(table))
        errAbort("table %s (and %s) don't exist in %s", table, chrTable, database);
    table = chrTable;
    }
getFeatDna();
return 0;
}

/* getFeatDna - Get dna for a type of feature. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "genePred.h"
#include "dnaseq.h"
#include "fa.h"
#include "dystring.h"
#include "jksql.h"
#include "hdb.h"
#include "psl.h"
#include "nib.h"

/* Variables set explicitly or optionally from command line. */
char *database = NULL;		/* Which database? */
int chromStart = 0;		/* Start of range to select from. */
int chromEnd = BIGNUM;          /* End of range. */
char *where = NULL;		/* Extra selection info. */
boolean breakUp = FALSE;	/* Break up things? */
int merge = -1;			/* Merge close blocks? */
char *outputType = "fasta";	/* Type of output. */

static int blockIx = 0;	/* Index of block written. */

char *makeFaStartLine(char *chrom, char *table, int start, int size)
/* Create fa start line. */
{
char faName[128];
static char faStartLine[512];

++blockIx;
if (startsWith("chr", table))
    sprintf(faName, "%s_%d", table, blockIx);
else
    sprintf(faName, "%s_%s_%d", chrom, table, blockIx);
sprintf(faStartLine, "%s %s %s %d %d", 
	faName, database, chrom, start, start+size);
return faStartLine;
}

void writeOut(FILE *f, char *chrom, int start, int size, char strand, DNA *dna, char *faHeader)
/* Write output to file */
{
if (sameWord(outputType, "pos"))
    fprintf(f, "%s\t%d\t%d\t%c\n", chrom, start, start+size, strand);
else if (sameWord(outputType, "fasta"))
    faWriteNext(f, faHeader, dna, size);
else
    errAbort("Unknown output type %s\n", outputType);
}

void outputDna(FILE *f, char *chrom, char *table, int start, int size, char *dna, 
	char *nibFileName, FILE *nibFile, int nibSize, char strand)
/* Output DNA either directly from nib or by upper-casing DNA. */
{
if (merge >= 0)
    toUpperN(dna + start, size);
else
    {
    struct dnaSeq *seq = nibLdPart(nibFileName, nibFile, nibSize, start, size);
    if (strand == '-')
        reverseComplement(seq->dna, size);
    writeOut(f, chrom, start, size, strand, seq->dna, makeFaStartLine(chrom, table, start, size));
    freeDnaSeq(&seq);
    }
}

void chromFeatDna(char *table, char *chrom, FILE *f)
/* chromFeatDna - Get dna for a type of feature on one chromosome. */
{
/* Get chromosome in lower case case.  If merging set bits that 
 * are part of feature of interest to upper case, and then scan 
 * for blocks of lower upper case and output them. If not merging
 * then just output dna for features directly. */
struct dyString *query = newDyString(512);
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
char chromField[32], startField[32], endField[32];
struct dnaSeq *seq = NULL;
DNA *dna = NULL;
int s,e,sz,i,size;
boolean lastIsUpper = FALSE, isUpper = FALSE;
int last;
char nibFileName[512];
FILE *nibFile = NULL;
int nibSize;

if (!hFindChromStartEndFields(table, chromField, startField, endField))
    errAbort("Couldn't find chrom/start/end fields in table %s", table);

if (merge >= 0)
    {
    seq = hLoadChrom(chrom);
    dna = seq->dna;
    size = seq->size;
    }
else
    {
    hNibForChrom(chrom, nibFileName);
    nibOpenVerify(nibFileName, &nibFile, &nibSize);
    }

if (breakUp)
    {
    if (sameString(startField, "tStart"))
	{
	dyStringPrintf(query, "select * from %s where tStart >= %d and tEnd < %d",
	    table, chromStart, chromEnd);
	dyStringPrintf(query, " and %s = '%s'", chromField, chrom);
	if (where != NULL)
	    dyStringPrintf(query, " and %s", where);
	sr = sqlGetResult(conn, query->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    struct psl *psl = pslLoad(row);
	    if (psl->strand[1] == '-')	/* Minus strand on target */
		{
		int tSize = psl->tSize;
		for (i=0; i<psl->blockCount; ++i)
		     {
		     sz = psl->blockSizes[i];
		     s = tSize - (psl->tStarts[i] + sz);
		     outputDna(f, chrom, table, s, sz, dna, nibFileName, nibFile, nibSize, '-');
		     }
		}
	    else
		{
		for (i=0; i<psl->blockCount; ++i)
		     outputDna(f, chrom, table, psl->tStarts[i], psl->blockSizes[i], 
			    dna, nibFileName, nibFile, nibSize, '+');
		}
	    pslFree(&psl);
	    }
	}
    else if (sameString(startField, "txStart"))
        {
	dyStringPrintf(query, "select * from %s where txStart >= %d and txEnd < %d",
	    table, chromStart, chromEnd);
	dyStringPrintf(query, " and %s = '%s'", chromField, chrom);
	if (where != NULL)
	    dyStringPrintf(query, " and %s", where);
	sr = sqlGetResult(conn, query->string);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    struct genePred *gp = genePredLoad(row);
	    for (i=0; i<gp->exonCount; ++i)
		 {
		 s = gp->exonStarts[i];
		 sz = gp->exonEnds[i] - s;
		 outputDna(f, chrom, table, 
		    s, sz, dna, nibFileName, nibFile, nibSize, gp->strand[0]);
		 }
	    genePredFree(&gp);
	    }
	}
    else
        {
        errAbort("Can only use breakUp parameter with psl or genePred formatted tables");
	}
    }
else
    {
    dyStringPrintf(query, "select %s,%s from %s where %s >= %d and %s < %d", 
	    startField, endField, table,
	    startField, chromStart, endField, chromEnd);
    dyStringPrintf(query, " and %s = '%s'", chromField, chrom);
    if (where != NULL)
	dyStringPrintf(query, " and %s", where);
    sr = sqlGetResult(conn, query->string);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	s = sqlUnsigned(row[0]);
	e = sqlUnsigned(row[1]);
	sz = e - s;
	if (seq != NULL && (sz < 0 || e >= size))
	    errAbort("Coordinates out of range %d %d (%s size is %d)", s, e, chrom, size);
	outputDna(f, chrom, table, s, sz, dna, nibFileName, nibFile, nibSize, '+');
	}
    }

/* Merge nearby upper case blocks. */
if (merge > 0)
    {
    e = -merge-10;		/* Don't merge initial block. */
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

if (merge >= 0)
    {
    for (i=0; i<=size; ++i)
	{
	isUpper = isupper(dna[i]);
	if (isUpper && !lastIsUpper)
	    s = i;
	else if (!isUpper && lastIsUpper)
	    {
	    e = i;
	    sz = e - s;
	    writeOut(f, chrom, s, sz, '+', dna+s, makeFaStartLine(chrom, table, s, sz));
	    }
	lastIsUpper = isUpper;
	}
    }

hFreeConn(&conn);
freeDnaSeq(&seq);
carefulClose(&nibFile);
}

void getFeatDna(char *table, char *chrom, char *outName)
/* getFeatDna - Get dna for a type of feature an all relevant chromosomes. */
{
char chrTableBuf[256];
struct slName *chromList = NULL, *chromEl;
boolean chromSpecificTable = !hTableExists(table);
char *chrTable = (chromSpecificTable ? chrTableBuf : table);
FILE *f = mustOpen(outName, "w");
boolean toStdout = (sameString(outName, "stdout"));

if (sameWord(chrom, "all"))
    chromList = hAllChromNames();
else
    chromList = newSlName(chrom);

for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    chrom = chromEl->name;
    if (chromSpecificTable)
        {
	sprintf(chrTable, "%s_%s", chrom, table);
	if (!hTableExists(table))
	    errAbort("table %s (and %s) don't exist in %s", table, 
	         chrTable, database);
	}
    if (!toStdout)
        printf("Processing %s %s\n", chrTable, chrom);
    chromFeatDna(chrTable, chrom, f);
    }
carefulClose(&f);
if (!toStdout)
    printf("%d features extracted to %s\n", blockIx, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 5)
    usage();
chromStart = cgiUsualInt("chromStart", chromStart);
chromEnd = cgiUsualInt("chromEnd", chromEnd);
where = cgiOptionalString("where");
breakUp = cgiBoolean("breakUp");
merge = cgiUsualInt("merge", merge);
database = argv[1];
hSetDb(database);
getFeatDna(argv[2], argv[3], argv[4]);
return 0;
}

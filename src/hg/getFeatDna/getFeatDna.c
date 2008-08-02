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

static char const rcsid[] = "$Id: getFeatDna.c,v 1.9.116.2 2008/08/02 04:06:20 markd Exp $";

/* Variables set explicitly or optionally from command line. */
char *database = NULL;		/* Which database? */
int chromStart = 0;		/* Start of range to select from. */
int chromEnd = BIGNUM;          /* End of range. */
char *where = NULL;		/* Extra selection info. */
boolean breakUp = FALSE;	/* Break up things? */
int merge = -1;			/* Merge close blocks? */
char *outputType = "fasta";	/* Type of output. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getFeatDna - Get dna for a type of feature\n"
  "usage:\n"
  "   getFeatDna database table chrom output.fa\n"
  "where:\n"
  "   database is the name of a browser database, like 'hg6'\n"
  "   table is the name of a table in this database, like 'exoFish'\n"
  "   chrom is a chromosome name, like 'chr8' or 'chr7_random'.  'all'\n"
  "       can be used here to denote all chromosomes\n"
  "   output.fa is where to put the output in Fasta format\n"
  "options:\n"
  "   -chromStart=NNN   restrict to features that start after NNN\n"
  "   -chromEnd=NNN     restrict to features that end before NNN\n"
  "   '-where=some sql pattern'  restrict to features matching some sql pattern\n"
  "   -breakUp=how break up alignments into pieces.  How is one of\n"
  "                exons - parts that align\n"
  "                introns - parts between alignments\n"
  "                cdsExons - coding exons (only for gene predictions)\n"
  "   -merge=N     merge blocks separated by N or less\n"
  "   -output=format format of output.  One of:\n"
  "                fasta - fasta sequence (default)\n"
  "                pos - position list\n"
  "   -addStart=NNN How much to add to feature start coordinate\n"
  "   -addEnd=NNN   How much to add to feature end coordinate\n"
  "   -addTo=how    How to add to feature coordinates.  One of:\n"
  "                   whole - the whole thing (default)\n"
  "                   start - addStart/padEnd relative to feature start\n"
  "                   end   - addStart/padEnd relative to feature end\n"
  );
}

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

void outputDna(FILE *f, char *chrom, char *table, 
    int start, int size, char *dna, 
    char *nibFileName, FILE *nibFile, int nibSize, 
    char strand, char *faName)
/* Output DNA either directly from nib or by upper-casing DNA. */
{
if (merge >= 0)
    toUpperN(dna + start, size);
else
    {
    struct dnaSeq *seq = nibLdPart(nibFileName, nibFile, nibSize, start, size);
    if (strand == '-')
        reverseComplement(seq->dna, size);
    if (faName == NULL)
        faName = makeFaStartLine(chrom, table, start, size);
    writeOut(f, chrom, start, size, strand, seq->dna, faName);
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
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
char chromField[32], startField[32], endField[32];
struct dnaSeq *seq = NULL;
DNA *dna = NULL;
int s=0,e,sz,i,size=0;
boolean lastIsUpper = FALSE, isUpper = FALSE;
char nibFileName[512];
FILE *nibFile = NULL;
int nibSize;

if (!hFindChromStartEndFields(database, table, chromField, startField, endField))
    errAbort("Couldn't find chrom/start/end fields in table %s", table);

if (merge >= 0)
    {
    seq = hLoadChrom(database, chrom);
    dna = seq->dna;
    size = seq->size;
    }
else
    {
    hNibForChrom(database, chrom, nibFileName);
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
		     outputDna(f, chrom, table, 
		         s, sz, dna, nibFileName, nibFile, nibSize, '-', NULL);
		     }
		}
	    else
		{
		for (i=0; i<psl->blockCount; ++i)
		     outputDna(f, chrom, table, psl->tStarts[i], psl->blockSizes[i], 
			    dna, nibFileName, nibFile, nibSize, '+', NULL);
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
		    s, sz, dna, nibFileName, nibFile, nibSize, 
		    gp->strand[0], gp->name);
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
	outputDna(f, chrom, table, s, sz, dna, nibFileName, nibFile, nibSize, 
		'+', NULL);
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
boolean chromSpecificTable = !hTableExists(database, table);
char *chrTable = (chromSpecificTable ? chrTableBuf : table);
FILE *f = mustOpen(outName, "w");
boolean toStdout = (sameString(outName, "stdout"));

if (sameWord(chrom, "all"))
    chromList = hAllChromNames(database);
else
    chromList = newSlName(chrom);

for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    chrom = chromEl->name;
    if (chromSpecificTable)
        {
	sprintf(chrTable, "%s_%s", chrom, table);
	if (!hTableExists(database, table))
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
getFeatDna(argv[2], argv[3], argv[4]);
return 0;
}

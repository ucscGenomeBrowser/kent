/* featureBits - Correlate tables via bitmap projections and booleans. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "fa.h"
#include "bits.h"
#include "bed.h"
#include "psl.h"
#include "portable.h"
#include "featureBits.h"

int minSize = 1;	/* Minimum size of feature. */
char *clChrom = "all";	/* Which chromosome. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "featureBits - Correlate tables via bitmap projections. \n"
  "usage:\n"
  "   featureBits database table(s)\n"
  "This will return the number of bits in all the tables anded together"
  "Options:\n"
  "   -bed=output.bed   Put intersection into bed format\n"
  "   -fa=output.fa     Put sequence in intersection into .fa file\n"
  "   -minSize=N        Minimum size to output (default 1)\n"
  "   -chrom=chrN       Restrict to one chromosome\n"
  "You can include a '!' before a table name to negate it.\n"
  "Some table names can be followed by modifiers such as:\n"
  "    :exon:N  Break into exons and add N to each end of each exon\n"
  "    :upstream:N  Consider the region of N bases before region\n"
  );
}

void bitsToBed(Bits *bits, char *chrom, int chromSize, FILE *bed, FILE *fa, 
	int minSize)
/* Write out runs of bits of at least minSize as items in a bed file. */
{
int i;
boolean thisBit, lastBit = FALSE;
int start = 0;
int id = 0;

for (i=0; i<=chromSize; ++i)	/* We depend on extra zero bit at end. */
    {
    thisBit = bitReadOne(bits, i);
    if (thisBit)
	{
	if (!lastBit)
	    start = i;
	}
    else
        {
	if (lastBit && i-start >= minSize)
	    {
	    if (bed)
		fprintf(bed, "%s\t%d\t%d\t%s.%d\n", chrom, start, i, chrom, ++id);
	    if (fa)
	        {
		char name[256];
		struct dnaSeq *seq = hDnaFromSeq(chrom, start, i, dnaLower);
		sprintf(name, "%s:%d-%d", chrom, start, i);
		faWriteNext(fa, name, seq->dna, seq->size);
		freeDnaSeq(&seq);
		}
	    }
	}
    lastBit = thisBit;
    }
}

void check(struct sqlConnection *conn, char *table)
/* Check it's as planned. */
{
char query[256], **row;
struct sqlResult *sr;
int lastEnd = -1, lastStart = -1, start, end;
sprintf(query, "select chromStart,chromEnd from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    start = atoi(row[0]);
    end = atoi(row[1]);
    if (start < lastStart)
        printf("Out of order: %d,%d\n", lastStart, start);
    if (rangeIntersection(lastStart, lastEnd, start-1, end) > 0)
        printf("Overlapping: (%d %d) (%d %d)\n", lastStart, lastEnd, start, end);
    lastStart = start;
    lastEnd = end;
    }
sqlFreeResult(&sr);
errAbort("All for now");
}

char *chromFileName(char *track, char *chrom, char fileName[512])
/* Return chromosome specific version of file if it exists. */
{
if (fileExists(track))
    {
    strcpy(fileName, track);
    }
else
    {
    char dir[256], root[128], ext[65];
    splitPath(track, dir, root, ext);
    sprintf(fileName, "%s%s_%s%s", dir, chrom, track, fileName);
    if (!fileExists(fileName))
	{
        warn("Couldn't find %s or %s", track, fileName);
	}
    else
	{
	strcpy(fileName, track);
	}
    }
return fileName;
}

void outOfRange(struct lineFile *lf, char *chrom, int chromSize)
/* Complain that coordinate is out of range. */
{
errAbort("Coordinate out of allowed range [%d,%d) for %s line %d of %s",
	0, chromSize, chrom, lf->lineIx, lf->fileName);
}

void setPslBits(struct lineFile *lf, 
	Bits *bits, struct psl *psl, int winStart, int winEnd)
/* Set bits that are in psl. */
{
int i, s, e, w, blockCount = psl->blockCount;
boolean isRev = (psl->strand[1] == '-');
for (i=0; i<blockCount; ++i)
    {
    s = psl->tStarts[i];
    e = s + psl->blockSizes[i];
    if (isRev)
       {
       /* Use w as a temp variable to reverse coordinates s&e. */
       w = psl->tSize - e;
       e = psl->tSize - s;
       s = w;
       }
    /* Clip, and if anything left set it. */
    if (s < winStart) outOfRange(lf, psl->tName, psl->tSize);
    if (e > winEnd) outOfRange(lf, psl->tName, psl->tSize);
    w = e - s;
    if (w > 0)
	bitSetRange(bits, s, w);
    }
}

void fbOrPsl(Bits *acc, char *track, char *chrom, int chromSize)
/* Or in bits of psl file that correspond to chrom. */
{
struct lineFile *lf;
char fileName[512];
struct psl *psl;

chromFileName(track, chrom, fileName);
if (!fileExists(fileName))
    return;
lf = pslFileOpen(fileName);
while ((psl = pslNext(lf)) != NULL)
    {
    if (sameString(psl->tName, chrom))
	setPslBits(lf, acc, psl, 0, chromSize);
    pslFree(&psl);
    }
lineFileClose(&lf);
}

void fbOrBed(Bits *acc, char *track, char *chrom, int chromSize)
/* Or in bits of psl file that correspond to chrom. */
{
struct lineFile *lf;
char fileName[512];
char *row[3];
int s, e, w;

chromFileName(track, chrom, fileName);
if (!fileExists(fileName))
    return;
lf = lineFileOpen(fileName, TRUE);
while (lineFileRow(lf, row))
    {
    if (sameString(row[0], chrom))
        {
	s = lineFileNeedNum(lf, row, 1);
	if (s < 0) outOfRange(lf, chrom, chromSize);
	e = lineFileNeedNum(lf, row, 2);
	if (e > chromSize) outOfRange(lf, chrom, chromSize);
	w = e - s;
	if (w > 0)
	    bitSetRange(acc, s, w);
	}
    }
lineFileClose(&lf);
}



void orTable(Bits *acc, char *track, char *chrom, 
	int chromSize, struct sqlConnection *conn)
/* Or in table if it exists.  Else do nothing. */
{
boolean hasBin;
char t[512], *s;
char table[512];
boolean isSplit;
strcpy(t, track);
s = strrchr(t, ':');
if (s != NULL) *s = 0;
isSplit = hFindSplitTable(chrom, t, table, &hasBin);
s = strchr(track, '.');
if (s != NULL)
    {
    if (sameString(s, ".psl"))
        {
	fbOrPsl(acc, track, chrom, chromSize);
	}
    else if (sameString(s, ".bed"))
        {
	fbOrBed(acc, track, chrom, chromSize);
	}
    }
else
    {
    if (hTableExists(table))
	fbOrTableBits(acc, track, chrom, chromSize, conn);
    }
}


void chromFeatureBits(struct sqlConnection *conn,
	char *chrom, int tableCount, char *tables[],
	FILE *bedFile, FILE *faFile,
	int *retChromSize, int *retChromBits)
/* featureBits - Correlate tables via bitmap projections and booleans
 * on one chromosome. */
{
int chromSize, i;
Bits *acc = NULL;
Bits *bits = NULL;
char *table;

*retChromSize = chromSize = hChromSize(chrom);
acc = bitAlloc(chromSize+1);
bits = bitAlloc(chromSize+1);
for (i=0; i<tableCount; ++i)
    {
    boolean not = FALSE;
    table = tables[i];
    if (table[0] == '!')
        {
	not = TRUE;
	++table;
	}
    if (i == 0)
        {
	orTable(acc, table, chrom, chromSize, conn);
	if (not)
	   bitNot(acc, chromSize);
	}
    else
	{
	bitClear(bits, chromSize);
	orTable(bits, table, chrom, chromSize, conn);
	if (not)
	   bitNot(bits, chromSize);
	bitAnd(acc, bits, chromSize);
	}
    }
*retChromBits = bitCountRange(acc, 0, chromSize);
if (bedFile != NULL || faFile != NULL)
    {
    minSize = cgiUsualInt("minSize", minSize);
    bitsToBed(acc, chrom, chromSize, bedFile, faFile, minSize);
    }
bitFree(&acc);
bitFree(&bits);
}

void featureBits(char *database, int tableCount, char *tables[])
/* featureBits - Correlate tables via bitmap projections and booleans. */
{
struct sqlConnection *conn = NULL;
char *bedName = cgiOptionalString("bed"), *faName = cgiOptionalString("fa");
FILE *bedFile = NULL, *faFile = NULL;
struct slName *allChroms = NULL, *chrom = NULL;
double totalBases = 0, totalBits = 0;

hSetDb(database);
if (bedName)
    bedFile = mustOpen(bedName, "w");
if (faName)
    faFile = mustOpen(faName, "w");
clChrom = cgiUsualString("chrom", clChrom);
if (sameWord(clChrom, "all"))
    allChroms = hAllChromNames();
else
    allChroms = newSlName(clChrom);
conn = hAllocConn();
for (chrom = allChroms; chrom != NULL; chrom = chrom->next)
    {
    int chromSize, chromBitSize;
    chromFeatureBits(conn, chrom->name, tableCount, tables,
        bedFile, faFile, &chromSize, &chromBitSize);
    totalBases += chromSize;
    totalBits += chromBitSize;
    }
printf("%1.0f bases of %1.0f (%3.2f%%) in intersection\n",
    totalBits, totalBases, 100.0*totalBits/totalBases);
hFreeConn(&conn);
}


int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 3)
    usage();
featureBits(argv[1], argc-2, argv+2);
return 0;
}

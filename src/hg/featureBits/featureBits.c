/* featureBits - Correlate tables via bitmap projections and booleans. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "fa.h"
#include "bits.h"
#include "featureBits.h"

int minSize = 1;	/* Minimum size of feature. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "featureBits - Correlate tables via bitmap projections. \n"
  "usage:\n"
  "   featureBits database chromosome table(s)\n"
  "This will return the number of bits in all the tables anded together"
  "Options:\n"
  "   -bed=output.bed   Put intersection into bed format\n"
  "   -fa=output.fa     Put sequence in intersection into .fa file\n"
  "   -minSize=N        Minimum size to output (default 1)\n"
  "You can include a '!' before a table name to negate it.\n"
  "Some table names can be followed by modifiers such as:\n"
  "    :exon:N  Break into exons and add N to each end of each exon\n"
  "    :upstream:N  Consider the region of N bases before region\n"
  );
}

void bitsToBed(Bits *bits, char *chrom, int chromSize, char *bedName, char *faName, int minSize)
/* Write out runs of bits of at least minSize as items in a bed file. */
{
int i;
boolean thisBit, lastBit = FALSE;
int start = 0;
int id = 0;
FILE *bed = NULL, *fa = NULL;

if (bedName != NULL)
    bed = mustOpen(bedName, "w");
if (faName != NULL)
    fa = mustOpen(faName, "w");

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
carefulClose(&bed);
carefulClose(&fa);
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

void featureBits(char *database, char *chrom, int tableCount, char *tables[])
/* featureBits - Correlate tables via bitmap projections and booleans. */
{
struct sqlConnection *conn;
int chromSize, i, setSize;
Bits *acc = NULL;
Bits *bits = NULL;
char *table;

hSetDb(database);
conn = hAllocConn();
chromSize = hChromSize(chrom);
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
	fbOrTableBits(acc, table, chrom, chromSize, conn);
	if (not)
	   bitNot(acc, chromSize);
	}
    else
	{
	bitClear(bits, chromSize);
	fbOrTableBits(bits, table, chrom, chromSize, conn);
	if (not)
	   bitNot(bits, chromSize);
	bitAnd(acc, bits, chromSize);
	}
    }
hFreeConn(&conn);
setSize = bitCountRange(acc, 0, chromSize);
printf("%d bases of %d (%4.2f%%) in intersection\n", setSize, chromSize, 100.0*setSize/chromSize);
if (cgiVarExists("bed") || cgiVarExists("fa"))
    {
    char *bedName = cgiOptionalString("bed");
    char *faName = cgiOptionalString("fa");
    minSize = cgiUsualInt("minSize", minSize);
    bitsToBed(acc, chrom, chromSize, bedName, faName, minSize);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc < 4)
    usage();
featureBits(argv[1], argv[2], argc-3, argv+3);
return 0;
}

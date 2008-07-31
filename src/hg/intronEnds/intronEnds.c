/* intronEnds - Gather stats on intron ends.. */
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "hash.h"
#include "cheapcgi.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "jksql.h"
#include "hdb.h"
#include "genePred.h"

static char const rcsid[] = "$Id: intronEnds.c,v 1.4.338.1 2008/07/31 02:24:25 markd Exp $";

char *chromName;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "intronEnds - Gather stats on intron ends.\n"
  "usage:\n"
  "   intronEnds database geneTable\n"
  "options:\n"
  "   -chrom=chrN - restrict to a particular chromosome\n"
  "   -withUtr - restrict to genes with UTR regions\n"
  );
}

void intronEnds(char *database, char *table)
/* intronEnds - Gather stats on intron ends.. */
{
struct dyString *query = newDyString(1024);
struct sqlConnection *conn;
struct sqlResult *sr;
char **row;
struct genePred *gp;
int total = 0;
int gtag = 0;
int gcag = 0;
int atac = 0;
int ctac = 0;
DNA ends[4];
int exonIx, txStart;
struct dnaSeq *seq;
int rowOffset;
char strand;

rowOffset = hOffsetPastBin(database, NULL, table);
conn = hAllocConn(database);
dyStringPrintf(query, "select * from %s", table);
if (chromName != NULL)
    dyStringPrintf(query, " where chrom = '%s'", chromName);
if (cgiBoolean("withUtr"))
    {
    dyStringPrintf(query, " %s txStart != cdsStart", 
        (chromName == NULL ? "where" : "and"));
    }
sr = sqlGetResult(conn, query->string);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    strand = gp->strand[0];
    txStart = gp->txStart;
    seq = hDnaFromSeq(database, gp->chrom, txStart, gp->txEnd, dnaLower);
    for (exonIx=1; exonIx < gp->exonCount; ++exonIx)
        {
	++total;
	memcpy(ends, seq->dna + gp->exonEnds[exonIx-1] - txStart, 2);
	memcpy(ends+2, seq->dna + gp->exonStarts[exonIx] - txStart - 2, 2);
	if (strand == '-')
	    reverseComplement(ends, 4);
	if (ends[0] == 'g' && ends[1] == 't' && ends[2] == 'a' && ends[3] == 'g')
	   ++gtag;
	if (ends[0] == 'g' && ends[1] == 'c' && ends[2] == 'a' && ends[3] == 'g')
	   ++gcag;
	if (ends[0] == 'a' && ends[1] == 't' && ends[2] == 'a' && ends[3] == 'c')
	   ++atac;
	if (ends[0] == 'c' && ends[1] == 't' && ends[2] == 'a' && ends[3] == 'c')
	   ++ctac;
	}
    freeDnaSeq(&seq);
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printf("gt/ag %d (%4.2f)\n", gtag, 100.0*gtag/total);
printf("gc/ag %d (%4.2f)\n", gcag, 100.0*gcag/total);
printf("at/ac %d (%4.2f)\n", atac, 100.0*atac/total);
printf("ct/ac %d (%4.2f)\n", ctac, 100.0*ctac/total);
printf("Total %d\n", total);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
chromName = cgiOptionalString("chrom");
if (argc != 3)
    usage();
intronEnds(argv[1], argv[2]);
return 0;
}

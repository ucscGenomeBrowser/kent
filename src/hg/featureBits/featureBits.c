/* featureBits - Correlate tables via bitmap projections and booleans. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "hdb.h"
#include "bits.h"
#include "trackDb.h"
#include "bed.h"
#include "psl.h"
#include "genePred.h"
#include "rmskOut.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "featureBits - Correlate tables via bitmap projections and booleans\n"
  "usage:\n"
  "   featureBits database chromosome table(s)\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

boolean promoterQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a promoter qualifier. */
{
if (qualifier != NULL && sameString(qualifier, "promoter"))
    {
    int size = 100;
    if (extra != NULL)
        size = atoi(extra);
    *retSize = size;
    return TRUE;
    }
else
    return FALSE;
}

void fbOrPslBits(Bits *bits, int chromSize, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a psl table - or results exon by exon into bits. */
{
struct psl *psl;
char **row;
int i, blockCount, *tStarts, *blockSizes, s, e, w;
boolean doPromo;
int promoSize;

doPromo = promoterQualifier(qualifier, extra, &promoSize);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    if (psl->tSize != chromSize)
        errAbort("Inconsistent chromSize (%d) and tSize (%d) in fbOrPslBits", chromSize, psl->tSize);
    if (doPromo)
        {
	if (psl->strand[0] == '-')
	    bitSetRange(bits, psl->tEnd, promoSize);
	else
	    bitSetRange(bits, psl->tStart-promoSize, promoSize);
	}
    else
	{
	blockCount = psl->blockCount;
	blockSizes = psl->blockSizes;
	tStarts = psl->tStarts;
	if (psl->strand[1] == '-')
	    {
	    for (i=0; i<blockCount; ++i)
		{
		w = blockSizes[i];
		s = chromSize - tStarts[i] - w;
		bitSetRange(bits, s, w);
		}
	    }
	else
	    {
	    for (i=0; i<blockCount; ++i)
		bitSetRange(bits, tStarts[i], blockSizes[i]);
	    }
	}
    pslFree(&psl);
    }
}

void fbOrBedBits(Bits *bits, int chromSize, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a bed table - or results of whole thing. */
{
struct bed *bed;
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoad3(row+rowOffset);
    bitSetRange(bits, bed->chromStart, bed->chromEnd - bed->chromStart);
    bedFree(&bed);
    }
}

void fbOrGenePredBits(Bits *bits, int chromSize, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a genePred table - or results of whole thing. */
{
struct genePred *gp;
char **row;
int i, count, s, e, w, *starts, *ends;
int uglyCount = 0;
boolean doPromo, doCds = FALSE;
int promoSize;

doPromo = promoterQualifier(qualifier, extra, &promoSize);
if (!doPromo)
    {
    if (qualifier != NULL && sameWord(qualifier, "CDS"))
        doCds = TRUE;
    }

while ((row = sqlNextRow(sr)) != NULL)
    {
    ++uglyCount;
    gp = genePredLoad(row+rowOffset);
    if (doPromo)
	{
	if (gp->strand[0] == '-')
	    bitSetRange(bits, gp->txEnd, promoSize);
	else
	    bitSetRange(bits, gp->txStart-promoSize, promoSize);
	}
    else
	{
	count = gp->exonCount;
	starts = gp->exonStarts;
	ends = gp->exonEnds;
	for (i=0; i<count; ++i)
	    {
	    s = starts[i];
	    e = ends[i];
	    if (doCds)
	        {
		if (s < gp->cdsStart) s = gp->cdsStart;
		if (e > gp->cdsEnd) e = gp->cdsEnd;
		}
	    w = e - s;
	    if (w > 0)
		bitSetRange(bits, s, w);
	    }
	}
    genePredFree(&gp);
    }
uglyf("Got %d rows of genePreds\n", uglyCount);
}

void fbOrRmskBits(Bits *bits, int chromSize, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a RepeatMasker table - or results of whole thing. */
{
struct rmskOut ro;
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    rmskOutStaticLoad(row+rowOffset, &ro);
    bitSetRange(bits, ro.genoStart, ro.genoEnd - ro.genoStart);
    }
}

void parseTrackQualifier(char *trackQualifier, char **retTrack, 
	char **retQualifier, char **retExtra)
/* Parse track:qualifier:extra. */
{
char *words[4];
int wordCount;
words[1] = words[2] = words[3] = 0;
wordCount = chopString(trackQualifier, ":", words, ArraySize(words));
if (wordCount < 1)
    errAbort("empty trackQualifier");
*retTrack = words[0];
*retQualifier = words[1];
*retExtra = words[2];
}

void fbOrTableBits(Bits *bits, char *trackQualifier, char *chrom, 
	int chromSize, struct sqlConnection *conn)
/* Ors in features in track on chromosome into bits.  */
{
struct trackDb *tdb;
char *type;
int rowOffset;
struct sqlResult *sr;
char *track, *qualifier, *extra;

parseTrackQualifier(trackQualifier, &track, &qualifier, &extra);
tdb = hTrackInfo(conn, track);
sr = hChromQuery(conn, track, chrom, NULL, &rowOffset);
type = tdb->type;

if (startsWith("psl ", type))
    {
    fbOrPslBits(bits, chromSize, sr, rowOffset, qualifier, extra);
    }
else if (startsWith("bed ", type))
    {
    fbOrBedBits(bits, chromSize, sr, rowOffset, qualifier, extra);
    }
else if (sameString("genePred", type) || startsWith("genePred ", type))
    {
    fbOrGenePredBits(bits, chromSize, sr, rowOffset, qualifier, extra);
    }
else if (sameString("rmsk", type))
    {
    fbOrRmskBits(bits, chromSize, sr, rowOffset, qualifier, extra);
    }
else
    {
    errAbort("Unknown table type %s", type);
    }
sqlFreeResult(&sr);
}

void featureBits(char *database, char *chrom, int tableCount, char *tables[])
/* featureBits - Correlate tables via bitmap projections and booleans. */
{
struct sqlConnection *conn;
int chromSize, i, setSize;
Bits *acc = NULL;
Bits *bits = NULL;

hSetDb(database);
conn = hAllocConn();
chromSize = hChromSize(chrom);
acc = bitAlloc(chromSize);
bits = bitAlloc(chromSize);

fbOrTableBits(acc, tables[0], chrom, chromSize, conn);
for (i=1; i<tableCount; ++i)
    {
    bitClear(bits, chromSize);
    fbOrTableBits(bits, tables[i], chrom, chromSize, conn);
    bitAnd(acc, bits, chromSize);
    }
setSize = bitCountRange(acc, 0, chromSize);
printf("%d bases of %d (%4.2f%%) in intersection\n", setSize, chromSize, 100.0*setSize/chromSize);
hFreeConn(&conn);
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

/* FeatureBits - convert features tables to bitmaps. */
#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "bits.h"
#include "trackDb.h"
#include "bed.h"
#include "psl.h"
#include "genePred.h"
#include "rmskOut.h"
#include "featureBits.h"

static boolean fetchQualifiers(char *type, char *qualifier, char *extra, int *retSize)
/* Return true if qualifier is of type.  Convert extra to *retSize. */
{
if (qualifier != NULL && sameWord(qualifier, type))
    {
    int size = 0;
    if (extra != NULL)
        size = atoi(extra);
    *retSize = size;
    return TRUE;
    }
else
    return FALSE;
}

static boolean upstreamQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a upstream qualifier. */
{
return fetchQualifiers("upstream", qualifier, extra, retSize);
}

static boolean exonQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a exon qualifier. */
{
return fetchQualifiers("exon", qualifier, extra, retSize);
}

static boolean cdsQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a exon qualifier. */
{
return fetchQualifiers("cds", qualifier, extra, retSize);
}

void setRangePlusExtra(Bits *bits, int s, int e, int extraStart, int extraEnd, int chromSize)
/* Set range between s and e plus possibly some extra. */
{
int w;
s -= extraStart;
if (s < 0) s = 0;
e += extraEnd;
if (e > chromSize) e = chromSize;
w = e - s;
if (w > 0)
    bitSetRange(bits, s, w);
}

static void fbOrPslBits(Bits *bits, int chromSize, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a psl table - or results exon by exon into bits. */
{
struct psl *psl;
char **row;
int i, blockCount, *tStarts, *blockSizes, s, e, w;
boolean doUp, doExon;
int promoSize, extraSize = 0;

doUp = upstreamQualifier(qualifier, extra, &promoSize);
doExon = exonQualifier(qualifier, extra, &extraSize);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    if (psl->tSize != chromSize)
        errAbort("Inconsistent chromSize (%d) and tSize (%d) in fbOrPslBits", chromSize, psl->tSize);
    if (doUp)
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
		e = s + w;
		setRangePlusExtra(bits, s, e, (i == 0 ? 0 : extraSize), 
			(i == blockCount-1 ?  0 : extraSize), chromSize);
		}
	    }
	else
	    {
	    for (i=0; i<blockCount; ++i)
		{
		s = tStarts[i];
		e = s + blockSizes[i];
		setRangePlusExtra(bits, s, e, (i == 0 ? 0 : extraSize), 
			(i == blockCount-1 ? 0 : extraSize), chromSize);
		}
	    }
	}
    pslFree(&psl);
    }
}

static void fbOrBedBits(Bits *bits, int chromSize, struct sqlResult *sr, int rowOffset,
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

static void fbOrGenePredBits(Bits *bits, int chromSize, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a genePred table - or results of whole thing. */
{
struct genePred *gp;
char **row;
int i, count, s, e, w, *starts, *ends;
boolean doUp, doCds = FALSE, doExon;
int promoSize, extraSize = 0;

if ((doUp = upstreamQualifier(qualifier, extra, &promoSize)) != FALSE)
    {
    }
else if ((doCds = cdsQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    }
else if ((doExon = exonQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    if (doUp)
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
	    setRangePlusExtra(bits, s, e, 
	    	(i == 0 ? 0 : extraSize), 
		(i == count-1 ? 0 : extraSize), chromSize);
	    }
	}
    genePredFree(&gp);
    }
}

static void fbOrRmskBits(Bits *bits, int chromSize, struct sqlResult *sr, int rowOffset,
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

static void parseTrackQualifier(char *trackQualifier, char **retTrack, 
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

trackQualifier = cloneString(trackQualifier);
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
freeMem(trackQualifier);
}


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

boolean fbUnderstandTrack(char *track)
/* Return TRUE if can turn track into a set of ranges or bits. */
{
struct trackDb *tdb;
boolean understand = FALSE;
char *type;

struct sqlConnection *conn = hAllocConn();
tdb = hTrackInfo(conn, track);
type = tdb->type;
understand = (startsWith("psl ", type) || startsWith("bed ", type) ||
	sameString("genePred", type) || startsWith("genePred ", type) ||
	sameString("rmsk", type));
trackDbFree(&tdb);
hFreeConn(&conn);
return understand;
}

static void fbAddFeature(struct featureBits **pList, int start, int size, int winStart, int winEnd)
/* Add new feature to head of list. */
{
struct featureBits *fb;
int s, e;

s = start;
e = s + size;
if (s < winStart) s = winStart;
if (e > winEnd) e = winEnd;
if (s < e)
    {
    AllocVar(fb);
    fb->start = s;
    fb->end = e;
    slAddHead(pList, fb);
    }
}

void setRangePlusExtra(struct featureBits **pList, int s, int e, int extraStart, int extraEnd, 
	int winStart, int winEnd)
/* Set range between s and e plus possibly some extra. */
{
int w;
s -= extraStart;
e += extraEnd;
w = e - s;
fbAddFeature(pList, s, w, winStart,winEnd);
}


static struct featureBits *fbPslBits(int winStart, int winEnd, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a psl table - or results exon by exon into bits. */
{
struct psl *psl;
char **row;
int i, blockCount, *tStarts, *blockSizes, s, e, w;
boolean doUp, doExon;
int promoSize, extraSize = 0;
struct featureBits *fbList = NULL, *fb;
int chromSize;

doUp = upstreamQualifier(qualifier, extra, &promoSize);
doExon = exonQualifier(qualifier, extra, &extraSize);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    chromSize = psl->tSize;
    if (doUp)
        {
	if (psl->strand[0] == '-')
	    fbAddFeature(&fbList, psl->tEnd, promoSize, winStart, winEnd);
	else
	    fbAddFeature(&fbList, psl->tStart-promoSize, promoSize, winStart, winEnd);
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
		setRangePlusExtra(&fbList, s, e, (i == 0 ? 0 : extraSize), 
			(i == blockCount-1 ?  0 : extraSize), winStart, winEnd);
		}
	    }
	else
	    {
	    for (i=0; i<blockCount; ++i)
		{
		s = tStarts[i];
		e = s + blockSizes[i];
		setRangePlusExtra(&fbList, s, e, (i == 0 ? 0 : extraSize), 
			(i == blockCount-1 ? 0 : extraSize), winStart, winEnd);
		}
	    }
	}
    pslFree(&psl);
    }
slReverse(&fbList);
return fbList;
}

static struct featureBits *fbBedBits(int winStart, int winEnd, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a bed table - or results of whole thing. */
{
struct bed *bed;
char **row;
struct featureBits *fbList = NULL;

while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoad3(row+rowOffset);
    fbAddFeature(&fbList, bed->chromStart, bed->chromEnd - bed->chromStart, winStart, winEnd);
    bedFree(&bed);
    }
slReverse(&fbList);
return fbList;
}

static struct featureBits *fbGenePredBits(int winStart, int winEnd, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a genePred table - or results of whole thing. */
{
struct genePred *gp;
char **row;
int i, count, s, e, w, *starts, *ends;
boolean doUp, doCds = FALSE, doExon;
int promoSize, extraSize = 0;
struct featureBits *fbList = NULL;

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
	    fbAddFeature(&fbList, gp->txEnd, promoSize, winStart, winEnd);
	else
	    fbAddFeature(&fbList, gp->txStart-promoSize, promoSize, winStart, winEnd);
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
	    setRangePlusExtra(&fbList, s, e, 
	    	(i == 0 ? 0 : extraSize), 
		(i == count-1 ? 0 : extraSize), winStart, winEnd);
	    }
	}
    genePredFree(&gp);
    }
slReverse(&fbList);
return fbList;
}

static struct featureBits *fbRmskBits(int winStart, int winEnd, struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a RepeatMasker table - or results of whole thing. */
{
struct rmskOut ro;
char **row;
struct featureBits *fbList = NULL;

while ((row = sqlNextRow(sr)) != NULL)
    {
    rmskOutStaticLoad(row+rowOffset, &ro);
    fbAddFeature(&fbList, ro.genoStart, ro.genoEnd - ro.genoStart, winStart, winEnd);
    }
slReverse(&fbList);
return fbList;
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

struct featureBits *fbGetRange(char *trackQualifier, char *chrom,
    int chromStart, int chromEnd)
/* Get features in range. */
{
struct featureBits *fbList = NULL;
struct trackDb *tdb;
char *type;
int rowOffset;
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char *track, *qualifier, *extra;
int chromSize = hChromSize(chrom);

trackQualifier = cloneString(trackQualifier);
parseTrackQualifier(trackQualifier, &track, &qualifier, &extra);
tdb = hTrackInfo(conn, track);
if (chromStart == 0 && chromEnd >= chromSize)
    sr = hChromQuery(conn, track, chrom, NULL, &rowOffset);
else
    sr = hRangeQuery(conn, track, chrom, chromStart, chromEnd, NULL, &rowOffset);
type = tdb->type;
if (startsWith("psl ", type))
    {
    fbList = fbPslBits(chromStart, chromEnd, sr, rowOffset, qualifier, extra);
    }
else if (startsWith("bed ", type))
    {
    fbList = fbBedBits(chromStart, chromEnd, sr, rowOffset, qualifier, extra);
    }
else if (sameString("genePred", type) || startsWith("genePred ", type))
    {
    fbList = fbGenePredBits(chromStart, chromEnd, sr, rowOffset, qualifier, extra);
    }
else if (sameString("rmsk", type))
    {
    fbList = fbRmskBits(chromStart, chromEnd, sr, rowOffset, qualifier, extra);
    }
else
    {
    errAbort("Unknown table type %s", type);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
freeMem(trackQualifier);
return fbList;
}

void fbOrBits(Bits *bits, int bitSize, struct featureBits *fbList, int bitOffset)
/* Or in bits.   Bits should have bitSize bits.  */
{
int s, e, w;
struct featureBits *fb;

for (fb = fbList; fb != NULL; fb = fb->next)
    {
    s = fb->start - bitOffset;
    e = fb->end - bitOffset;
    if (e > bitSize) e = bitSize;
    if (s < 0) s = 0;
    w = e - s;
    if (w > 0)
	bitSetRange(bits, s , w);
    }
}

void fbOrTableBits(Bits *bits, char *trackQualifier, char *chrom, 
	int chromSize, struct sqlConnection *conn)
{
struct featureBits *fbList = fbGetRange(trackQualifier, chrom, 0, chromSize);
fbOrBits(bits, chromSize, fbList, 0);
slFreeList(&fbList);
}





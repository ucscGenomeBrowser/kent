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

void featureBitsFree(struct featureBits **pBits)
/* Free up feature bits. */
{
struct featureBits *bits;
if ((bits = *pBits) != NULL)
    {
    freeMem(bits->name);
    freeMem(bits->chrom);
    freez(pBits);
    }
}

void featureBitsFreeList(struct featureBits **pList)
/* Free up a list of featureBits */
{
struct featureBits *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    freeMem(el);
    }
*pList = NULL;
}

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

static boolean endQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's an end qualifier. */
{
return fetchQualifiers("end", qualifier, extra, retSize);
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

static void fbAddFeature(struct featureBits **pList, char *name,
	char *chrom, int start, int size, char strand, 
	int winStart, int winEnd)
/* Add new feature to head of list.  Name can be NULL. */
{
struct featureBits *fb;
int s, e;
char nameBuf[512];

if (name == NULL)
    sprintf(nameBuf, "%s:%d-%d", chrom, start+1, start+size);
else
    sprintf(nameBuf, "%s %s:%d-%d", name, chrom, start+1, start+size);
s = start;
e = s + size;
if (s < winStart) s = winStart;
if (e > winEnd) e = winEnd;
if (s < e)
    {
    AllocVar(fb);
    fb->name = cloneString(nameBuf);
    fb->chrom = cloneString(chrom);
    fb->start = s;
    fb->end = e;
    fb->strand = strand;
    slAddHead(pList, fb);
    }
}

static void setRangePlusExtra(struct featureBits **pList, 
	char *name, char *chrom, int s, int e, char strand, 
	int extraStart, int extraEnd, 
	int winStart, int winEnd)
/* Set range between s and e plus possibly some extra. */
{
int w;
s -= extraStart;
e += extraEnd;
w = e - s;
fbAddFeature(pList, name, chrom, s, w, strand, winStart,winEnd);
}


char frForStrand(char strand)
/* Return 'r' for '-', else 'f' */
{
if (strand == '-')
    return 'r';
else
    return 'f';
}

static struct featureBits *fbPslBits(int winStart, int winEnd, 
	struct sqlResult *sr, int rowOffset,
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
char *chrom;
char nameBuf[512];

doUp = upstreamQualifier(qualifier, extra, &promoSize);
doExon = exonQualifier(qualifier, extra, &extraSize);
while ((row = sqlNextRow(sr)) != NULL)
    {
    psl = pslLoad(row+rowOffset);
    chrom = psl->tName;
    chromSize = psl->tSize;
    if (doUp)
        {
	int start;
	char strand;


	if (psl->strand[0] == '-')
	    {
	    start = psl->tEnd;
	    strand = '-';
	    }
	else
	    {
	    start = psl->tStart - promoSize;
	    strand = '+';
	    }
	sprintf(nameBuf, "%s_up_%d_%s_%d_%c", 
		psl->qName, promoSize, chrom, start+1, frForStrand(strand));
	fbAddFeature(&fbList, nameBuf, chrom, start, promoSize, strand, 
		winStart, winEnd);
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
		setRangePlusExtra(&fbList, NULL, chrom, s, e, '-', 
			(i == 0 ? 0 : extraSize), 
			(i == blockCount-1 ?  0 : extraSize), winStart, winEnd);
		}
	    }
	else
	    {
	    for (i=0; i<blockCount; ++i)
		{
		s = tStarts[i];
		e = s + blockSizes[i];
		setRangePlusExtra(&fbList, NULL, chrom, s, e, '+', 
			(i == 0 ? 0 : extraSize), 
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
    fbAddFeature(&fbList, NULL, bed->chrom, 
    	bed->chromStart, bed->chromEnd - bed->chromStart, 
    	'?', winStart, winEnd);
    bedFree(&bed);
    }
slReverse(&fbList);
return fbList;
}

static struct featureBits *fbGenePredBits(int winStart, int winEnd, 
	struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a genePred table - or results of whole thing. */
{
struct genePred *gp;
char **row;
int i, count, s, e, w, *starts, *ends;
boolean doUp = FALSE, doEnd = FALSE, doCds = FALSE, doExon = FALSE;
int promoSize = 0, extraSize = 0;
struct featureBits *fbList = NULL;
char nameBuf[512];

if ((doUp = upstreamQualifier(qualifier, extra, &promoSize)) != FALSE)
    {
    }
else if ((doCds = cdsQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    }
else if ((doExon = exonQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    }
else if ((doEnd = endQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    if (doUp)
	{
	int start;
	char strand;
	if (gp->strand[0] == '-')
	    {
	    start = gp->txEnd;
	    strand = '-';
	    }
	else
	    {
	    start = gp->txStart - promoSize;
	    strand = '+';
	    }
	sprintf(nameBuf, "%s_up_%d_%s_%d_%c", 
		gp->name, promoSize, gp->chrom, start+1, frForStrand(strand));
	fbAddFeature(&fbList, nameBuf, gp->chrom, start, promoSize, strand, 
	    winStart, winEnd);
	}
    else if (doEnd)
        {
	int start;
	char strand;
	if (gp->strand[0] == '-')
	    {
	    start = gp->txStart;
	    strand = '-';
	    }
	else
	    {
	    start = gp->txEnd - extraSize;
	    strand = '+';
	    }
	sprintf(nameBuf, "%s_end_%d_%s_%d_%c", 
		gp->name, promoSize, gp->chrom, start+1, frForStrand(strand));
	fbAddFeature(&fbList, nameBuf, gp->chrom, start, extraSize, strand, 
	    winStart, winEnd);
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
	    setRangePlusExtra(&fbList, NULL, gp->chrom, s, e, gp->strand[0],
	    	(i == 0 ? 0 : extraSize), 
		(i == count-1 ? 0 : extraSize), winStart, winEnd);
	    }
	}
    genePredFree(&gp);
    }
slReverse(&fbList);
return fbList;
}

static struct featureBits *fbRmskBits(int winStart, int winEnd, 
	struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a RepeatMasker table - or results of whole thing. */
{
struct rmskOut ro;
char **row;
struct featureBits *fbList = NULL;

while ((row = sqlNextRow(sr)) != NULL)
    {
    rmskOutStaticLoad(row+rowOffset, &ro);
    fbAddFeature(&fbList, NULL, ro.genoName, 
    	ro.genoStart, ro.genoEnd - ro.genoStart, ro.strand[0],
    	winStart, winEnd);
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





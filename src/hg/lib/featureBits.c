/* FeatureBits - convert features tables to bitmaps. */
#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "bits.h"
#include "cheapcgi.h"
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

static boolean scoreQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a score qualifier. */
{
return fetchQualifiers("score", qualifier, extra, retSize);
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

static boolean intronQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a exon qualifier. */
{
return fetchQualifiers("intron", qualifier, extra, retSize);
}

static boolean cdsQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a cds qualifier. */
{
return fetchQualifiers("cds", qualifier, extra, retSize);
}

static boolean endQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's an end qualifier. */
{
boolean res = fetchQualifiers("downstream", qualifier, extra, retSize);
if (res)
    return res;
return fetchQualifiers("end", qualifier, extra, retSize);
}

static boolean utr3Qualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a utr3 qualifier. */
{
return fetchQualifiers("utr3", qualifier, extra, retSize);
}

static boolean utr5Qualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a utr5 qualifier. */
{
return fetchQualifiers("utr5", qualifier, extra, retSize);
}



boolean fbUnderstandTrack(char *track)
/* Return TRUE if can turn track into a set of ranges or bits. */
{
struct trackDb *tdb;
boolean understand = FALSE;
char *type;

struct sqlConnection *conn = hAllocConn();
tdb = hMaybeTrackInfo(conn, track);
if (tdb == NULL)
    return FALSE;
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

static void fbNoOptions()
{
puts("<TABLE><TR><TD>\n");
cgiMakeRadioButton("fbQual", "whole", TRUE);
puts(" Whole Item </TD><TD> </TD></TR></TABLE>\n");
}

static void fbPslOptions()
{
puts("<TABLE><TR><TD>\n");
cgiMakeRadioButton("fbQual", "whole", TRUE);
puts(" Whole Alignment </TD><TD> ");
puts(" </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "exon", FALSE);
puts(" Blocks plus </TD><TD> ");
cgiMakeTextVar("fbExonBases", "0", 8);
puts(" bases at each end </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "intron", FALSE);
puts(" Regions between blocks plus </TD><TD> ");
cgiMakeTextVar("fbIntronBases", "0", 8);
puts(" bases at each end </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "upstream", FALSE);
puts(" Upstream by </TD><TD> ");
cgiMakeTextVar("fbUpBases", "200", 8);
puts(" bases </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "end", FALSE);
puts(" Downstream by </TD><TD> ");
cgiMakeTextVar("fbDownBases", "200", 8);
puts(" bases </TD></TR></TABLE>");
}

static struct featureBits *fbPslBits(int winStart, int winEnd, 
	struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a psl table - or results exon by exon into bits. */
{
struct psl *psl;
char **row;
int i, blockCount, *tStarts, *blockSizes, s, e, w;
boolean doUp, doExon, doEnd, doScore;
int promoSize = 0, endSize = 0, extraSize = 0, scoreThreshold = 0;
struct featureBits *fbList = NULL, *fb;
int chromSize;
char *chrom;
char nameBuf[512];

if (scoreQualifier(qualifier, extra, &scoreThreshold))
    errAbort("Can't handle score on psl tables yet, sorry");
if (intronQualifier(qualifier, extra, &extraSize))
    errAbort("Can't handle intron on psl tables yet, sorry");
doUp = upstreamQualifier(qualifier, extra, &promoSize);
doEnd = endQualifier(qualifier, extra, &endSize);
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
    else if (doEnd)
        {
	int start;
	char strand;

	if (psl->strand[0] == '-')
	    {
	    start = psl->tStart - endSize;
	    strand = '-';
	    }
	else
	    {
	    start = psl->tEnd;
	    strand = '+';
	    }
	sprintf(nameBuf, "%s_up_%d_%s_%d_%c", 
		psl->qName, endSize, chrom, start+1, frForStrand(strand));
	fbAddFeature(&fbList, nameBuf, chrom, start, endSize, strand, 
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
            setRangePlusExtra(&fbList, NULL, chrom, s, e, '-', extraSize, 
				  extraSize, winStart, winEnd);
		}
	    }
	else
	    {
	    for (i=0; i<blockCount; ++i)
		{
		s = tStarts[i];
		e = s + blockSizes[i];
            setRangePlusExtra(&fbList, NULL, chrom, s, e, '+', extraSize, 
				  extraSize, winStart, winEnd);
		}
	    }
	}
    pslFree(&psl);
    }
slReverse(&fbList);
return fbList;
}

static void fbBedOptions(int fields)
{
boolean doIntron = (fields >= 12);

puts("<TABLE><TR><TD>\n");
cgiMakeRadioButton("fbQual", "whole", TRUE);
puts(" Whole Gene </TD><TD> ");
puts(" </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "exon", FALSE);
puts(" Exons plus </TD><TD> ");
cgiMakeTextVar("fbExonBases", "0", 8);
puts(" bases at each end </TD></TR><TR><TD>\n");
if (doIntron)
    {
    cgiMakeRadioButton("fbQual", "intron", FALSE);
    puts(" Introns plus </TD><TD> ");
    cgiMakeTextVar("fbIntronBases", "0", 8);
    puts(" bases at each end </TD></TR><TR><TD>\n");
    }
cgiMakeRadioButton("fbQual", "upstream", FALSE);
puts(" Upstream by </TD><TD> ");
cgiMakeTextVar("fbUpBases", "200", 8);
puts(" bases </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "end", FALSE);
puts(" Downstream by </TD><TD> ");
cgiMakeTextVar("fbDownBases", "200", 8);
puts(" bases </TD></TR></TABLE>");
}

static struct featureBits *fbBedBits(int fields, int winStart, int winEnd, struct sqlResult *sr, 
	int rowOffset, char *qualifier, char *extra)
/* Given a sqlQuery result on a bed table - or results of whole thing. */
{
struct bed *bed;
char **row;
struct featureBits *fbList = NULL;
char strand = '?';
boolean doUp, doEnd, doExon = FALSE, doIntron = FALSE, doScore = FALSE;
int promoSize, endSize, extraSize, scoreThreshold;
int i, count, *starts, *sizes;

doScore = scoreQualifier(qualifier, extra, &scoreThreshold);
doUp = upstreamQualifier(qualifier, extra, &promoSize);
doEnd = endQualifier(qualifier, extra, &endSize);
if (doExon = exonQualifier(qualifier, extra, &extraSize))
    ;
else if (doIntron = intronQualifier(qualifier, extra, &extraSize))
    ;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int s, e; 
    bed = bedLoadN(row+rowOffset, fields);
    if (fields >= 5)
         strand = bed->strand[0];
    if (doUp)
        {
	if (strand == '-')
	    {
	    s = bed->chromEnd;
	    e = s + promoSize;
	    }
	else
	    {
	    e = bed->chromStart;
	    s = e - promoSize;
	    }
	fbAddFeature(&fbList, bed->name, bed->chrom, s, e - s, strand,
		     winStart, winEnd);
	}
    else if (doEnd)
        {
	if (strand == '-')
	    {
	    e = bed->chromStart;
	    s = e - endSize;
	    }
	else
	    {
	    s = bed->chromEnd;
	    e = s + endSize;
	    }
	fbAddFeature(&fbList, bed->name, bed->chrom, s, e - s, strand,
		     winStart, winEnd);
	}
    else if (doIntron)
        {
	if (fields >= 12)
	    {
	    count  = bed->blockCount;
	    starts = bed->chromStarts;
	    sizes  = bed->blockSizes;
	    for (i=1; i<count; ++i)
	      {
		s = bed->chromStart + starts[i-1] + sizes[i-1];
		e = bed->chromStart + starts[i];
		setRangePlusExtra(&fbList, bed->name, bed->chrom, s, e, strand,
				  extraSize, extraSize, winStart, winEnd);
	      }
	    }
	/* N/A if fields < 12.... but don't crash, just return empty. */
	}
    else
        {
	if (fields >= 12)
	    {
	    count  = bed->blockCount;
	    starts = bed->chromStarts;
	    sizes  = bed->blockSizes;
	    for (i=0; i<count; ++i)
	      {
		s = bed->chromStart + starts[i];
		e = bed->chromStart + starts[i] + sizes[i];
        if (!doScore || (doScore && bed->score >= scoreThreshold))
            setRangePlusExtra(&fbList, bed->name, bed->chrom, s, e, strand,
				  extraSize, extraSize, winStart, winEnd);
	      }
	    }
	else
  	    {
	    s = bed->chromStart;
	    e = bed->chromEnd;
        if (!doScore || (doScore && bed->score >= scoreThreshold))
            setRangePlusExtra(&fbList, bed->name, bed->chrom, s, e, strand,
			      extraSize, extraSize, winStart, winEnd);
	    }
	}
    bedFree(&bed);
    }
slReverse(&fbList);
return fbList;
}

static void fbGenePredOptions()
{
puts("<TABLE><TR><TD>\n");
cgiMakeRadioButton("fbQual", "whole", TRUE);
puts(" Whole Gene </TD><TD> ");
puts(" </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "exon", FALSE);
puts(" Exons plus </TD><TD> ");
cgiMakeTextVar("fbExonBases", "0", 8);
puts(" bases at each end </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "intron", FALSE);
puts(" Introns plus </TD><TD> ");
cgiMakeTextVar("fbIntronBases", "0", 8);
puts(" bases at each end </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "cds", FALSE);
puts(" Coding Exons </TD><TD> ");
puts(" </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "utr3", FALSE);
puts(" 3' UTR </TD><TD> ");
puts(" </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "utr5", FALSE);
puts(" 5' UTR </TD><TD> ");
puts(" </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "upstream", FALSE);
puts(" Upstream by </TD><TD> ");
cgiMakeTextVar("fbUpBases", "200", 8);
puts(" bases </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "end", FALSE);
puts(" Downstream by </TD><TD> ");
cgiMakeTextVar("fbDownBases", "200", 8);
puts(" bases </TD></TR></TABLE>");
}

static struct featureBits *fbGenePredBits(int winStart, int winEnd, 
	struct sqlResult *sr, int rowOffset,
	char *qualifier, char *extra)
/* Given a sqlQuery result on a genePred table - or results of whole thing. */
{
struct genePred *gp;
char **row;
int i, count, s, e, w, *starts, *ends;
boolean doUp = FALSE, doEnd = FALSE, doCds = FALSE, doExon = FALSE,
	doUtr3 = FALSE, doUtr5 = FALSE, doIntron = FALSE;
int promoSize = 0, extraSize = 0, endSize = 0;
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
else if ((doIntron = intronQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    }
else if ((doEnd = endQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    }
else if ((doUtr3 = utr3Qualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    }
else if ((doUtr5 = utr5Qualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    }
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    if (doUp)
	{
	if (gp->txStart != gp->cdsStart && gp->txEnd != gp->cdsEnd)
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
	}
    else if (doEnd)
        {
	int start;
	char strand;
	if (gp->txStart != gp->cdsStart && gp->txEnd != gp->cdsEnd)
	    {
	    if (gp->strand[0] == '-')
		{
		start = gp->txStart - extraSize;
		strand = '-';
		}
	    else
		{
		start = gp->txEnd;
		strand = '+';
		}
	    sprintf(nameBuf, "%s_end_%d_%s_%d_%c", 
		    gp->name, promoSize, gp->chrom, 
		    start+1, frForStrand(strand));
	    fbAddFeature(&fbList, nameBuf, gp->chrom, 
	    	start, extraSize, strand, 
		winStart, winEnd);
	    }
	}
    else if (doIntron)
        {
	count = gp->exonCount;
	starts = gp->exonStarts;
	ends = gp->exonEnds;
	for (i=1; i<count; ++i)
	    {
	    s = ends[i-1];
	    e = starts[i];
	    setRangePlusExtra(&fbList, gp->name, gp->chrom, s, e, gp->strand[0],
		extraSize, extraSize, winStart, winEnd);
	    }
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
	    else if (doUtr5)
	        {
		if (gp->strand[0] == '-')
		    {
		    if (s < gp->cdsEnd) s = gp->cdsEnd;
		    }
		else
		    {
		    if (e > gp->cdsStart) e = gp->cdsStart;
		    }
		}
	    else if (doUtr3)
	        {
		if (gp->strand[0] == '-')
		    {
		    if (e > gp->cdsStart) e = gp->cdsStart;
		    }
		else
		    {
		    if (s < gp->cdsEnd) s = gp->cdsEnd;
		    }
		}
	    setRangePlusExtra(&fbList, gp->name, gp->chrom, s, e, gp->strand[0],
			      extraSize, extraSize, winStart, winEnd);
	    }
	}
    genePredFree(&gp);
    }
slReverse(&fbList);
return fbList;
}

void fbRmskOptions()
{
fbNoOptions();
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

void fbOptions(char *track)
/* Print out an HTML table with radio buttons for featureBits options. */
{
struct sqlConnection *conn;
struct trackDb *tdb;
char *type;

if (! fbUnderstandTrack(track))
    {
    fbNoOptions();
    return;
    }

conn = hAllocConn();
tdb = hTrackInfo(conn, track);
type = tdb->type;
if (startsWith("psl ", type))
    {
    fbPslOptions();
    }
else if (startsWith("bed ", type))
    {
    int fields = atoi(type + strlen("bed "));
    if (fields <= 3) fields = 3;
    fbBedOptions(fields);
    }
else if (sameString("genePred", type) || startsWith("genePred ", type))
    {
    fbGenePredOptions();
    }
else if (sameString("rmsk", type))
    {
    fbRmskOptions();
    }
else
    {
    errAbort("Unknown table type %s", type);
    }
hFreeConn(&conn);
}

char *fbOptionsToQualifier()
/* Translate CGI variable created by fbOptions() to a featureBits qualifier. */
{
char qual[128];
char *fbQual  = cgiOptionalString("fbQual");

if (fbQual == NULL)
    return NULL;

if (sameString(fbQual, "whole"))
    qual[0] = 0;
else if (sameString(fbQual, "exon"))
    snprintf(qual, sizeof(qual), "%s:%s", fbQual, cgiString("fbExonBases"));
else if (sameString(fbQual, "intron"))
    snprintf(qual, sizeof(qual), "%s:%s", fbQual,
			 cgiString("fbIntronBases"));
else if (sameString(fbQual, "upstream"))
    snprintf(qual, sizeof(qual), "%s:%s", fbQual, cgiString("fbUpBases"));
else if (sameString(fbQual, "end"))
    snprintf(qual, sizeof(qual), "%s:%s", fbQual, cgiString("fbDownBases"));
else
    strcpy(qual, fbQual);
return(cloneString(qual));
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
    int fields = atoi(type + strlen("bed "));
    if (fields <= 3) fields = 3;
    fbList = fbBedBits(fields, chromStart, chromEnd, sr, rowOffset, qualifier, extra);
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





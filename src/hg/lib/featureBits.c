/* FeatureBits - convert features tables to bitmaps. */
#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "bits.h"
#include "cart.h"
#include "cheapcgi.h"
#include "trackDb.h"
#include "bed.h"
#include "psl.h"
#include "genePred.h"
#include "rmskOut.h"
#include "featureBits.h"
#include "fa.h"

static char const rcsid[] = "$Id: featureBits.c,v 1.29.42.1 2008/07/31 02:24:27 markd Exp $";

/* By default, clip features to the search range.  It's important to clip 
 * when featureBits output will be used to populate Bits etc.  But allow 
 * the user to turn off clipping if they don't want it. */
boolean clipToWin = TRUE;

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
    featureBitsFree(&el);
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


static boolean upstreamAllQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's a upstreamAll qualifier. */
{
return fetchQualifiers("upstreamAll", qualifier, extra, retSize);
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
/* Return TRUE if it's an end or downstream qualifier. */
{
boolean res = fetchQualifiers("downstream", qualifier, extra, retSize);
if (res)
    return res;
return fetchQualifiers("end", qualifier, extra, retSize);
}

static boolean endAllQualifier(char *qualifier, char *extra, int *retSize)
/* Return TRUE if it's an endAll or downstreamAll qualifier. */
{
boolean res = fetchQualifiers("downstreamAll", qualifier, extra, retSize);
if (res)
    return res;
return fetchQualifiers("endAll", qualifier, extra, retSize);
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



boolean fbUnderstandTrack(char *db, char *track)
/* Return TRUE if can turn track into a set of ranges or bits. */
{
struct hTableInfo *hti = hFindTableInfoDb(db, NULL, track);

if (hti == NULL)
    return FALSE;
else
    return hti->isPos;
}

static void fbAddFeature(char *db, struct featureBits **pList, char *name,
	char *chrom, int start, int size, char strand, 
	int winStart, int winEnd)
/* Add new feature to head of list.  Name can be NULL. */
{
struct featureBits *fb;
int s, e;
char nameBuf[512];
int chromSize = hChromSize(db, chrom);

if (name == NULL)
    safef(nameBuf, sizeof(nameBuf), "%s:%d-%d", chrom, start+1, start+size);
else
    safef(nameBuf, sizeof(nameBuf), "%s %s:%d-%d", name, chrom, start+1,
	  start+size);
s = start;
e = s + size;
/* Padding might push us off the edge of the chrom; if so, truncate: */
if (s < 0)
    s = 0;
if (e > chromSize)
    e = chromSize;
if (clipToWin)
    {
    if (s < winStart) s = winStart;
    if (e > winEnd) e = winEnd;
    }
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

static void setRangePlusExtra(char *db, struct featureBits **pList, 
	char *name, char *chrom, int s, int e, char strand, 
	int extraStart, int extraEnd, 
	int winStart, int winEnd)
/* Set range between s and e plus possibly some extra. */
{
int w;
s -= extraStart;
e += extraEnd;
w = e - s;
fbAddFeature(db, pList, name, chrom, s, w, strand, winStart,winEnd);
}


char frForStrand(char strand)
/* Return 'r' for '-', else 'f' */
{
if (strand == '-')
    return 'r';
else
    return 'f';
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

void fbOptionsHtiCart(struct hTableInfo *hti, struct cart *cart)
/* Print out an HTML table with radio buttons for featureBits options. 
 * Use defaults from CGI and cart. */
{
boolean isGene;
char *setting, *fbQual;

if (sameString("psl", hti->type))
    isGene = FALSE;
else
    isGene = TRUE;

puts("<TABLE><TR><TD>\n");
fbQual = cartCgiUsualString(cart, "fbQual", "whole");
cgiMakeRadioButton("fbQual", "whole", sameString(fbQual, "whole"));
if (isGene)
    puts(" Whole Gene </TD><TD> ");
else
    puts(" Whole Alignment </TD><TD> ");
puts(" </TD></TR><TR><TD>\n");
cgiMakeRadioButton("fbQual", "upstreamAll", sameString(fbQual, "upstreamAll"));
puts(" Upstream by </TD><TD> ");
setting = cartCgiUsualString(cart, "fbUpBases", "200");
cgiMakeTextVar("fbUpBases", setting, 8);
puts(" bases </TD></TR><TR><TD>\n");
if (hti->hasBlocks)
    {
    cgiMakeRadioButton("fbQual", "exon", sameString(fbQual, "exon"));
    if (isGene)
	puts(" Exons plus </TD><TD> ");
    else
	puts(" Blocks plus </TD><TD> ");
    setting = cartCgiUsualString(cart, "fbExonBases", "0");
    cgiMakeTextVar("fbExonBases", setting, 8);
    puts(" bases at each end </TD></TR><TR><TD>\n");
    cgiMakeRadioButton("fbQual", "intron", sameString(fbQual, "intron"));
    if (isGene)
	puts(" Introns plus </TD><TD> ");
    else
	puts(" Regions between blocks plus </TD><TD> ");
    setting = cartCgiUsualString(cart, "fbIntronBases", "0");
    cgiMakeTextVar("fbIntronBases", setting, 8);
    puts(" bases at each end </TD></TR><TR><TD>\n");
    }
if (hti->hasBlocks && hti->hasCDS)
    {
    cgiMakeRadioButton("fbQual", "utr5", sameString(fbQual, "utr5"));
    puts(" 5' UTR Exons </TD><TD> ");
    puts(" </TD></TR><TR><TD>\n");
    cgiMakeRadioButton("fbQual", "cds", sameString(fbQual, "cds"));
    puts(" Coding Exons </TD><TD> ");
    puts(" </TD></TR><TR><TD>\n");
    cgiMakeRadioButton("fbQual", "utr3", sameString(fbQual, "utr3"));
    puts(" 3' UTR Exons </TD><TD> ");
    puts(" </TD></TR><TR><TD>\n");
    }
else if (hti->hasCDS)
    {
    cgiMakeRadioButton("fbQual", "utr5", sameString(fbQual, "utr5"));
    puts(" 5' UTR  </TD><TD> ");
    puts(" </TD></TR><TR><TD>\n");
    cgiMakeRadioButton("fbQual", "cds", sameString(fbQual, "cds"));
    puts(" CDS </TD><TD> ");
    puts(" </TD></TR><TR><TD>\n");
    cgiMakeRadioButton("fbQual", "utr3", sameString(fbQual, "utr3"));
    puts(" 3' UTR </TD><TD> ");
    puts(" </TD></TR><TR><TD>\n");
    }
cgiMakeRadioButton("fbQual", "endAll", sameString(fbQual, "endAll"));
puts(" Downstream by </TD><TD> ");
setting = cartCgiUsualString(cart, "fbDownBases", "200");
cgiMakeTextVar("fbDownBases", setting, 8);
puts(" bases </TD></TR></TABLE>");
puts("Note: if a feature is close to the beginning or end of a chromosome \n"
     "and upstream/downstream bases are added, they may be truncated \n"
     "in order to avoid extending past the edge of the chromosome. <BR>");
}

void fbOptionsHti(struct hTableInfo *hti)
/* Print out an HTML table with radio buttons for featureBits options.
 * Use defaults from CGI. */
{
fbOptionsHtiCart(hti, NULL);
}

void fbOptions(char *db, char *track)
/* Print out an HTML table with radio buttons for featureBits options. */
{
struct hTableInfo *hti = hFindTableInfoDb(db, NULL, track);
if (hti == NULL)
    errAbort("Could not find table info for table %s in database %s",
	     track, db);
fbOptionsHti(hti);
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
else if (sameString(fbQual, "upstreamAll"))
    snprintf(qual, sizeof(qual), "%s:%s", fbQual, cgiString("fbUpBases"));
else if (sameString(fbQual, "upstream"))
    snprintf(qual, sizeof(qual), "%s:%s", fbQual, cgiString("fbUpBases"));
else if (sameString(fbQual, "endAll"))
    snprintf(qual, sizeof(qual), "%s:%s", fbQual, cgiString("fbDownBases"));
else if (sameString(fbQual, "end"))
    snprintf(qual, sizeof(qual), "%s:%s", fbQual, cgiString("fbDownBases"));
else
    strcpy(qual, fbQual);
return(cloneString(qual));
}

struct featureBits *fbFromBed(char *db, char *trackQualifier, struct hTableInfo *hti,
	struct bed *bedList, int chromStart, int chromEnd,
	boolean clipToWindow, boolean filterOutNoUTR)
/* Translate a list of bed items into featureBits. */
{
struct bed *bed;
struct featureBits *fbList = NULL;
char nameBuf[512];
char *fName;
char *track, *qualifier, *extra;
boolean doUp = FALSE, doEnd = FALSE, doCds = FALSE, doExon = FALSE,
	doUtr3 = FALSE, doUtr5 = FALSE, doIntron = FALSE, doScore = FALSE,
	doUpAll = FALSE, doEndAll = FALSE;
int promoSize = 0, extraSize = 0, endSize = 0, scoreThreshold = 0;
boolean canDoIntrons, canDoUTR, canDoScore;
boolean oldClipToWin = clipToWin;
int i, count, *starts, *sizes;
int s, e;

clipToWin = clipToWindow;

trackQualifier = cloneString(trackQualifier);
parseTrackQualifier(trackQualifier, &track, &qualifier, &extra);
canDoUTR = hti->hasCDS;
canDoIntrons = hti->hasBlocks;
canDoScore = (hti->scoreField[0] != 0);

if ((doScore = scoreQualifier(qualifier, extra, &scoreThreshold)) != FALSE)
    {
    if (! canDoScore)
	errAbort("Can't handle score on table %s, sorry", track);
    }

if ((doUpAll = upstreamAllQualifier(qualifier, extra, &promoSize)) != FALSE)
    {
    }
else if ((doUp = upstreamQualifier(qualifier, extra, &promoSize)) != FALSE)
    {
    }
else if ((doEnd = endQualifier(qualifier, extra, &endSize)) != FALSE)
    {
    }
else if ((doEndAll = endAllQualifier(qualifier, extra, &endSize)) != FALSE)
    {
    }
else if ((doExon = exonQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    if (! canDoIntrons)
	errAbort("Can't handle exon on table %s, sorry", track);
    }
else if ((doIntron = intronQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    if (! canDoIntrons)
	errAbort("Can't handle intron on table %s, sorry", track);
    }
else if ((doCds = cdsQualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    if (! canDoUTR)
	errAbort("Can't handle cds on table %s, sorry", track);
    }
else if ((doUtr3 = utr3Qualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    if (! canDoUTR)
	errAbort("Can't handle utr3 on table %s, sorry", track);
    }
else if ((doUtr5 = utr5Qualifier(qualifier, extra, &extraSize)) != FALSE)
    {
    if (! canDoUTR)
	errAbort("Can't handle utr5 on table %s, sorry", track);
    }

if (doUpAll || doEndAll)
    filterOutNoUTR = FALSE;

for (bed = bedList;  bed != NULL;  bed = bed->next)
    {
    if (doUp || doUpAll)
        {
	if (!canDoUTR || !filterOutNoUTR ||
	    ((bed->chromStart != bed->thickStart) &&
	     (bed->chromEnd != bed->thickEnd)))
	    {
	    if (bed->strand[0] == '-')
		{
		s = bed->chromEnd;
		e = s + promoSize;
		}
	    else
		{
		e = bed->chromStart;
		s = e - promoSize;
		}
	    safef(nameBuf, sizeof(nameBuf), "%s_up_%d_%s_%d_%c", 
		    bed->name, promoSize, bed->chrom, s+1,
		    frForStrand(bed->strand[0]));
	    fbAddFeature(db, &fbList, nameBuf, bed->chrom, s, e - s,
			 bed->strand[0], chromStart, chromEnd);
	    }
	}
    else if (doEnd || doEndAll)
        {
	if (!canDoUTR || !filterOutNoUTR ||
	    ((bed->chromStart != bed->thickStart) &&
	     (bed->chromEnd != bed->thickEnd)))
	    {
	    if (bed->strand[0] == '-')
		{
		e = bed->chromStart;
		s = e - endSize;
		}
	    else
		{
		s = bed->chromEnd;
		e = s + endSize;
		}
	    safef(nameBuf, sizeof(nameBuf), "%s_end_%d_%s_%d_%c", 
		    bed->name, endSize, bed->chrom, s+1,
		    frForStrand(bed->strand[0]));
	    fbAddFeature(db, &fbList, nameBuf, bed->chrom, s, e - s,
			 bed->strand[0], chromStart, chromEnd);
	    }
	}
    else if (doIntron)
        {
	count  = bed->blockCount;
	starts = bed->chromStarts;
	sizes  = bed->blockSizes;
	for (i=1; i<count; ++i)
	    {
	    s = bed->chromStart + starts[i-1] + sizes[i-1];
	    e = bed->chromStart + starts[i];
	    safef(nameBuf, sizeof(nameBuf), "%s_intron_%d_%d_%s_%d_%c", 
		    bed->name, i-1, extraSize, bed->chrom, s+1,
		    frForStrand(bed->strand[0]));
	    setRangePlusExtra(db, &fbList, nameBuf, bed->chrom, s, e,
			      bed->strand[0], extraSize, extraSize,
			      chromStart, chromEnd);
	    }
	}
    else
        {
	if (canDoIntrons)
	    {
	    // doExon is the default action.
	    count  = bed->blockCount;
	    starts = bed->chromStarts;
	    sizes  = bed->blockSizes;
	    for (i=0; i<count; ++i)
		{
		s = bed->chromStart + starts[i];
		e = bed->chromStart + starts[i] + sizes[i];
		if (doCds)
		    {
		    if ((e < bed->thickStart) || (s > bed->thickEnd)) continue;
		    if (s < bed->thickStart) s = bed->thickStart;
		    if (e > bed->thickEnd) e = bed->thickEnd;
		    fName = "cds";
		    }
		else if (doUtr5)
		    {
		    if (bed->strand[0] == '-')
			{
			if (e < bed->thickEnd) continue;
			if (s < bed->thickEnd) s = bed->thickEnd;
			}
		    else
			{
			if (s > bed->thickStart) continue;
			if (e > bed->thickStart) e = bed->thickStart;
			}
		    fName = "utr5";
		    }
		else if (doUtr3)
		    {
		    if (bed->strand[0] == '-')
			{
			if (s > bed->thickStart) continue;
			if (e > bed->thickStart) e = bed->thickStart;
			}
		    else
			{
			if (e < bed->thickEnd) continue;
			if (s < bed->thickEnd) s = bed->thickEnd;
			}
		    fName = "utr3";
		    }
		else
		    {
		    fName = "exon";
		    }
		if (!doScore || (doScore && bed->score >= scoreThreshold))
		    {
		    safef(nameBuf, sizeof(nameBuf), "%s_%s_%d_%d_%s_%d_%c", 
			    bed->name, fName, i, extraSize, bed->chrom, s+1,
			    frForStrand(bed->strand[0]));
		    setRangePlusExtra(db, &fbList, nameBuf, bed->chrom, s, e,
				      bed->strand[0], extraSize, extraSize,
				      chromStart, chromEnd);
		    }
		}
	    }
	else
	    {
	    if (doCds)
		{
		s = bed->thickStart;
		e = bed->thickEnd;
		fName = "cds";
		}
	    else if (doUtr5)
		{
		if (bed->strand[0] == '-')
		    {
		    s = bed->thickEnd;
		    e = bed->chromEnd;
		    }
		else
		    {
		    s = bed->chromStart;
		    e = bed->thickStart;
		    }
		fName = "utr5";
		}
	    else if (doUtr3)
		{
		if (bed->strand[0] == '-')
		    {
		    s = bed->chromStart;
		    e = bed->thickStart;
		    }
		else
		    {
		    s = bed->thickEnd;
		    e = bed->chromEnd;
		    }
		fName = "utr3";
		}
	    else
		{
		s = bed->chromStart;
		e = bed->chromEnd;
		fName = "whole";
		}
	    if (!doScore || (doScore && bed->score >= scoreThreshold))
		{
		safef(nameBuf, sizeof(nameBuf), "%s_%s_%d_%s_%d_%c", 
			bed->name, fName, extraSize, bed->chrom, s+1,
			frForStrand(bed->strand[0]));
		setRangePlusExtra(db, &fbList, nameBuf, bed->chrom, s, e,
				  bed->strand[0], extraSize, extraSize,
				  chromStart, chromEnd);
		}
	    }
	}
    }
clipToWin = oldClipToWin;
freeMem(trackQualifier);
slReverse(&fbList);
return fbList;
}

struct featureBits *fbGetRangeQuery(char *db, char *trackQualifier,
	char *chrom, int chromStart, int chromEnd, char *sqlConstraints,
	boolean clipToWindow, boolean filterOutNoUTR)
/* Get features in range that match sqlConstraints. */
{
struct hTableInfo *hti;
struct bed *bedList;
struct featureBits *fbList;
char *tQ, *track, *qualifier, *extra;

tQ = cloneString(trackQualifier);
parseTrackQualifier(tQ, &track, &qualifier, &extra);
hti = hFindTableInfoDb(db, NULL, track);
if (hti == NULL)
    errAbort("Could not find table info for table %s in database %s",
	     track, db);
bedList = hGetBedRangeDb(db, track, chrom, chromStart, chromEnd,
			 sqlConstraints);
fbList = fbFromBed(db, trackQualifier, hti, bedList, chromStart, chromEnd,
		   clipToWindow, filterOutNoUTR);
bedFreeList(&bedList);
return(fbList);
}

struct featureBits *fbGetRange(char *db, char *trackQualifier, char *chrom,
	int chromStart, int chromEnd)
/* Get features in range that match sqlConstraints. */
{
return(fbGetRangeQuery(db, trackQualifier,
                       chrom, chromStart, chromEnd, NULL, TRUE, TRUE));
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

void fbOrTableBits(char *db, Bits *bits, char *trackQualifier, char *chrom, 
	int chromSize, struct sqlConnection *conn)
/* Ors in features in track on chromosome into bits.  */
{
struct featureBits *fbList = fbGetRange(db, trackQualifier, chrom, 0, chromSize);
fbOrBits(bits, chromSize, fbList, 0);
featureBitsFreeList(&fbList);
}

void fbOrTableBitsQueryMinSize(char *db, Bits *bits, char *trackQualifier, char *chrom, 
	int chromSize, struct sqlConnection *conn, char *sqlConstraints,
	boolean clipToWindow, boolean filterOutNoUTR, int minSize)
/* Ors in features matching sqlConstraints in track on chromosome into bits. 
   Skips featureBits that are less than minSize. minSize is useful for introns where
   things less than a given threshold are alignment gaps rather than introns. */
{
struct featureBits *goodList = NULL, *badList = NULL, *fb = NULL, *fbNext = NULL;
struct featureBits *fbList = fbGetRangeQuery(db, trackQualifier, chrom, 0,
					     chromSize, sqlConstraints,
					     clipToWindow, filterOutNoUTR);
if(minSize > 0) 
    {
    for(fb = fbList; fb != NULL; fb = fbNext)
	{
	fbNext = fb->next;
	if(fb->end - fb->start > minSize) 
	    {
	    slAddHead(&goodList, fb);
	    }
	else
	    {
	    slAddHead(&badList, fb);
	    }
	}
    }
else
    {
    goodList = fbList;
    }
fbOrBits(bits, chromSize, goodList, 0);
featureBitsFreeList(&goodList);
featureBitsFreeList(&badList);
}

void fbOrTableBitsQuery(char *db, Bits *bits, char *trackQualifier, char *chrom, 
	int chromSize, struct sqlConnection *conn, char *sqlConstraints,
	boolean clipToWindow, boolean filterOutNoUTR)
/* Ors in features matching sqlConstraints in track on chromosome into bits. */
{
fbOrTableBitsQueryMinSize(db, bits, trackQualifier, chrom, chromSize, conn,
			  sqlConstraints, clipToWindow, filterOutNoUTR, 0);
}

struct bed *fbToBedOne(struct featureBits *fb)
/* Translate a featureBits item into (scoreless) bed 6. */
{
struct bed *bed;
AllocVar(bed);
bed->chrom = cloneString(fb->chrom);
bed->chromStart = fb->start;
bed->chromEnd = fb->end;
bed->name = cloneString(fb->name);
bed->strand[0] = fb->strand;
return bed;
}

struct bed *fbToBed(struct featureBits *fbList)
/* Translate a list of featureBits items into (scoreless) bed 6. */
{
struct bed *bedList = NULL, *bed;
struct featureBits *fb;

for (fb=fbList;  fb != NULL;  fb=fb->next)
    {
    bed = fbToBedOne(fb);
    slAddHead(&bedList, bed);
    }
return(bedList);
}

void bitsToBed(char *db, Bits *bits, char *chrom, int chromSize, FILE *bed, FILE *fa, 
	int minSize)
/* Write out runs of bits of at least minSize as items in a bed file. */
{
int i;
boolean thisBit, lastBit = FALSE;
int start = 0;
int id = 0;

for (i=0; i<chromSize; ++i)
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
		struct dnaSeq *seq = hDnaFromSeq(db,chrom, start, i, dnaLower);
		sprintf(name, "%s:%d-%d", chrom, start, i);
		faWriteNext(fa, name, seq->dna, seq->size);
		freeDnaSeq(&seq);
		}
	    }
	}
    lastBit = thisBit;
    }
if (lastBit && i-start >= minSize)
    {
    if (bed)
	fprintf(bed, "%s\t%d\t%d\t%s.%d\n", chrom, start, i, chrom, ++id);
    if (fa)
	{
	char name[256];
	struct dnaSeq *seq = hDnaFromSeq(db, chrom, start, i, dnaLower);
	sprintf(name, "%s:%d-%d", chrom, start, i);
	faWriteNext(fa, name, seq->dna, seq->size);
	freeDnaSeq(&seq);
	}
    }
}


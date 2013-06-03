/* cds.c - code for coloring of bases, codons, or alignment differences. */
#include "common.h"
#include "hCommon.h"
#include "hash.h"
#include "jksql.h"
#include "memalloc.h"
#include "dystring.h"
#include "memgfx.h"
#include "hvGfx.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "hdb.h"
#include "psl.h"
#include "fa.h"
#include "genePred.h"
#include "cds.h"
#include "genbank.h"
#include "twoBit.h"
#include "hgTracks.h"
#include "cdsSpec.h"
#include "axt.h"

/*
 * WARNING: this code is incomprehensible:
 *     - variables named codon often contain amino acids, not condons
 *     - it does two passes, one undocumented function encodes both a
 *       color and an amino acid into the struct simpleFeature grayIx
 *       field and this is decoded in the second pass.
 *     - baseColorDrawItem doesn't draw a item, it draws a single codon,
 *       or a maybe even a single base.
 *     - there are many assumptions and state shared between this module
 *       and the simpleTracks.c.
 * may The Force be with you..
 */

#ifndef GBROWSE
#include "pcrResult.h"
#endif /* GBROWSE */


Color lighterShade(struct hvGfx *hvg, Color color, double percentLess);
/* Find a color which is a percentless 'lighter' shade of color */
/* Forward declaration */

/* Array of colors used in drawing codons/bases/differences: */
Color cdsColor[CDS_NUM_COLORS];
boolean cdsColorsMade = FALSE;

Color getCdsColor(int index)
{
assert(index < CDS_NUM_COLORS);
return cdsColor[index];
}

static void drawScaledBoxSampleWithText(struct hvGfx *hvg, 
                                        int chromStart, int chromEnd,
                                        double scale, int xOff, int y,
                                        int height, Color color, int score,
                                        MgFont *font, char *text, bool zoomed,
                                        int winStart, int maxPixels, boolean isCoding, boolean justifyString)
/* Draw a box scaled from chromosome to window coordinates with
   a codon or set of 3 or less bases drawn in the box. */
{

/*first draw the box itself*/
drawScaledBoxSample(hvg, chromStart, chromEnd, scale, xOff, y, height, 
		    color, score);

/*draw text in box if space, and align properly for codons or DNA*/
if (zoomed)
    {
    int i;
    int x1, x2, w;
    x1 = round((double)(chromStart-winStart)*scale) + xOff;
    x2 = round((double)(chromEnd-winStart)*scale) + xOff;
    if (x2 >= maxPixels)
        x2 = maxPixels - 1;
    w = x2-x1;
    if (w < 1)
        w = 1;

    if (chromEnd - chromStart == 3 && isCoding)
        {
        if (justifyString)
            spreadBasesString(hvg, x1, y, w, height, whiteIndex(),  font, text, strlen(text),  TRUE);
        else
            hvGfxTextCentered(hvg, x1, y, w, height, whiteIndex(), font, text);
        }
    else if (chromEnd - chromStart < 3 && isCoding)
        {
        if (justifyString)
            spreadBasesString(hvg, x1, y, w, height, cdsColor[CDS_PARTIAL_CODON], font, text, strlen(text), TRUE);
        else
            hvGfxTextCentered(hvg, x1, y, w, height, cdsColor[CDS_PARTIAL_CODON], font, text);
        }
    else
        {
        int thisX,thisX2;
        char c[2];
	c[1] = '\0';
	int iMin = max(0, (winStart-chromStart));
	int iMax = min((chromEnd-chromStart), (winEnd-chromStart));
        for (i=iMin; i<iMax; i++)
            {
	    if (text[i] == ' ')
		continue;
            c[0] = text[i];
            thisX = round((double)(chromStart+i-winStart)*scale) + xOff;
            thisX2 = round((double)(chromStart+1+i-winStart)*scale) + xOff;
            hvGfxTextCentered(hvg, thisX, y, thisX2-thisX, height,
			   whiteIndex(),font,c);
            }
        }
    }
}

static int convertCoordUsingPsl( int s, struct psl *psl )
/*return query position corresponding to target position in one 
 * of the coding blocks of a psl file, or return -1 if target position
 * is not in an exon of this psl entry*/
{
int i;
int idx = -1;
unsigned *qStarts = psl->qStarts;
unsigned tStart = 0;
unsigned thisQStart = 0;

if (psl == NULL)
    errAbort("NULL passed to convertCoordUsingPsl<br>\n");

if (s < psl->tStart || s >= psl->tEnd)
    {
    warn("Position %d is outside of this psl entry q(%s:%d-%d), t(%s:%d-%d)."
	 "<br>\n",
	 s, psl->qName, psl->qStart, psl->qEnd,
	 psl->tName, psl->tStart, psl->tEnd);
    return(-1);
    }

for ( i=0; i<psl->blockCount; i++ )
    {
    unsigned tEnd;
    tStart = psl->tStarts[i];
    tEnd = tStart + psl->blockSizes[i];
    if (psl->strand[1] == '-') 
         reverseUnsignedRange(&tStart, &tEnd, psl->tSize);

    if (s >= tStart && s < tEnd)
	{
	idx = i;
	break;
	}
    }

if (idx < 0)
    return(-1);

if(psl->strand[1] == '-')
    thisQStart = psl->qSize - (qStarts[idx]+psl->blockSizes[idx]);
else
    thisQStart = qStarts[idx];

return(thisQStart + (s - tStart));
}

/* Calls to hDnaFromSeq are rather expensive for 2bit, so cache genomic sequence */
static char *initedTrack = NULL;
static struct dnaSeq *cachedGenoDna = NULL;
static int cachedGenoStart = 0;
static int cachedGenoEnd = 0;

static void getLinkedFeaturesSpan(struct linkedFeatures *lfList, int *retStart, int *retEnd,
				  boolean isSeries)
/* Find the overall lowest and highest coords in lfList.  If any items hang off the
 * edge of the window, we will end up with coords <winStart and/or >winEnd which is
 * what we want. */
{
int start = winStart, end = winEnd;
if (isSeries)
    {
    struct linkedFeaturesSeries *lfs, *lfsList = (struct linkedFeaturesSeries *)lfList;
    for (lfs = lfsList;  lfs != NULL;  lfs = lfs->next)
	{
	if (lfs->start < start)
	    start = lfs->start;
	if (lfs->end > end)
	    end = lfs->end;
	}
    }
else
    {
    struct linkedFeatures *lf;
    for (lf = lfList;  lf != NULL;  lf = lf->next)
	{
	if (lf->start < start)
	    start = lf->start;
	if (lf->end > end)
	    end = lf->end;
	}
    }
if (retStart)
    *retStart = start;
if (retEnd)
    *retEnd = end;
}

static char *getCachedDna(int chromStart, int chromEnd)
/* Return a pointer into our cached genomic dna.  chromEnd is just for
 * bounds-checking (honor system).  Do not change or free the return value! */
{
if (!initedTrack || ! cachedGenoDna)
    errAbort("getCachedDnaAt called before baseColorInitTrack?!");
if (chromStart < cachedGenoStart || chromEnd > cachedGenoEnd)
    errAbort("getCachedDnaAt: coords %d,%d are out of cached range %d,%d",
	     chromStart, chromEnd, cachedGenoStart, cachedGenoEnd);
return &(cachedGenoDna->dna[chromStart - cachedGenoStart]);
}

static void getNextCodonDna(char *retStr, int n, struct genePred *gp, int startI,
			    boolean posStrand)
/* Get at most n bases from coding exons following exon startI. */
{
int i, j, thisN;
int cdsExonStart, cdsExonEnd;
char *codonDna;
for (i = 0;  i < n;  i++)
    retStr[i] = 'N';
retStr[n] = '\0';
if (posStrand)
    {
    for (thisN = 0, i = startI+1;  thisN < n && i < gp->exonCount;  i++)
	{
        // get dna for exon to the right
        codonDna = getCachedDna(gp->exonStarts[i], gp->exonEnds[i]);
	cdsExonStart = gp->exonStarts[i];
	cdsExonEnd = gp->exonEnds[i];
	if (gp->cdsStart < gp->txEnd && cdsExonStart < gp->cdsStart)
	    cdsExonStart = gp->cdsStart;
	if (gp->cdsEnd > gp->txStart  && cdsExonEnd > gp->cdsEnd)
	    cdsExonEnd = gp->cdsEnd;
	for (j=0; j < (cdsExonEnd - cdsExonStart); j++)
	    {
	    retStr[thisN] = codonDna[j+cdsExonStart-gp->exonStarts[i]];
	    thisN++;
	    if (thisN >= n) 
		break;
	    }
	}
    }
else
    {
    for (thisN = n-1, i = startI-1;  thisN >= 0 && i >= 0;  i--)
	{
        // get dna for exon to the left
        codonDna = getCachedDna(gp->exonStarts[i], gp->exonEnds[i]);
	cdsExonStart = gp->exonStarts[i];
	cdsExonEnd = gp->exonEnds[i];
	if (gp->cdsStart < gp->txEnd && cdsExonStart < gp->cdsStart)
	    cdsExonStart = gp->cdsStart;
	if (gp->cdsEnd > gp->txStart && cdsExonEnd > gp->cdsEnd)
	    cdsExonEnd = gp->cdsEnd;
	for (j=0; j < (cdsExonEnd - cdsExonStart); j++)
	    {
	    retStr[thisN] = codonDna[cdsExonEnd-j-1-gp->exonStarts[i]];
	    thisN--;
	    if (thisN < 0) 
		break;
	    }
	}
    }
}


static void drawVertLine(struct linkedFeatures *lf, struct hvGfx *hvg,
                         int chromStart, int xOff, int y,
			 int height, double scale, Color color)
/* Draw a 1-pixel wide vertical line at the given chromosomal coord.
 * The line is 0 bases wide (chromStart==chromEnd) but that doesn't
 * matter if we're zoomed out to >1base/pixel, so this is OK for diffs
 * when zoomed way out and for insertion points at any scale. */
{
int thisX = round((double)(chromStart-winStart)*scale) + xOff;
int thisY = y;
int thisHeight = height;
if ((chromStart < lf->tallStart) || (chromStart > lf->tallEnd))
    {
    /* adjust for UTR. WARNING: this duplicates shortOff & shortHeight
     * calculations in linkedFeaturesDrawAt */
    thisY += height/4;
    thisHeight = height - height/2;
    }
hvGfxLine(hvg, thisX, thisY, thisX, thisY+thisHeight, color);
}


static void drawCdsDiffBaseTickmarksOnly(struct track *tg,
	struct linkedFeatures *lf,
	struct hvGfx *hvg, int xOff,
	int y, double scale, int heightPer,
	struct dnaSeq *mrnaSeq, struct psl *psl,
	int winStart)
/* Draw 1-pixel wide red lines only where mRNA bases differ from genomic.  
 * This assumes that lf has been drawn already, we're zoomed out past 
 * zoomedToBaseLevel, we're not in dense mode etc. */
{
struct simpleFeature *sf = NULL;
char *winDna = getCachedDna(winStart, winEnd);
Color c = cdsColor[CDS_STOP];
// check if we need a contrasting color instead of default 'red' (CDS_STOP)
char *tickColor = NULL;
if ( tg->itemColor && (tickColor = trackDbSetting(tg->tdb, "baseColorTickColor")))
    {
    Color ci = tg->itemColor(tg, lf, hvg);
    if (sameString(tickColor, "contrastingColor"))
	c = hvGfxContrastingColor(hvg, ci);
    else if (sameString(tickColor, "lighterShade"))
	c = lighterShade(hvg, ci, 1.5);
    }
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    int s = max(winStart, sf->start);
    int e = min(winEnd, sf->end);
    if (s > winEnd || e < winStart)
      continue;
    if (e > s)
	{
	int mrnaS = -1;
	if (psl)
	    mrnaS = convertCoordUsingPsl(s, psl);
	else
	    mrnaS = sf->qStart + (s - sf->start);
	if(mrnaS >= 0)
	    {
	    int i;
	    for (i=0; i < (e - s); i++)
		{
		if (mrnaSeq->dna[mrnaS+i] != winDna[s-winStart+i])
		    drawVertLine(lf, hvg, s+i, xOff, y+1, heightPer-2, scale, c);
		}
	    }
	}
    }
}


static void maskDiffString( char *retStr, char *s1, char *s2, char mask )
/*copies s1, masking off similar characters, and returns result into retStr.
 *if strings are of different size it stops after s1 is done.*/
{
int i;
for (i=0; i<strlen(s1); i++)
    {
    if (s1[i] == s2[i])
	retStr[i] = mask;
    else 
	retStr[i] = s1[i];

    }
retStr[i] = '\0';
}

Color lighterShade(struct hvGfx *hvg, Color color, double percentLess)
    /* Get lighter shade of a color, with a variable level */ 
{
    struct rgbColor rgbColor =  hvGfxColorIxToRgb(hvg, color); 
    rgbColor.r = (int)((rgbColor.r+127)/percentLess);
    rgbColor.g = (int)((rgbColor.g+127)/percentLess);
    rgbColor.b = (int)((rgbColor.b+127)/percentLess);
    return hvGfxFindColorIx(hvg, rgbColor.r, rgbColor.g, rgbColor.b);
}


// We need an uppercase alpha character to represent the stop codon in
// the alpha-offset grayIx scheme.  J is in neither dnautil.c's
// codonTable nor axt.c's blosum62 peptide scoring matrix.  X was
// formerly used to represent stop codon, but unfortunately it also is
// the error return from dnautil.c's lookupCodon (and it is in
// blosum62, so maybe it is a valid peptide in some contexts?).
#define GRAYIX_STOP_CODON_ALPHA 'J'

// In addition to the alpha-offset encoding of peptide and alternating
// shade, grayIx can take on several negative values to represent colors
// for special cases:
#define GRAYIX_CDS_START -1
#define GRAYIX_CDS_ERROR -2
#define GRAYIX_CDS_STOP  -3
#define GRAYIX_CDS_SYN_PROT -4

static Color colorAndCodonFromGrayIx(struct hvGfx *hvg, char *codon, int grayIx, 
			      Color ixColor)
/*convert grayIx value to color and codon which
 * are both encoded in the grayIx*/
{
Color color;
if (grayIx == GRAYIX_CDS_ERROR)
    {
    color = cdsColor[CDS_ERROR];
    sprintf(codon,"X");
    }
else if (grayIx == GRAYIX_CDS_START)
    {
    color = cdsColor[CDS_START];
    sprintf(codon,"M");
    }
else if (grayIx == GRAYIX_CDS_STOP)
    {
      color = cdsColor[CDS_STOP];
    sprintf(codon,"*");
    }
else if (grayIx == GRAYIX_CDS_SYN_PROT)
    {
    color = cdsColor[CDS_SYN_PROT];
    sprintf(codon,"*");
    }
#ifdef LOWELAB
else if(grayIx == - 'V')
   {
   color = cdsColor[CDS_ALT_START];
   sprintf(codon,"%c",'V');   
   }
else if(grayIx == - 'L')
   {
   color = cdsColor[CDS_ALT_START];
   sprintf(codon,"%c",'L');   
   }
#endif
else if (grayIx <= 26)
    {
    color = ixColor;
    sprintf(codon,"%c",grayIx + 'A' - 1);
    if (codon[0] == GRAYIX_STOP_CODON_ALPHA)
	codon[0] = '*';
    }
else if (grayIx > 26)
    {
    color = lighterShade(hvg, ixColor,1.5);
    sprintf(codon,"%c",grayIx - 26 + 'A' - 1);
    if (codon[0] == GRAYIX_STOP_CODON_ALPHA)
	codon[0] = '*';
    }
else
    {
    errAbort("colorAndCodonFromGrayIx: invalid grayIx %d", grayIx);
    color = cdsColor[CDS_ERROR];
    sprintf(codon,"X");
    }
return color;
}


static char baseColorLookupCodon(DNA *dna)
/* Call dnautil's lookupCodon, but translate stop codon '\0' to '*' for display. */
{
char peptide;
if (sameString(chromName, "chrM"))
    peptide = lookupMitoCodon(dna);
else
    peptide = lookupCodon(dna);
if (peptide == '\0')
    peptide = '*';
return peptide;
}

static int peptideToGrayIx(char peptide, boolean codonFirstColor)
/* Encode peptide (a letter or '*') and alternating gray shade into our alpha-offset scheme. */
{
if (peptide == '*')
    peptide = GRAYIX_STOP_CODON_ALPHA;
if (codonFirstColor)
    return(peptide - 'A' + 1);
else
    return(peptide - 'A' + 1 + 26);
}

static int codonToGrayIx(DNA *dna, bool codonFirstColor, boolean *foundStart, 
			 boolean reverse, boolean colorStopStart)
/* Return grayIx encoding the codon and color (or alternating shades). */
{
if (reverse)
    reverseComplement(dna,strlen(dna));

char codonChar = baseColorLookupCodon(dna);
if (codonChar == 'M' && foundStart != NULL && !(*foundStart))
    *foundStart = TRUE;

#ifdef LOWELAB
if(sameString(dna,"GTG"))
   {
     return -'V';
   }
if(sameString(dna,"TTG"))
   {
     return -'L';
   }
#endif

if (codonChar == '*')
    {
    if (colorStopStart)
	return(GRAYIX_CDS_STOP);
    else
	return peptideToGrayIx(codonChar, codonFirstColor);
    }
else if (codonChar == 'X')
    return(GRAYIX_CDS_ERROR);     // bad input to lookupCodon, e.g. 'n' base
else if (colorStopStart && codonChar == 'M')
    return(GRAYIX_CDS_START);
else
    return peptideToGrayIx(codonChar, codonFirstColor);
}


static boolean protEquivalent(int aa1, int aa2)
/* returns TRUE if amino acids have a positive blosum62 score
   i.e. I, V or R, K
   else FALSE
   Note: X is a valid code in the default scheme (blosum62), so don't pass in
   dnautil.c's X which is an error return...
   */
{
static struct axtScoreScheme *ss = NULL;
if (ss == NULL)
    ss = axtScoreSchemeProteinDefault();
if ((ss->matrix[aa1][aa2]) > 0)
    return TRUE;
else
    return FALSE;
}

static int mrnaCodonToGrayIx(DNA *rna, char genomicCodon, bool codonFirstColor)
/* Difference ==> red, otherwise keep the alternating shades. */
{
char rnaCodon = baseColorLookupCodon(rna);

if (genomicCodon != rnaCodon)
    {
    if (genomicCodon != 'X' && rnaCodon != 'X' && protEquivalent(genomicCodon, rnaCodon))
        return(GRAYIX_CDS_SYN_PROT);     // yellow, "synonymous" protein 
    else
        return(GRAYIX_CDS_STOP);
    }
else
    return peptideToGrayIx(genomicCodon, codonFirstColor);
}


static void getGenbankCds(char *acc, struct genbankCds* cds)
/* Get cds start and stop from genbank tables, if available. Otherwise it
 * does nothing */
{
static boolean first = TRUE, haveGbCdnaInfo = FALSE;
if (first)
    {
    haveGbCdnaInfo = hTableExists(database, "gbCdnaInfo");
    first = FALSE;
    }
if (haveGbCdnaInfo)
    {
    char query[256], buf[256], *cdsStr;
    struct sqlConnection *conn = hAllocConn(database);
    sqlSafef(query, sizeof query, "select cds.name from gbCdnaInfo,cds where (acc = '%s') and (gbCdnaInfo.cds = cds.id)", acc);
    cdsStr = sqlQuickQuery(conn, query, buf, sizeof(buf));
    if (cdsStr != NULL)
        genbankCdsParse(cdsStr, cds);
    hFreeConn(&conn);
    }
}

static void getCdsFromTbl(char *acc, char *baseColorSetting, struct genbankCds* cds)
/* Get CDS from a specified table, doing nothing if not found */
{
char *p = skipToSpaces(baseColorSetting);
char *cdsSpecTbl = skipLeadingSpaces(p);
if (*cdsSpecTbl == '\0')
    errAbort("%s table requires a table name as an argument", BASE_COLOR_USE_CDS);
struct sqlConnection *conn = hAllocConnDbTbl(cdsSpecTbl, &cdsSpecTbl, database);
// allow multiple, but only use the first, since transMapGene table might have
// multiple entries for same gene from different source dbs.
struct cdsSpec *cdsSpec
    = sqlQueryObjs(conn, (sqlLoadFunc)cdsSpecLoad, sqlQueryMulti,
                   "SELECT * FROM %s WHERE id=\"%s\"", cdsSpecTbl, acc);
hFreeConn(&conn);
if (cdsSpec != NULL)
    genbankCdsParse(cdsSpec->cds, cds);
if (cds != NULL)
cdsSpecFreeList(&cdsSpec);
}

static void getPslCds(struct psl *psl, struct track *tg, struct genbankCds *cds)
/* get CDS defintion for a PSL */
{
ZeroVar(cds);

if (pslIsProtein(psl))
    {
    cds->start=0;
    cds->end=psl->qSize*3;
    cds->startComplete = TRUE;
    cds->endComplete = TRUE; 
    }
else 
    {
    char *setting = trackDbSetting(tg->tdb, BASE_COLOR_USE_CDS);
    char *dataName = getItemDataName(tg, psl->qName);
    if ((setting != NULL) && startsWith("table", setting))
        getCdsFromTbl(dataName, setting, cds);
    else
        getGenbankCds(dataName, cds);
    }
}

static void getHiddenGaps(struct psl *psl, unsigned *gaps)
/*return the insertions in the query sequence of a psl that are 'hidden' 
  in the browser. This lets these insertions be indicated with the
  color orange.*/
{
int i;
gaps[0] = psl->qStarts[0] - psl->qStart;
for (i=1; i<psl->blockCount; i++)
    {
    gaps[i] = psl->qStarts[i] - 
	(psl->qStarts[i-1] + psl->blockSizes[i-1]);
    }
gaps[psl->blockCount] = psl->qSize - 
    (psl->qStarts[psl->blockCount-1] + psl->blockSizes[psl->blockCount-1]);
}




static struct simpleFeature *splitPslByCodon(char *chrom, 
					     struct linkedFeatures *lf, struct 
                                             psl *psl, int sizeMul, boolean 
                                             isXeno, int maxShade, 
                                             enum baseColorDrawOpt drawOpt,
					     boolean colorStopStart,
                                             struct track *tg)
{
struct simpleFeature *sfList = NULL;

/* if cds not in genbank or not parsable - revert to normal*/
/* if showing bases we don't want to have thin bars ala genePred format */
if (drawOpt == baseColorDrawItemBases ||
    drawOpt == baseColorDrawDiffBases)
    {
    int grayIx = pslGrayIx(psl, isXeno, maxShade);
    sfList = sfFromPslX(psl, grayIx, sizeMul);
    }
else
    {
    /* Previous code didn't use exon frames for baseColorDrawGenomicCodons.
     * This meant simply counting off aligned bases to define frames.  It
     * didn't work very well for TransMap alignments and not clear that its
     * the right thing to do for any alignment.  By using exonFrames for
     * genomic codons, this is letting the query sequence define the frame.
     */
    struct genbankCds cds;
    getPslCds(psl, tg, &cds);
    int insertMergeSize = -1;
    unsigned opts = genePredCdsStatFld|genePredExonFramesFld;
    struct genePred *gp = genePredFromPsl2(psl, opts, &cds, insertMergeSize);
    lf->start = gp->txStart;
    lf->end = gp->txEnd;
    lf->tallStart = gp->cdsStart;
    lf->tallEnd = gp->cdsEnd;
    sfList = baseColorCodonsFromGenePred(lf, gp, colorStopStart, FALSE);
    genePredFree(&gp);
    }
return(sfList);
}



struct simpleFeature *baseColorCodonsFromPsl(struct linkedFeatures *lf, 
        struct psl *psl, int sizeMul, boolean isXeno, int maxShade,
        enum baseColorDrawOpt drawOpt, struct track *tg)
/* Given an lf and the psl from which the lf was constructed, 
 * return a list of simpleFeature elements, one per codon (or partial 
 * codon if the codon falls on a gap boundary.  sizeMul, isXeno and maxShade
 * are for defaulting to one-simpleFeature-per-exon if cds is not found. */
{
boolean colorStopStart = (drawOpt != baseColorDrawDiffCodons);
struct simpleFeature *sfList = splitPslByCodon(psl->tName, lf, psl, sizeMul,
                                               isXeno, maxShade, drawOpt,
					       colorStopStart, tg);
slReverse(&sfList);
return sfList;
}


enum baseColorDrawOpt baseColorGetDrawOpt(struct track *tg)
/* Determine what base/codon coloring option (if any) has been selected 
 * in trackDb/cart, and gate with zoom level. */
{
// N.B. the way the option is chosen has become insanely complex
enum baseColorDrawOpt ret = baseColorDrawOff;

if (cart == NULL || tg == NULL || tg->tdb == NULL)
    return ret ;

ret = baseColorDrawOptEnabled(cart, tg->tdb);
boolean showDiffBasesAllScales =
    (trackDbSetting(tg->tdb, "showDiffBasesAllScales") != NULL);
float showDiffBasesMaxZoom =
    trackDbFloatSettingOrDefault(tg->tdb, "showDiffBasesMaxZoom", -1.0);
if ((showDiffBasesMaxZoom >= 0)
    && ((basesPerPixel > showDiffBasesMaxZoom) || (showDiffBasesMaxZoom == 0.0)))
    showDiffBasesAllScales = FALSE;
enum baseColorDrawOpt zoomOutDefault = baseColorDrawOff;
if (trackDbSetting(tg->tdb, "showCdsAllScales") != NULL)
    {
    float showCdsMaxZoom = trackDbFloatSettingOrDefault(tg->tdb, "showCdsMaxZoom", -1.0);
    if ((showCdsMaxZoom < 0.0)
        || ((basesPerPixel <= showCdsMaxZoom) && (showCdsMaxZoom != 0.0)))
        zoomOutDefault = baseColorDrawCds;
#if 0
    /* FIXME: this doesn't actually work, it enables maxItems, but then
     * doesn't does into dense at all. need to figure this out, but in the
     * mean time, just usine showCdsMaxZoom to keep performance sane */
    // don't show CDS if in dense or squish
    if (zoomOutDefault == baseColorDrawCds)
        {
        enum trackVisibility vis = limitVisibility(tg);
        if ((vis == tvDense) || (vis == tvSquish))
            zoomOutDefault = baseColorDrawOff;
        }
#endif
    }

/* Gate with zooming constraints: */
if (!zoomedToCdsColorLevel && (ret == baseColorDrawGenomicCodons ||
                               ret == baseColorDrawItemCodons))
    ret = zoomOutDefault;
if (!zoomedToBaseLevel && (ret == baseColorDrawItemBases))
    ret = zoomOutDefault;
if (!(zoomedToCdsColorLevel || showDiffBasesAllScales) &&
    ret == baseColorDrawDiffCodons)
    ret = zoomOutDefault;
if (!(zoomedToBaseLevel || showDiffBasesAllScales) &&
    ret == baseColorDrawDiffBases)
    ret = zoomOutDefault;

return ret;
}


static struct dnaSeq *maybeGetUserSeq(char *qName)
/* Look in user's blat sequence file for qName. */
{
#ifndef GBROWSE
static struct hash *userSeqHash = NULL;
struct dnaSeq *userSeq = NULL;
char *ss = cartOptionalString(cart, "ss");

if (ss == NULL || !ssFilesExist(ss))
    return NULL;

if (userSeqHash == NULL)
    {
    struct dnaSeq *seqList = NULL, *seq;
    char *faFileName, *pslFileName;
    parseSs(ss, &pslFileName, &faFileName);
    seqList = faReadAllMixed(faFileName);
    userSeqHash = hashNew(0);
    for (seq = seqList;  seq != NULL;  seq = seq->next)
	{
	hashAdd(userSeqHash, seq->name, seq);
	}
    }

userSeq = hashFindVal(userSeqHash, qName);
if (userSeq == NULL)
    return NULL;
else
    return cloneDnaSeq(userSeq);
#else
return NULL;
#endif
}

#ifndef GBROWSE
static struct dnaSeq *maybeGetPcrResultSeq(struct linkedFeatures *lf)
/* Return (if possible) the primer sequences concatenated with the 
 * target sequence between where they match. */
{
struct dnaSeq *seq = NULL;
char *pslFileName, *primerFileName;
struct targetDb *target;
if (! pcrResultParseCart(database, cart, &pslFileName, &primerFileName, &target))
    return NULL;
char *fPrimer, *rPrimer;
pcrResultGetPrimers(primerFileName, &fPrimer, &rPrimer);
int fPrimerSize = strlen(fPrimer);
int rPrimerSize = strlen(rPrimer);
reverseComplement(rPrimer, rPrimerSize);
if (target != NULL)
    {
    struct psl *tpsl;
    char *words[3];
    int wordCount = chopByChar(cloneString(lf->extra), '|', words, ArraySize(words));
    if (wordCount != 3)
	errAbort("maybeGetPcrResultSeq: expected 3 |-sep'd words but got '%s'",
		 (char *)lf->extra);
    char *displayName = words[0];
    int ampStart = atoi(words[1]), ampEnd = atoi(words[2]);
    char *realName = pcrResultItemAccName(lf->name, displayName);
    /* isPcr results are so sparse that I think the performance impact 
     * of re-reading the psl file in the draw function is negligible. */
    pcrResultGetPsl(pslFileName, target, realName, chromName, ampStart, ampEnd, &tpsl, NULL);
    /* Use seq+extFile if specified; otherwise just retrieve from seqFile. */
    if (isNotEmpty(target->seqTable) && isNotEmpty(target->extFileTable))
	{
	struct sqlConnection *conn = hAllocConn(database);
	seq = hDnaSeqGet(database, tpsl->tName, target->seqTable,
			 target->extFileTable);
	hFreeConn(&conn);
	}
    else
	{
	struct twoBitFile *tbf = twoBitOpen(target->seqFile);
	seq = twoBitReadSeqFrag(tbf, tpsl->tName, 0, 0);
	twoBitClose(&tbf);
	}
    int start, end;
    if (tpsl->strand[0] == '-')
	{
	reverseComplement(seq->dna, seq->size);
	start = seq->size - tpsl->tEnd;
	end = seq->size - tpsl->tStart;
	}
    else
	{
	start = tpsl->tStart;
	end = tpsl->tEnd;
	}
    CopyArray(fPrimer, (seq->dna + start), fPrimerSize);
    CopyArray(rPrimer, (seq->dna + end - rPrimerSize), rPrimerSize);
    if (tpsl->strand[0] == '-')
	reverseComplement(seq->dna, seq->size);
    }
else
    {
    seq = hChromSeq(database, chromName, lf->start, lf->end);
    if (lf->orientation < 0)
	reverseComplement(seq->dna, seq->size);
    CopyArray(fPrimer, seq->dna, fPrimerSize);
    CopyArray(rPrimer, (seq->dna + seq->size - rPrimerSize), rPrimerSize);
    }
return seq;
}
#endif /* GBROWSE */

static struct dnaSeq *maybeGetExtFileSeq(char *seqSource, char *name)
/* look up sequence name in seq and extFile tables specified in seqSource */
{
/* seqSource is: extFile seqTbl extFileTbl */
static struct dyString *buf = NULL;
if (buf == NULL)
    buf = dyStringNew(0);
dyStringClear(buf);
dyStringAppend(buf, seqSource);
char *words[3];
int nwords = chopByWhite(buf->string, words, ArraySize(words));
if ((nwords != ArraySize(words)) || !sameString(words[0], "extFile"))
    errAbort("invalid %s track setting: %s", BASE_COLOR_USE_SEQUENCE,
             seqSource);
return hDnaSeqGet(database, name, words[1], words[2]);
}

static struct dnaSeq *maybeGetSeqUpper(struct linkedFeatures *lf,
				       char *tableName, struct track *tg)
/* Look up the sequence in genbank tables (hGenBankGetMrna also searches 
 * seq if it can't find it in GB tables) or user's blat sequence, 
 * uppercase and return it if we find it, return NULL if we don't find it. */
{
struct dnaSeq *mrnaSeq = NULL;
char *name = getItemDataName(tg, lf->name);
char *seqSource = trackDbSetting(tg->tdb, BASE_COLOR_USE_SEQUENCE);
if (sameString(tableName,"refGene"))
    mrnaSeq = hGenBankGetMrna(database, name, "refMrna");
else if (sameString(seqSource, "ss"))
    mrnaSeq = maybeGetUserSeq(name);
#ifndef GBROWSE
else if (sameString(seqSource, PCR_RESULT_TRACK_NAME))
    mrnaSeq = maybeGetPcrResultSeq(lf);
#endif /* GBROWSE */
else if (startsWith("extFile", seqSource))
    mrnaSeq = maybeGetExtFileSeq(seqSource, name);
else if (endsWith("ExtFile", seqSource))
    mrnaSeq = maybeGetExtFileSeq(seqSource, name);
else if (sameString("nameIsSequence", seqSource))
    {
    mrnaSeq = newDnaSeq(cloneString(name), strlen(name), name);
    if (lf->orientation == -1)
	reverseComplement(mrnaSeq->dna, mrnaSeq->size);
    }
else if (sameString("seq1Seq2", seqSource))
    {
    mrnaSeq = lf->extra;
    if (lf->orientation == -1)
	reverseComplement(mrnaSeq->dna, mrnaSeq->size);
    }
else if (sameString("lfExtra", seqSource))
    {
    mrnaSeq = newDnaSeq(cloneString(lf->extra), strlen(lf->extra), lf->extra);
    if (lf->orientation == -1)
	reverseComplement(mrnaSeq->dna, mrnaSeq->size);
    }
else
    mrnaSeq = hGenBankGetMrna(database, name, NULL);

if (mrnaSeq != NULL)
    touppers(mrnaSeq->dna);
return mrnaSeq;
}



static void makeCdsShades(struct hvGfx *hvg, Color *cdsColor)
/* setup CDS colors */
{
cdsColor[CDS_ERROR] = hvGfxFindColorIx(hvg,0,0,0); 
cdsColor[CDS_ODD] = hvGfxFindColorIx(hvg,CDS_ODD_R,CDS_ODD_G,CDS_ODD_B);
cdsColor[CDS_EVEN] = hvGfxFindColorIx(hvg,CDS_EVEN_R,CDS_EVEN_G,CDS_EVEN_B);
cdsColor[CDS_START] = hvGfxFindColorIx(hvg,CDS_START_R,CDS_START_G,CDS_START_B);
cdsColor[CDS_STOP] = hvGfxFindColorIx(hvg,CDS_STOP_R,CDS_STOP_G,CDS_STOP_B);
cdsColor[CDS_SPLICE] = hvGfxFindColorIx(hvg,CDS_SPLICE_R,CDS_SPLICE_G,
				     CDS_SPLICE_B);
cdsColor[CDS_PARTIAL_CODON] = hvGfxFindColorIx(hvg,CDS_PARTIAL_CODON_R,
					    CDS_PARTIAL_CODON_G, 
					    CDS_PARTIAL_CODON_B);
cdsColor[CDS_QUERY_INSERTION] = hvGfxFindColorIx(hvg,CDS_QUERY_INSERTION_R, 
						CDS_QUERY_INSERTION_G,
						CDS_QUERY_INSERTION_B);
cdsColor[CDS_QUERY_INSERTION_AT_END] = hvGfxFindColorIx(hvg, CDS_QUERY_INSERTION_AT_END_R, 
					      CDS_QUERY_INSERTION_AT_END_G,
					      CDS_QUERY_INSERTION_AT_END_B);
cdsColor[CDS_POLY_A] = hvGfxFindColorIx(hvg,CDS_POLY_A_R,
					    CDS_POLY_A_G, 
					    CDS_POLY_A_B);

cdsColor[CDS_ALT_START] = hvGfxFindColorIx(hvg,CDS_ALT_START_R,
					    CDS_ALT_START_G, 
					    CDS_ALT_START_B);
cdsColor[CDS_SYN_PROT] = hvGfxFindColorIx(hvg,CDS_SYN_PROT_R,
					    CDS_SYN_PROT_G, 
					    CDS_SYN_PROT_B);
cdsColor[CDS_SYN_BLEND] = hvGfxFindColorIx(hvg,CDS_SYN_BLEND_R,
					    CDS_SYN_BLEND_G, 
					    CDS_SYN_BLEND_B);
}



static void updatePartialCodon(char *retStr, int exonStart, int exonEnd, boolean posStrand)
/* Add bases to the appropriate end of retStr, from exonStart until we have 3 bases or
 * run into exonEnd*/
{
char tmpStr[4];
char tmpDna[4];
char *codonDna = getCachedDna(exonStart, exonEnd);
int baseCount = min(3-strlen(retStr), abs(exonEnd-exonStart));
memcpy(tmpDna, codonDna, baseCount);
tmpDna[baseCount] = '\0';
if (posStrand)
    safef(tmpStr, sizeof(tmpStr), "%s%s", retStr, tmpDna);
else
    safef(tmpStr, sizeof(tmpStr), "%s%s", tmpDna, retStr);
memcpy(retStr, tmpStr, 3);
retStr[3] = '\0';
}

struct simpleFeature *baseColorCodonsFromDna(int frame, int chromStart,
	int chromEnd, struct dnaSeq *seq, bool reverse)
/* Create list of codons from a DNA sequence.
   The DNA sequence passed in must include 3 bases extra at the
   start and the end to allow for creating partial codons */
{
struct simpleFeature *sfList = NULL, *sf = NULL;
char codon[4];
int chromPos = chromStart+1;
int i;
DNA *start;
int seqOffset;

if (frame < 0 || frame > 2 || seq == NULL)
    return NULL;

/* pick up bases for partial codon */
seqOffset = frame;
chromPos = chromPos - 3 + frame;

zeroBytes(codon, 4);
for (i = 0, start = seq->dna + seqOffset; i < seq->size; i++, chromPos++)
    {
    int offset = i % 3;
    codon[offset] = *start++;
    if (offset != 2)
        continue;

    /* new codon */
    AllocVar(sf);
    sf->start = chromPos - 3;
    sf->end = sf->start + 3;
    if (reverse)
        {
        sf->start = winEnd - sf->start + winStart - 3;
        sf->end = sf->start + 3;
        }
    // Base offsets mod 6 for alternating colors: 0,1,2 --> first codon, 3,4,5 --> second codon.
    bool codonFirstColor = (sf->start % 6 < 3);
    sf->grayIx = codonToGrayIx(codon, codonFirstColor, NULL, FALSE, TRUE);
    zeroBytes(codon, 4);
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
return sfList;
}

struct simpleFeature *baseColorCodonsFromGenePred(struct linkedFeatures *lf, 
        struct genePred *gp, boolean colorStopStart, boolean codonNumbering)
/* Given an lf and the genePred from which the lf was constructed, 
 * return a list of simpleFeature elements, one per codon (or partial 
 * codon if the codon falls on a gap boundary. */
{
unsigned *starts = gp->exonStarts;
unsigned *ends = gp->exonEnds;
int blockCount = gp->exonCount;
unsigned cdsStart = gp->cdsStart;
unsigned cdsEnd = gp->cdsEnd;
int *exonFrames = gp->exonFrames;
boolean useExonFrames = (gp->optFields >= genePredExonFramesFld);
    int frame = 0;
    int currentStart = 0, currentEnd = 0;
    char partialCodonSeq[4];
    int currentSize;
    int i;

    boolean foundStart = FALSE;
    struct simpleFeature *sfList = NULL, *sf = NULL;

    int i0, iN, iInc;
    boolean posStrand;
    partialCodonSeq[0] = '\0';

    if (lf->orientation > 0) //positive strand
    {
        i0 = 0; iN = blockCount; iInc = 1;
        posStrand = TRUE;
    }
    else
    {
        i0 = blockCount-1; iN=-1; iInc = -1;
        posStrand = FALSE;
    }

    bool altColor = FALSE;
    unsigned cds5Prime = posStrand ? cdsStart : cdsEnd;
    int width = winEnd - winStart;
    // width cutoff really should be based on (a) how many codons this gene has, (2) the current font and (3) image width.
    int codonIndex = !codonNumbering || width > 60 ? 0 : 1;
    for (i=i0; (iInc*i)<(iInc*iN); i=i+iInc)
	{
        int exonStart = starts[i];
        int exonEnd = ends[i];
        if (useExonFrames)
	    {
            if(exonFrames[i] > 0)
                frame = 3 - exonFrames[i];
            else
                frame = 0;
	    }

        if(frame == 0)
            strcpy(partialCodonSeq,"");


        // 3' or 5' UTR exon
        if ((exonStart < cdsStart && exonEnd <= cdsStart) ||
                (exonStart >= cdsEnd && exonEnd > cdsEnd))
	    {
	    if (exonEnd > winStart && exonStart < winEnd)
		{
		AllocVar(sf);
		sf->start = exonStart;
		sf->end = exonEnd; 
		slAddHead(&sfList, sf);
		}
            continue;
	    }
        // UTR + coding exon
        else if (exonEnd > cds5Prime && exonStart < cds5Prime)
	    {
	    int utrStart, utrEnd;
            if (posStrand)
		{
                utrStart = exonStart;
                utrEnd = cdsStart;
                currentStart = cdsStart;
		}
            else
		{
                utrStart = cdsEnd;
                utrEnd = exonEnd;
                currentEnd = cdsEnd;
		}
	    if (utrEnd > winStart && utrStart < winEnd)
		{
		AllocVar(sf);
		sf->start = utrStart;
		sf->end = utrEnd; 
		slAddHead(&sfList, sf);
		}
	    }
        else
            if(posStrand)
                currentStart = exonStart;
            else 
                currentEnd = exonEnd;

	// If UTR + coding, trim to coding portion:
	if (exonStart < cdsStart) exonStart = cdsStart;
	if (exonEnd > cdsEnd) exonEnd = cdsEnd;

        // break each exon into codons and assign alternating shades.
        while (TRUE)
	    {
            int codonInc = frame;
            if (frame == 0)  
		{
                altColor = altColor ? FALSE : TRUE;
		codonInc = 3;
		}

            if(posStrand)
                currentEnd = currentStart + codonInc;
            else
                currentStart = currentEnd - codonInc; 

            //we've gone off the end of the current exon
            if ((posStrand && currentEnd > exonEnd) ||
		(!posStrand && currentStart < exonStart ))
		{
                if (posStrand)
		    {
                    frame = currentEnd - exonEnd;
                    currentEnd = exonEnd;
		    }
                else
		    {
                    frame = exonStart - currentStart;
                    currentStart = exonStart;
		    }
                if(frame == 3) 
		    {
                    frame = 0;
                    break;
		    }
		/* accumulate partial codon in case of one base exon, or start a new one. */
		updatePartialCodon(partialCodonSeq, currentStart, currentEnd, posStrand);
		if (currentStart < winEnd && currentEnd > winStart)
		    {
		    // get next 'frame' nt's to see what codon will be (skipping intron sequence)
		    char theRestOfCodon[4];
		    getNextCodonDna(theRestOfCodon, frame, gp, i, posStrand);
		    /* This code doesn't really work right in all cases of a
		     * one-base blocks. It broke with some TransMap alignments
		     * with indels around the one base.  This code is fragile, so
		     * just work around it by truncating the sequence.
		     */
		    char tempCodonSeq[8];
		    if(posStrand)
			safef(tempCodonSeq, sizeof(tempCodonSeq), "%s%s", partialCodonSeq, 
			      theRestOfCodon);
		    else
			safef(tempCodonSeq, sizeof(tempCodonSeq), "%s%s", theRestOfCodon, 
			      partialCodonSeq );
		    tempCodonSeq[4] = '\0';  // no more than 3 bases

		    AllocVar(sf);
		    sf->start = currentStart;
		    sf->end = currentEnd;
		    sf->grayIx = ((posStrand && currentEnd <= cdsEnd) || 
				  (!posStrand && currentStart >= cdsStart)) ?
			codonToGrayIx(tempCodonSeq, altColor, &foundStart, 
				      !posStrand, colorStopStart) :
			GRAYIX_CDS_ERROR;
                    sf->codonIndex = codonIndex;
		    slAddHead(&sfList, sf);
		    }
                break;
		} // end if we've gone off the end of the current exon

            currentSize = currentEnd - currentStart;
            /*inside a coding block (with 3 bases)*/
            if (currentSize == 3)
		{
		AllocVar(sf);
		sf->start = currentStart;
		sf->end = currentEnd;
		if ((posStrand && currentEnd <= cdsEnd) ||
		    (!posStrand && currentStart >= cdsStart))
		    {
		    char currentCodon[4];
		    char *thisDna = getCachedDna(currentStart, currentEnd);
		    memcpy(currentCodon, thisDna, 3);
		    currentCodon[3] = '\0';
		    sf->grayIx = codonToGrayIx(currentCodon, altColor, &foundStart, 
					       !posStrand, colorStopStart);
		    }
		else
		    sf->grayIx = GRAYIX_CDS_ERROR;
		}
            /*start of a coding block with less than 3 bases*/
            else if (currentSize < 3)
		{
                updatePartialCodon(partialCodonSeq, currentStart, currentEnd, posStrand);
		AllocVar(sf);
		sf->start = currentStart;
		sf->end = currentEnd;
                if (strlen(partialCodonSeq) == 3) 
                    sf->grayIx = codonToGrayIx(partialCodonSeq, altColor,
                            &foundStart, !posStrand, colorStopStart);
                else
                    sf->grayIx = GRAYIX_CDS_ERROR;
                strcpy(partialCodonSeq,"" );

                /*update frame based on bases appended*/
                frame -= currentSize;
		}
            else
                errAbort("%s: Too much dna (%d - %d = %d)<br>\n", lf->name, 
			 currentEnd, currentStart, currentSize);

            if (codonIndex)
                sf->codonIndex = codonIndex++;
            slAddHead(&sfList, sf);
            if(posStrand)
                currentStart = currentEnd;
            else
                currentEnd = currentStart;
	    } // end loop on codons within exon

	/* coding + UTR exon */
	if (posStrand && (exonEnd < ends[i]) &&
	    exonEnd < winEnd && ends[i] > winStart)
	    {
            AllocVar(sf);
            sf->start = exonEnd;
            sf->end = ends[i];
            slAddHead(&sfList, sf);
	    }
	else if (!posStrand && (exonStart > starts[i]) &&
		 starts[i] < winEnd && exonStart > winStart)
	    {
            AllocVar(sf);
            sf->start = starts[i];
            sf->end = exonStart;
            slAddHead(&sfList, sf);
	    }
	} // end loop on exons

    if(posStrand)
        slReverse(&sfList);
    return(sfList);
}


static void getMrnaBases(struct psl *psl, struct dnaSeq *mrnaSeq,
			 int mrnaS, int s, int e, boolean isRc,
			 char retMrnaBases[4], boolean *retQueryInsertion)
/* Get mRNA bases for the current mRNA codon triplet.  If this is a split
 * codon, retrieve the adjacent mRNA bases to make a full triplet. */
{
int size = e - s;
if(size < 3)
    {
    if (mrnaSeq != NULL && mrnaS-(3-size) > 0)
        {
	int i=0;
	int idx = -1;
	int newIdx = -1;
	unsigned *gaps = NULL;
	boolean appendAtStart = FALSE;
	AllocArray(gaps, psl->blockCount+2);

	for(i=0; i<psl->blockCount; i++)
	    {
	    unsigned tStart = psl->tStarts[i];
	    unsigned tEnd = tStart + psl->blockSizes[i];
	    if (psl->strand[1] == '-') 
		reverseUnsignedRange(&tStart, &tEnd, psl->tSize);

	    if (s == tStart)
		{
		idx = i;
		appendAtStart = TRUE;
		break;
		}
	    else if (e == tEnd)
		{
		idx = i+1;
		appendAtStart = FALSE;
		break;
		}
	    }

	getHiddenGaps(psl, gaps);
	if(idx >= 0 && gaps[idx] > 0 && retQueryInsertion != NULL)
	    *retQueryInsertion = TRUE;

	if (!appendAtStart)
            {
	    newIdx = mrnaS + size;
	    memcpy(retMrnaBases, &mrnaSeq->dna[mrnaS], size);
	    memcpy(retMrnaBases+size, &mrnaSeq->dna[newIdx], 3-size);
            }
	else
            {
	    newIdx = mrnaS - (3 - size);
	    memcpy(retMrnaBases, &mrnaSeq->dna[newIdx], 3);
            }
        }
    else
	{
	memcpy(retMrnaBases, "NNN", 3);
	}
    }
else
    memcpy(retMrnaBases, &mrnaSeq->dna[mrnaS], 3);
retMrnaBases[3] = '\0';
if (isRc)
    reverseComplement(retMrnaBases, strlen(retMrnaBases));
}

static void drawDiffTextBox(struct hvGfx *hvg, int xOff, int y, 
        double scale, int heightPer, MgFont *font, Color color, 
        char *chrom, unsigned s, unsigned e, struct simpleFeature *sf, struct psl *psl, 
        struct dnaSeq *mrnaSeq, struct linkedFeatures *lf,
        int grayIx, enum baseColorDrawOpt drawOpt,
        int maxPixels, Color *trackColors, Color ixColor)
{
int mrnaS = -1;
if (psl)
    mrnaS = convertCoordUsingPsl( s, psl ); 
else if (sf)
    mrnaS = sf->qStart;
if(mrnaS >= 0)
    {
    struct dyString *dyMrnaSeq = newDyString(256);
    char mrnaBases[4];
    char genomicCodon[2];
    char mrnaCodon[2]; 
    boolean queryInsertion = FALSE;
    boolean isCoding = (drawOpt == baseColorDrawItemCodons || drawOpt == baseColorDrawDiffCodons);

    mrnaBases[0] = '\0';
    if (psl && isCoding)
	getMrnaBases(psl, mrnaSeq, mrnaS, s, e, (lf->orientation == -1),
		    mrnaBases, &queryInsertion);
    if (queryInsertion && isCoding)
	color = cdsColor[CDS_QUERY_INSERTION];

    dyStringAppendN(dyMrnaSeq, (char*)&mrnaSeq->dna[mrnaS], e-s);

    if (drawOpt == baseColorDrawItemBases)
	{
	if (cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE))
	    complement(dyMrnaSeq->string, dyMrnaSeq->stringSize);
	drawScaledBoxSampleWithText(hvg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score, font, dyMrnaSeq->string,
				    zoomedToBaseLevel, winStart, maxPixels, isCoding, TRUE);
	}
    else if (drawOpt == baseColorDrawItemCodons)
	{
	if (e <= lf->tallEnd)
	    {
	    boolean startColor = FALSE;
	    /* re-set color of this block based on mrna codons rather than
	     * genomic, but keep the odd/even cycle of dark/light shades. */
	    int mrnaGrayIx = codonToGrayIx(mrnaBases, (grayIx > 26), NULL,
					   FALSE, TRUE);
	    if (color == cdsColor[CDS_START])
                startColor = TRUE;
	    color = colorAndCodonFromGrayIx(hvg, mrnaCodon, mrnaGrayIx,
					    ixColor);
	    if (startColor && sameString(mrnaCodon,"M"))
                color = cdsColor[CDS_START];
	    drawScaledBoxSampleWithText(hvg, s, e, scale, xOff, y, heightPer, 
					color, lf->score, font, mrnaCodon,
					zoomedToCodonLevel, winStart,
					maxPixels, isCoding, TRUE);
	    }
	else
	    drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, color);
	}
    else if (drawOpt == baseColorDrawDiffBases)
	{
	char *diffStr = NULL;
	char *genoDna = getCachedDna(s, e);
	diffStr = needMem(sizeof(char) * (e - s + 1));
	maskDiffString(diffStr, dyMrnaSeq->string, genoDna, ' ');
	if (cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE))
	    complement(diffStr, strlen(diffStr));
	drawScaledBoxSampleWithText(hvg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score, font, diffStr, 
				    zoomedToBaseLevel, winStart, maxPixels, isCoding, TRUE);
	freeMem(diffStr);
	}
    else if (drawOpt == baseColorDrawDiffCodons)
	{
	if (e <= lf->tallEnd)
	    {
	    /* Color codons red wherever mrna differs from genomic;
	     * keep the odd/even cycle of dark/light shades. */
	    colorAndCodonFromGrayIx(hvg, genomicCodon, grayIx, ixColor);
	    int mrnaGrayIx = mrnaCodonToGrayIx(mrnaBases, genomicCodon[0],
					       (grayIx > 26));
	    color = colorAndCodonFromGrayIx(hvg, mrnaCodon, mrnaGrayIx,
					    ixColor);
	    // Look up mrnaCodon again because if mrnaGrayIx is GRAYIX_SYN_PROT,
	    // codon value is lost:
	    safef(mrnaCodon, sizeof(mrnaCodon), "%c", baseColorLookupCodon(mrnaBases));
	    if (mrnaCodon[0] != genomicCodon[0])
		{
		drawScaledBoxSampleWithText(hvg, s, e, scale, xOff, y, 
					    heightPer, color, lf->score, font,
					    mrnaCodon, zoomedToCodonLevel,
					    winStart, maxPixels, isCoding, TRUE);
		}
	    else
		drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, color);
	    }
	else
	    drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, color);
	}
    else if (drawOpt != baseColorDrawCds)
        errAbort("Unknown drawOpt: %d<br>\n", drawOpt);

    dyStringFree(&dyMrnaSeq);
    }
else
    {
    /*show we have an error by coloring entire exon block yellow*/
    drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, MG_YELLOW);
    // FIXME: this shouldn't ever happen, should be an errAbort
    warn("Bug: drawDiffTextBox: convertCoordUsingPsl failed<br>\n");
    }
}

void baseColorDrawItem(struct track *tg,  struct linkedFeatures *lf, 
		       int grayIx, struct hvGfx *hvg, int xOff, 
                       int y, double scale, MgFont *font, int s, int e, 
                       int heightPer, boolean zoomedToCodonLevel, 
                       struct dnaSeq *mrnaSeq, struct simpleFeature *sf, struct psl *psl, 
		       enum baseColorDrawOpt drawOpt,
                       int maxPixels, int winStart, 
                       Color originalColor)
/* Draw codon/base-colored item. */
{
char codon[64] = " ";
Color color = colorAndCodonFromGrayIx(hvg, codon, grayIx, originalColor);
if (sf->codonIndex)
    safef(codon, sizeof(codon), "%c %d", codon[0], sf->codonIndex);
/* When we are zoomed out far enough so that multiple bases/codons share the 
 * same pixel, we have to draw differences in a separate pass (baseColorOverdrawDiff)
 * so don't waste time drawing the differences here: */
boolean zoomedOutToPostProcessing =
    ((drawOpt == baseColorDrawDiffBases && !zoomedToBaseLevel) ||
     (drawOpt == baseColorDrawDiffCodons && !zoomedToCdsColorLevel));

if (drawOpt == baseColorDrawGenomicCodons && (e-s <= 3))
    {
    if (lf->highlightColor)
	{
	drawScaledBoxSample(hvg, s, e, scale, xOff, y, heightPer, 
			    lf->highlightColor, lf->score );
	drawScaledBoxSampleWithText(hvg, s, e, scale, xOff, y+1, heightPer-2, 
				    color, lf->score, font, codon, 
				    zoomedToCodonLevel, winStart, maxPixels, TRUE, !sf->codonIndex);
	}
    else
	{
	drawScaledBoxSampleWithText(hvg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score, font, codon, 
				    zoomedToCodonLevel, winStart, maxPixels, TRUE, !sf->codonIndex);
	}
    }
else if (mrnaSeq != NULL && (psl != NULL || sf != NULL) && !zoomedOutToPostProcessing &&
	 drawOpt != baseColorDrawGenomicCodons && drawOpt != baseColorDrawOff)
    {
    if (lf->highlightColor)
	{
	drawScaledBoxSample(hvg, s, e, scale, xOff, y, heightPer, 
			    lf->highlightColor, lf->score );
	drawDiffTextBox(hvg, xOff+1, y+1, scale, heightPer-2, font, 
			color, chromName, s, e, sf, psl, mrnaSeq, lf,
			grayIx, drawOpt, maxPixels,
			tg->colorShades, originalColor);
	}
    else
	{
	drawDiffTextBox(hvg, xOff, y, scale, heightPer, font, 
			color, chromName, s, e, sf, psl, mrnaSeq, lf,
			grayIx, drawOpt, maxPixels,
			tg->colorShades, originalColor);
	}
    }
else
    {
    /* revert to normal coloring */
    if (lf->highlightColor)
	{
	drawScaledBoxSample(hvg, s, e, scale, xOff, y, heightPer, 
			    lf->highlightColor, lf->score );
	drawScaledBoxSample(hvg, s, e, scale, xOff+1, y+1, heightPer -2, 
			    color, lf->score );
	}
    else
	{
	drawScaledBoxSample(hvg, s, e, scale, xOff, y, heightPer, 
			    color, lf->score );
	}
    }
}


static void drawCdsDiffCodonsOnly(struct track *tg,  struct linkedFeatures *lf,
			   struct hvGfx *hvg, int xOff,
			   int y, double scale, int heightPer,
			   struct dnaSeq *mrnaSeq, struct psl *psl,
			   int winStart)
/* Draw red boxes only where mRNA codons differ from genomic.  This assumes
 * that lf has been drawn already, we're zoomed out past zoomedToCdsColorLevel,
 * we're not in dense mode etc. */
{
struct simpleFeature *sf = NULL;
Color dummyColor = 0;

if (lf->codons == NULL)
    errAbort("drawCdsDiffCodonsOnly: lf->codons is NULL");

for (sf = lf->codons; sf != NULL; sf = sf->next)
    {
    int s = sf->start;
    int e = sf->end;
    if (s < lf->tallStart)
	s = lf->tallStart;
    if (e > lf->tallEnd)
	e = lf->tallEnd;
    if (s > winEnd || e < winStart)
      continue;
    if (e > s)
	{
	int mrnaS = convertCoordUsingPsl( s, psl ); 
	if (mrnaS >= 0)
	    {
	    char mrnaBases[4];
	    char genomicCodon[2], mrnaCodon;
	    boolean queryInsertion = FALSE;
	    Color color = cdsColor[CDS_STOP];
	    getMrnaBases(psl, mrnaSeq, mrnaS, s, e, (lf->orientation == -1),
			 mrnaBases, &queryInsertion);
	    if (queryInsertion)
		color = cdsColor[CDS_QUERY_INSERTION];
	    mrnaCodon = baseColorLookupCodon(mrnaBases);
	    colorAndCodonFromGrayIx(hvg, genomicCodon, sf->grayIx, dummyColor);
	    if (queryInsertion)
		drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, color);
            if (mrnaCodon != genomicCodon[0])
                {
                if (mrnaCodon != genomicCodon[0] && protEquivalent(genomicCodon[0], mrnaCodon))
                    color = cdsColor[CDS_SYN_PROT];
#ifdef COLOR32
                /* this was a call to drawScaledBoxBlend, but this breaks under
                 * 32-bit color, so for the moment we're going to depend 
                 * on the painter's algorithm */
		drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, color);
#else
		drawScaledBoxBlend(hvg, s, e, scale, xOff, y, heightPer, color);
#endif
                }
	    }
	else
	    {
	    /*show we have an error by coloring entire exon block yellow*/
	    drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, MG_YELLOW);
            // FIXME: this shouldn't ever happen, should be an errAbort
            warn("Bug: drawCdsDiffCodonsOnly: convertCoordUsingPsl failed<br>\n");
	    }
	}
    }
}

void baseColorOverdrawDiff(struct track *tg,  struct linkedFeatures *lf,
			   struct hvGfx *hvg, int xOff,
			   int y, double scale, int heightPer,
			   struct dnaSeq *mrnaSeq, struct psl *psl,
			   int winStart, enum baseColorDrawOpt drawOpt)
/* If we're drawing different bases/codons, and zoomed out past base/codon 
 * level, draw 1-pixel wide red lines only where bases/codons differ from 
 * genomic.  This tests drawing mode and zoom level but assumes that lf itself 
 * has been drawn already and we're not in dense mode etc. */
{
boolean enabled = TRUE;

// check max zoom
float showDiffBasesMaxZoom = trackDbFloatSettingOrDefault(tg->tdb, "showDiffBasesMaxZoom", -1.0);
if ((showDiffBasesMaxZoom >= 0.0)
    && ((basesPerPixel > showDiffBasesMaxZoom) || (showDiffBasesMaxZoom == 0.0)))
    enabled = FALSE;

if (drawOpt == baseColorDrawDiffCodons && !zoomedToCdsColorLevel && lf->codons && enabled)
    {
    drawCdsDiffCodonsOnly(tg, lf, hvg, xOff, y, scale,
			  heightPer, mrnaSeq, psl, winStart);
    }
if (drawOpt == baseColorDrawDiffBases && !zoomedToBaseLevel && enabled)
    {
    drawCdsDiffBaseTickmarksOnly(tg, lf, hvg, xOff, y, scale,
				 heightPer, mrnaSeq, psl, winStart);
    }
}


void baseColorOverdrawQInsert(struct track *tg,  struct linkedFeatures *lf,
			      struct hvGfx *hvg, int xOff,
			      int y, double scale, int heightPer,
			      struct dnaSeq *mrnaSeq, struct psl *psl,
			      int winStart, enum baseColorDrawOpt drawOpt,
			      boolean indelShowQInsert, boolean indelShowPolyA)
/* If applicable, draw 1-pixel wide orange lines for query insertions in the
 * middle of the query, 1-pixel wide blue lines for query insertions at the 
 * end of the query, and 1-pixel wide green (instead of blue) when a query 
 * insertion at the end is a valid poly-A tail. */
{
assert(psl);
int i;
int s;
int lastBlk = psl->blockCount - 1;
boolean gotPolyAStart=FALSE, gotPolyAEnd=FALSE;

if (indelShowPolyA && mrnaSeq)
    {
    /* Draw green lines for polyA first, so if the entire transcript is 
     * jammed into one pixel and the other end has a blue line, blue is 
     * what the user sees. */
    if (psl->qStarts[0] != 0 && psl->strand[0] == '-')
	{
	/* Query is -.  We reverse-complemented in baseColorDrawSetup,
	 * so test for polyT head: */
	int polyTSize = headPolyTSizeLoose(mrnaSeq->dna, mrnaSeq->size);
	if (polyTSize > 0 && (polyTSize + 3) >= psl->qStarts[0])
	    {
	    if (psl->strand[1] == '-')
		s = psl->tSize - psl->tStarts[0] - 1;
	    else
		s = psl->tStarts[0];
	    drawVertLine(lf, hvg, s, xOff, y, heightPer-1, scale,
			 cdsColor[CDS_POLY_A]);
	    gotPolyAStart = TRUE;
	    }
	}
    if ((psl->qStarts[lastBlk] + psl->blockSizes[lastBlk] != psl->qSize) &&
	psl->strand[0] == '+')
	{
	if (psl->strand[1] == '-')
	    {
	    /* Query is + but target is -.  We reverse-complemented in
	     * baseColorDrawSetup, so test for polyT head: */
	    int polyTSize = headPolyTSizeLoose(mrnaSeq->dna, mrnaSeq->size);
	    int rcQStart = (psl->qSize -
			(psl->qStarts[lastBlk] + psl->blockSizes[lastBlk]));
	    if (polyTSize > 0 && (polyTSize + 3) >= rcQStart)
		{
		s = psl->tStart;
		drawVertLine(lf, hvg, s, xOff, y, heightPer-1, scale,
			     cdsColor[CDS_POLY_A]);
		gotPolyAEnd = TRUE;
		}
	    }
	else
	    {
	    /* Both are +.  We didn't reverse-complement in
	     * baseColorDrawSetup, so test for polyA tail: */
	    int polyASize = tailPolyASizeLoose(mrnaSeq->dna, mrnaSeq->size);
	    if (polyASize > 0 &&
		((polyASize + 3) >= 
		 (psl->qSize -
		  (psl->qStarts[lastBlk] + psl->blockSizes[lastBlk]))))
		{
		s = psl->tStarts[lastBlk] + psl->blockSizes[lastBlk];
		drawVertLine(lf, hvg, s, xOff, y, heightPer-1, scale,
			     cdsColor[CDS_POLY_A]);
		gotPolyAEnd = TRUE;
		}
	    }
	}
    }

if (indelShowQInsert)
    {
    if (psl->qStarts[0] != 0 && !gotPolyAStart)
	{
	/* Insert at beginning of query -- draw vertical blue line 
	 * unless it's polyA. */
	s = (psl->strand[1] == '-') ? (psl->tSize - psl->tStarts[0] - 1) :
				      psl->tStarts[0];
	drawVertLine(lf, hvg, s, xOff, y, heightPer-1, scale,
		     cdsColor[CDS_QUERY_INSERTION_AT_END]);
	}
    for (i = 1;  i < psl->blockCount;  i++)
	{
	int qBlkStart = psl->qStarts[i];
	int qPrevBlkEnd = (psl->qStarts[i-1] + psl->blockSizes[i-1]);
	if (qBlkStart > qPrevBlkEnd)
	    {
	    int tBlkStart = psl->tStarts[i];
	    int tPrevBlkEnd = (psl->tStarts[i-1] + psl->blockSizes[i-1]);
	    /* Note: if tBlkStart < tPrevBlkEnd, then we have overlap on 
	     * target, possibly indicating a bug in the aligner. */
	    if (tBlkStart <= tPrevBlkEnd)
		{
		/* Insert in query only -- draw vertical orange line. */
		s = (psl->strand[1] == '-') ? (psl->tSize - psl->tStarts[i] - 1) :
					      psl->tStarts[i];
		drawVertLine(lf, hvg, s, xOff, y, heightPer-1, scale,
			     cdsColor[CDS_QUERY_INSERTION]);
		}
	    }
	/* Note: if qBlkStart < qPrevBlkEnd, then we have overlap on query,
	 * possibly indicating a bug in the aligner.  Most likely a gap 
	 * should be drawn in that case (really a target insert) but I don't
	 * think this is the place to do it.  Should be caught by 
	 * pre-screening table data for block coords that overlap. */
	}
    if (psl->qStarts[lastBlk] + psl->blockSizes[lastBlk] != psl->qSize &&
	!gotPolyAEnd)
	{
	/* Insert at end of query -- draw vertical blue line unless it's 
	 * all polyA. */
	s = (psl->strand[1] == '-') ?
	    (psl->tSize - (psl->tStarts[lastBlk] + psl->blockSizes[lastBlk])) :
	    (psl->tStarts[lastBlk] + psl->blockSizes[lastBlk]);
	drawVertLine(lf, hvg, s, xOff, y, heightPer-1, scale,
		     cdsColor[CDS_QUERY_INSERTION_AT_END]);
	}
    }
}

void baseColorInitTrack(struct hvGfx *hvg, struct track *tg)
/* Set up base coloring state (e.g. cache genomic sequence) for tg.
 * This must be called by tg->drawItems if baseColorDrawSetup is used 
 * in tg->drawItemAt.  Uses tg->drawItems method to determine whether
 * tg is linkedFeatures or linkedFeaturesSeries (currently the only
 * two supported track types -- bed, psl etc. are subclasses of these). */
{
if (initedTrack == NULL || differentString(tg->track, initedTrack))
    {
    int overallStart, overallEnd;
    boolean isSeries = FALSE;
    if ((tg->drawItems == linkedFeaturesSeriesDraw)
#ifdef USE_BAM
        || (tg->drawItems == bamLinkedFeaturesSeriesDraw))
#else
	)
#endif
	isSeries = TRUE;
    else if (!baseColorCanDraw(tg))
	errAbort("baseColorInitTrack: track %s has a type not recognized by baseColorCanDraw.",
		 tg->track);
    getLinkedFeaturesSpan((struct linkedFeatures *)tg->items, &overallStart, &overallEnd,
			  isSeries);
    if (overallStart < cachedGenoStart || overallEnd > cachedGenoEnd)
	{
	// leak mem to save time (don't bother freeing old cached dna)
	cachedGenoStart = overallStart;
	cachedGenoEnd = overallEnd;
	cachedGenoDna = hDnaFromSeq(database, chromName, cachedGenoStart, cachedGenoEnd, dnaUpper);
	}
    initedTrack = cloneString(tg->track);
    }

/* allocate colors for coding coloring */
if (!cdsColorsMade)
    { 
    makeCdsShades(hvg, cdsColor);
    cdsColorsMade = TRUE;
    }
}

static void checkTrackInited(struct track *tg, char *what)
/* Die if baseColorInitTrack has not been called (most recently) for this track. */
{
if (initedTrack == NULL || differentString(tg->track, initedTrack))
    errAbort("Error: Track %s should have been baseColorInitTrack'd before %s.  "
	     "(tg->drawItems may be unrecognized by baseColorCanDraw)",
	     tg->track, what);
}

enum baseColorDrawOpt baseColorDrawSetup(struct hvGfx *hvg, struct track *tg,
			struct linkedFeatures *lf,
			struct dnaSeq **retMrnaSeq, struct psl **retPsl)
/* Returns the CDS coloring option, allocates colors if necessary, and 
 * returns the sequence and psl record for the given item if applicable. 
 * Note: even if base coloring is not enabled, this will return psl and 
 * mrna seq if query insert/polyA coloring is enabled.
 * baseColorInitTrack must be called before this (in tg->drawItems) --
 * this is meant to be called by tg->drawItemAt (i.e. linkedFeaturesDrawAt). */
{
enum baseColorDrawOpt drawOpt = baseColorGetDrawOpt(tg);
boolean indelShowDoubleInsert, indelShowQueryInsert, indelShowPolyA;
indelEnabled(cart, (tg ? tg->tdb : NULL), basesPerPixel,
	     &indelShowDoubleInsert, &indelShowQueryInsert, &indelShowPolyA);

if (drawOpt <= baseColorDrawOff && !(indelShowQueryInsert || indelShowPolyA))
    return drawOpt;

checkTrackInited(tg, "calling baseColorDrawSetup");

/* If we are using item sequence, fetch alignment and sequence: */
if ((drawOpt > baseColorDrawOff && startsWith("psl", tg->tdb->type)) ||
    indelShowQueryInsert || indelShowPolyA)
    {
    *retPsl = (struct psl *)(lf->original);
    if (*retPsl == NULL)
	return baseColorDrawOff;
    }
if (drawOpt == baseColorDrawItemBases ||
    drawOpt == baseColorDrawDiffBases ||
    drawOpt == baseColorDrawItemCodons ||
    drawOpt == baseColorDrawDiffCodons ||
    indelShowPolyA)
    {
    *retMrnaSeq = maybeGetSeqUpper(lf, tg->table, tg);
    if (*retMrnaSeq != NULL && *retPsl != NULL) // we have both sequence and PSL
	{
        if ((*retMrnaSeq)->size != (*retPsl)->qSize)
            errAbort("baseColorDrawSetup: %s: mRNA size (%d) != psl qSize (%d)",
                     (*retPsl)->qName, (*retMrnaSeq)->size, (*retPsl)->qSize);
	if ((*retPsl)->strand[0] == '-' || (*retPsl)->strand[1] == '-')
	    reverseComplement((*retMrnaSeq)->dna, strlen((*retMrnaSeq)->dna));
	}
    // if no sequence, no base color drawing
    // Note: we could have sequence but no PSL (eg, tagAlign format)
    else if (*retMrnaSeq == NULL) 
	return baseColorDrawOff;
    }

return drawOpt;
}

void baseColorDrawRulerCodons(struct hvGfx *hvg, struct simpleFeature *sfList,
                double scale, int xOff, int y, int height, MgFont *font, 
                int winStart, int maxPixels, bool zoomedToText)
/* Draw amino acid translation of genomic sequence based on a list
   of codons. Used for browser ruler in full mode*/
{
struct simpleFeature *sf;

if (!cdsColorsMade)
    {
    makeCdsShades(hvg, cdsColor);
    cdsColorsMade = TRUE;
    }

for (sf = sfList; sf != NULL; sf = sf->next)
    {
    char codon[4];
    Color color = colorAndCodonFromGrayIx(hvg, codon, sf->grayIx, MG_GRAY);
    if (zoomedToText)
        drawScaledBoxSampleWithText(hvg, sf->start, sf->end, scale, insideX, y,
				    height, color, 1.0, font, codon, TRUE,
				    winStart, maxPixels, TRUE, TRUE);
    else
        /* zoomed in just enough to see colored boxes */
        drawScaledBox(hvg, sf->start, sf->end, scale, xOff, y, height, color);
    }
}


void baseColorDrawCleanup(struct linkedFeatures *lf, struct dnaSeq **pMrnaSeq,
			  struct psl **pPsl)
/* Free structures allocated just for base/cds coloring. */
{
// We could free lf->original here (either genePredFree or pslFree, depending
// on the type -- but save time by skipping that.  Maybe we should save time
// by skipping this free too:
if (pMrnaSeq != NULL)
    dnaSeqFree(pMrnaSeq);
}

void baseColorSetCdsBounds(struct linkedFeatures *lf, struct psl *psl,
                           struct track *tg)
/* set CDS bounds in linked features for a PSL.  Used when zoomed out too far
 * for codon or base coloring, but still want to render CDS bounds */
{
struct genbankCds cds;
getPslCds(psl, tg, &cds);
if (cds.start < cds.end)
    {
    struct genbankCds genomeCds = genbankCdsToGenome(&cds, psl);
    if (genomeCds.start < genomeCds.end)
        {
        lf->tallStart = genomeCds.start;
        lf->tallEnd = genomeCds.end;
        }
    }
}


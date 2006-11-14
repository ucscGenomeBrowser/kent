/* cds.c - code for coloring of bases, codons, or alignment differences. */
#include "common.h"
#include "hCommon.h"
#include "hash.h"
#include "jksql.h"
#include "memalloc.h"
#include "dystring.h"
#include "memgfx.h"
#include "vGfx.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "hdb.h"
#include "psl.h"
#include "genePred.h"
#include "cds.h"
#include "genbank.h"
#include "hgTracks.h"

static char const rcsid[] = "$Id: cds.c,v 1.49 2006/11/14 00:30:24 angie Exp $";

/* Definitions of cds colors for coding coloring display */
#define CDS_ERROR   0

#define CDS_ODD     1
#define	CDS_ODD_R	0x00
#define	CDS_ODD_G	0x00
#define	CDS_ODD_B	0x9e

#define CDS_EVEN    2
#define	CDS_EVEN_R	0x00
#define	CDS_EVEN_G	0x00
#define	CDS_EVEN_B	0xdc

#define CDS_START   3
#define	CDS_START_R	0x00
#define	CDS_START_G	0xf0
#define	CDS_START_B	0x00

#define CDS_STOP    4
#define	CDS_STOP_R	0xe1
#define	CDS_STOP_G	0x00
#define	CDS_STOP_B	0x00

#define CDS_SPLICE      5
#define	CDS_SPLICE_R	0xa0
#define	CDS_SPLICE_G	0xa0
#define	CDS_SPLICE_B	0xd9

#define CDS_PARTIAL_CODON	6
#define CDS_PARTIAL_CODON_R	0x0
#define CDS_PARTIAL_CODON_G 0xc0
#define CDS_PARTIAL_CODON_B	0xc0

#define CDS_GENOMIC_INSERTION   7
#define CDS_GENOMIC_INSERTION_R 220
#define CDS_GENOMIC_INSERTION_G 128
#define CDS_GENOMIC_INSERTION_B 0

#define CDS_NUM_COLORS 8

/* Array of colors used in drawing codons/bases/differences: */
Color cdsColor[CDS_NUM_COLORS];
boolean cdsColorsMade = FALSE;


static void drawScaledBoxSampleWithText(struct vGfx *vg, 
                                        int chromStart, int chromEnd,
                                        double scale, int xOff, int y,
                                        int height, Color color, int score,
                                        MgFont *font, char *text, bool zoomed,
                                        int winStart, int maxPixels)
/* Draw a box scaled from chromosome to window coordinates with
   a codon or set of 3 or less bases drawn in the box. */
{

/*first draw the box itself*/
drawScaledBoxSample(vg, chromStart, chromEnd, scale, xOff, y, height, 
		    color, score);

/*draw text in box if space, and align properly for codons or DNA*/
if (zoomed)
    {
    int i;
    int x1, x2, w;
    x1 = round((double)(chromStart-winStart)*scale) + xOff;
    x2 = round((double)(chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    if (w < 1)
        w = 1;
    if (x2 >= maxPixels)
        x2 = maxPixels - 1;
    w = x2-x1;
    if (w < 1)
        w = 1;

    if (chromEnd - chromStart == 3)
        spreadBasesString(vg,x1,y,w,height,whiteIndex(),
		     font,text,strlen(text), TRUE);
    else if (chromEnd - chromStart < 3)
        spreadBasesString(vg,x1,y,w,height,cdsColor[CDS_PARTIAL_CODON],font,
		     text,strlen(text), TRUE);
    else
        {
        int thisX,thisX2;
        char c[2];
        for (i=0; i<strlen(text); i++)
            {
            sprintf(c,"%c",text[i]);
            thisX = round((double)(chromStart+i-winStart)*scale) + xOff;
            thisX2 = round((double)(chromStart+1+i-winStart)*scale) + xOff;
            vgTextCentered(vg,thisX+(thisX2-thisX)/2.0,y,1,height,
			   whiteIndex(),font,c);
            }
        }
    }
}

static int convertCoordUsingPsl( int s, struct psl *psl )
/*return query position corresponding to target position in one 
 * of the coding blocks of a psl file, or return -1 if target position
 * is not in a coding exon of this psl entry or if the psl entry is
 * null*/
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
         reverseIntRange(&tStart, &tEnd, psl->tSize);

    if (s >= tStart-3 && s < tEnd)
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


static void getCodonDna(char *retStr, char *chrom, int n, unsigned *exonStarts,
			unsigned *exonEnds, int exonCount, unsigned cdsStart, 
			unsigned cdsEnd, int startI, boolean reverse, 
			struct dnaSeq *mrnaSeq)
//get n bases from coding exons only.
{
int i, j, thisN;
int theStart, theEnd;

//clear retStr
sprintf(retStr,"   ");

if (!reverse)  /*positive strand*/
    {
   
    thisN = 0;
    //move to next block start
    i = startI+1;
    while (thisN < n)
	{
        struct dnaSeq *codonDna = NULL;

	/*already on last block*/
	if (i >= exonCount) 
	    break;

        //get dna for this exon
        codonDna = hDnaFromSeq( chrom, exonStarts[i], exonEnds[i], dnaUpper );

	theStart = exonStarts[i];
	theEnd = exonEnds[i];
	if (cdsStart > 0 && theStart < cdsStart)
	    theStart = cdsStart;

	if (cdsEnd > 0 && theEnd > cdsEnd)
	    theEnd = cdsEnd;
           
	for (j=0; j<(theEnd - theStart); j++)
	    {
	    if (mrnaSeq != NULL)
		retStr[thisN] = mrnaSeq->dna[theStart+j];
	    else
                retStr[thisN] = codonDna->dna[j+theStart-exonStarts[i]];

	    thisN++;
	    if (thisN >= n) 
		break;
	    }

	i++;
	}
    retStr[thisN] = '\0';
       
    }
else   /*negative strand*/
    {
    retStr[n] = '\0';
    thisN = n-1;
    //move to previous block end
    i = startI-1;
    while(thisN >= 0)
	{
        struct dnaSeq *codonDna = NULL;

	if (i < 0)
	    break;

        //get dna for this exon
        codonDna = hDnaFromSeq( chrom, exonStarts[i], exonEnds[i], dnaUpper );

	theStart = exonStarts[i];
	theEnd = exonEnds[i];
	if (theStart < cdsStart)
	    theStart = cdsStart;

	if (theEnd > cdsEnd)
	    theEnd = cdsEnd;
           
	for (j=0; j<(theEnd - theStart); j++)
	    {
	    if (mrnaSeq != NULL)
		retStr[thisN] = mrnaSeq->dna[theEnd-j-1];
	    else
                retStr[thisN] = codonDna->dna[theEnd-j-1-exonStarts[i]];

	    thisN--;
	    if (thisN < 0) 
		break;
	    }
	i--;
	}
    }
}


static void drawCdsDiffBaseTickmarksOnly(struct track *tg,
	struct linkedFeatures *lf,
	struct vGfx *vg, int xOff,
	int y, double scale, int heightPer,
	struct dnaSeq *mrnaSeq, struct psl *psl,
	int winStart)
/* Draw 1-pixel wide red lines only where mRNA bases differ from genomic.  
 * This assumes that lf has been drawn already, we're zoomed out past 
 * zoomedToBaseLevel, we're not in dense mode etc. */
{
struct simpleFeature *sf = NULL;
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    int s = sf->start;
    int e = sf->end;
    if (s > winEnd || e < winStart)
      continue;
    if (e > s)
	{
	int mrnaS = convertCoordUsingPsl(s, psl);
	if(mrnaS >= 0)
	    {
	    struct dnaSeq *genoSeq = hDnaFromSeq(chromName, s, e, dnaUpper);
	    int i;
	    for (i=0; i < (e - s); i++)
		{
		if (mrnaSeq->dna[mrnaS+i] != genoSeq->dna[i])
		    {
		    int thisX = round((double)(s+i-winStart)*scale) + xOff;
		    vgLine(vg, thisX, y+1, thisX, y+heightPer-2,
			   cdsColor[CDS_STOP]);
		    }
		}
	    dnaSeqFree(&genoSeq);
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

Color lighterShade(struct vGfx *vg, Color color, double percentLess)
    /* Get lighter shade of a color, with a variable level */ 
{
    struct rgbColor rgbColor =  vgColorIxToRgb(vg, color); 
    rgbColor.r = (int)((rgbColor.r+127)/percentLess);
    rgbColor.g = (int)((rgbColor.g+127)/percentLess);
    rgbColor.b = (int)((rgbColor.b+127)/percentLess);
    return vgFindColorIx(vg, rgbColor.r, rgbColor.g, rgbColor.b);
}



static Color colorAndCodonFromGrayIx(struct vGfx *vg, char *codon, int grayIx, 
			      Color ixColor)
/*convert grayIx value to color and codon which
 * are both encoded in the grayIx*/
{
Color color;
if (grayIx == -2)
    {
    color = cdsColor[CDS_ERROR];
    sprintf(codon,"X");
    }
else if (grayIx == -1)
    {
    color = cdsColor[CDS_START];
    sprintf(codon,"M");
    }
else if (grayIx == -3)
    {
    color = cdsColor[CDS_STOP];
    sprintf(codon,"*");
    }
else if (grayIx <= 26)
    {
    color = ixColor;
    sprintf(codon,"%c",grayIx + 'A' - 1);
    }
else if (grayIx > 26)
    {
    color = lighterShade(vg, ixColor,1.5);
    sprintf(codon,"%c",grayIx - 26 + 'A' - 1);
    }
else
    {
    color = cdsColor[CDS_ERROR];
    sprintf(codon,"X");
    }
return color;
}


static int setColorByCds(DNA *dna, bool codonFirstColor, boolean *foundStart, 
			 boolean reverse, boolean colorStopStart)
{
char codonChar;

if (reverse)
    reverseComplement(dna,strlen(dna));

if (sameString(chromName, "chrM"))
    codonChar = lookupMitoCodon(dna);
else
    codonChar = lookupCodon(dna);

if (codonChar == 'M' && foundStart != NULL && !(*foundStart))
    *foundStart = TRUE;

if (codonChar == 0)
    return(-3);    //stop codon
else if (codonChar == 'X')
    return(-2);     //bad input to lookupCodon
else if (colorStopStart && codonChar == 'M')
    {
    return(-1);     //start codon
    }
else if (codonFirstColor)
    return(codonChar - 'A' + 1);
else
    return(codonChar - 'A' + 1 + 26);
}


static int setColorByDiff(DNA *rna, char genomicCodon, bool codonFirstColor)
/* Difference ==> red, otherwise keep the alternating shades. */
{
char rnaCodon = lookupCodon(rna);

/* Translate lookupCodon stop codon result into what genomicCodon would have 
 * for a stop codon: */
if (rnaCodon == '\0')
    rnaCodon = '*';

if (genomicCodon != 'X' && genomicCodon != rnaCodon)
    return(-3);    //red (reusing stop codon color)
else if (codonFirstColor)
    return(genomicCodon - 'A' + 1);
else
    return(genomicCodon - 'A' + 1 + 26);
}


static void getMrnaCds(char *acc, struct genbankCds* cds)

/* Get cds start and stop from genbank, if available. Otherwise it
 * does nothing */
{
static boolean first = TRUE, haveGbCdnaInfo = FALSE;
if (first)
    {
    haveGbCdnaInfo = hTableExists("gbCdnaInfo");
    first = FALSE;
    }
if (haveGbCdnaInfo)
    {
    char query[256], buf[256], *cdsStr;
    struct sqlConnection *conn = hAllocConn();
    sprintf(query, "select cds.name from gbCdnaInfo,cds where (acc = '%s') and (gbCdnaInfo.cds = cds.id)", acc);
    cdsStr = sqlQuickQuery(conn, query, buf, sizeof(buf));
    if (cdsStr != NULL)
        genbankCdsParse(cdsStr, cds);
    hFreeConn(&conn);
    }
}

static void getHiddenGaps(struct psl *psl, unsigned *gaps)
/*return the gaps in the query sequence of a psl that are 'hidden' 
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
					     boolean colorStopStart)
{
struct simpleFeature *sfList = NULL;
unsigned *retGaps = NULL;
boolean extraInfo = (drawOpt != baseColorDrawGenomicCodons);
struct genbankCds cds;

if (pslIsProtein(psl))
    {
    cds.start=0;
    cds.end=psl->qSize*3;
    cds.startComplete = TRUE;
    cds.endComplete = TRUE; 
    cds.complement = (psl->strand[1] == '-');   
    }
else
    {
    /*get CDS from genBank*/
    memset(&cds, 0, sizeof(cds));
    getMrnaCds(psl->qName, &cds);
    }

/* if cds not in genbank or not parsable - revert to normal*/
/* if showing bases we don't want to have thin bars ala genePred format */
if (drawOpt == baseColorDrawItemBases ||
    drawOpt == baseColorDrawDiffBases ||
    (cds.start == cds.end))
    {
    int grayIx = pslGrayIx(psl, isXeno, maxShade);
    sfList = sfFromPslX(psl, grayIx, sizeMul);
    }
else
    {
    int insertMergeSize = extraInfo ? -1 : 0;
    struct genePred *gp = genePredFromPsl2(psl, genePredCdsStatFld|genePredExonFramesFld,
                                           &cds, insertMergeSize);
    lf->start = gp->txStart;
    lf->end = gp->txEnd;
    lf->tallStart = gp->cdsStart;
    lf->tallEnd = gp->cdsEnd;
    sfList = baseColorCodonsFromGenePred(chrom, lf, gp, retGaps, extraInfo,
				  colorStopStart);
    genePredFree(&gp);
    }
return(sfList);
}



void baseColorCodonsFromPsl(char *chromName, struct linkedFeatures *lf, 
			    struct psl *psl, int sizeMul, boolean
                            isXeno, int maxShade,
			    enum baseColorDrawOpt drawOpt)
/*divide a pslX record into a linkedFeature, where each simple feature
 * is a 3-base codon (or a partial codon if on a boundary). Uses
 * GenBank to get the CDS start/stop of the psl record and also grabs
 * the sequence. This takes care of hidden gaps in the alignment, that
 * alter the frame. Therefore this function relies on the mRNA
 * sequence (rather than the genomic) to determine the frame.*/
{
boolean colorStopStart = (drawOpt != baseColorDrawDiffCodons);
struct simpleFeature *sfList = splitPslByCodon(chromName, lf, psl, sizeMul,
                                               isXeno, maxShade, drawOpt,
					       colorStopStart);
slReverse(&sfList);
lf->codons = sfList;
}


enum baseColorDrawOpt baseColorGetDrawOpt(struct track *tg)
/* Determine what base/codon coloring option (if any) has been selected 
 * in trackDb/cart, and gate with zoom level. */
{
enum baseColorDrawOpt ret = baseColorDrawOff;
boolean showDiffBasesAllScales = FALSE;

if (cart == NULL || tg == NULL || tg->tdb == NULL)
    return ret ;

ret = baseColorDrawOptEnabled(cart, tg->tdb);
showDiffBasesAllScales =
    (trackDbSetting(tg->tdb, "showDiffBasesAllScales") != NULL);

/* Gate with zooming constraints: */
if (!zoomedToCdsColorLevel && (ret == baseColorDrawGenomicCodons ||
			       ret == baseColorDrawItemCodons))
    ret = baseColorDrawOff;
if (!zoomedToBaseLevel && (ret == baseColorDrawItemBases))
    ret = baseColorDrawOff;
if (!(zoomedToCdsColorLevel || showDiffBasesAllScales) &&
    ret == baseColorDrawDiffCodons)
    ret = baseColorDrawOff;
if (!(zoomedToBaseLevel || showDiffBasesAllScales) &&
    ret == baseColorDrawDiffBases)
    ret = baseColorDrawOff;

return ret;
}


static struct dnaSeq *maybeGetSeqUpper(char *name, char *tableName)
/* Look up the sequence in genbank tables (hGenBankGetMrna also searches 
 * seq if it can't find it in GB tables), uppercase and return it if we find 
 * it, return NULL if we don't find it. */
{
struct dnaSeq *mrnaSeq = NULL;
if (sameString(tableName,"refGene"))
    mrnaSeq = hGenBankGetMrna(name, "refMrna");
else
    mrnaSeq = hGenBankGetMrna(name, NULL);

if (mrnaSeq != NULL)
    touppers(mrnaSeq->dna);
return mrnaSeq;
}



static void makeCdsShades(struct vGfx *vg, Color *cdsColor)
/* setup CDS colors */
{
cdsColor[CDS_ERROR] = vgFindColorIx(vg,0,0,0); 
cdsColor[CDS_ODD] = vgFindColorIx(vg,CDS_ODD_R,CDS_ODD_G,CDS_ODD_B);
cdsColor[CDS_EVEN] = vgFindColorIx(vg,CDS_EVEN_R,CDS_EVEN_G,CDS_EVEN_B);
cdsColor[CDS_START] = vgFindColorIx(vg,CDS_START_R,CDS_START_G,CDS_START_B);
cdsColor[CDS_STOP] = vgFindColorIx(vg,CDS_STOP_R,CDS_STOP_G,CDS_STOP_B);
cdsColor[CDS_SPLICE] = vgFindColorIx(vg,CDS_SPLICE_R,CDS_SPLICE_G,
				     CDS_SPLICE_B);
cdsColor[CDS_PARTIAL_CODON] = vgFindColorIx(vg,CDS_PARTIAL_CODON_R,
					    CDS_PARTIAL_CODON_G, 
					    CDS_PARTIAL_CODON_B);
cdsColor[CDS_GENOMIC_INSERTION] = vgFindColorIx(vg,CDS_GENOMIC_INSERTION_R, 
						CDS_GENOMIC_INSERTION_G,
						CDS_GENOMIC_INSERTION_B);
}



static void updatePartialCodon(char *retStr, char *chrom, int start,
        int end, boolean reverse, struct dnaSeq *codonDna, int base)
{
    char tmpStr[5];
    char tmpDna[5];

    snprintf(tmpDna, min(3-strlen(retStr)+1,abs(end-start+1)), "%s",
            &codonDna->dna[start-base]);
    if (!reverse)
        safef(tmpStr, 4, "%s%s", retStr, tmpDna);
    else
        safef(tmpStr, 4, "%s%s", tmpDna, retStr);

    strncpy( retStr, tmpStr, 4 );
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
    sf->grayIx = setColorByCds(codon, sf->start % 6 < 3, NULL, FALSE, TRUE);
    zeroBytes(codon, 4);
    slAddHead(&sfList, sf);
    }
slReverse(&sfList);
return sfList;
}

static struct simpleFeature *splitByCodon( char *chrom, 
        struct linkedFeatures *lf, 
        unsigned *starts, unsigned *ends, 
        int blockCount, unsigned
        cdsStart, unsigned cdsEnd,
        unsigned *gaps, int *exonFrames, boolean colorStopStart)
{
    int codon = 0;
    int frame = 0;
    int currentStart = 0, currentEnd = 0;
    struct dnaSeq *codonDna = NULL;
    char partialCodonSeq[4];
    char theRestOfCodon[4];
    char tempCodonSeq[4];
    int currentSize, base;
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

    for (i=i0; (iInc*i)<(iInc*iN); i=i+iInc)
    {
        int thisStart, thisEnd;
	unsigned cdsLine;
        if(exonFrames != NULL)
        {
            if(exonFrames[i] > 0)
                frame = 3 - exonFrames[i];
            else
                frame = 0;
        }

        if(frame == 0)
            strcpy(partialCodonSeq,"");

        thisStart = starts[i];
        thisEnd = ends[i];
        cdsLine = posStrand?cdsStart:cdsEnd;

        // 3' block or 5' block
        if ((thisStart < cdsStart && thisEnd <= cdsStart) ||
                (thisStart >= cdsEnd && thisEnd > cdsEnd))
        {
            AllocVar(sf);
            sf->start = thisStart;
            sf->end = thisEnd; 
            slAddHead(&sfList, sf);
            continue;
        }
        //UTR to coding block
        else if (thisEnd > cdsLine && thisStart < cdsLine)
        {
            AllocVar(sf);
            if(posStrand)
            {
                sf->start = thisStart;
                sf->end = cdsStart;
                currentStart = cdsStart;
            }
            else
            {
                sf->start = cdsEnd;
                sf->end = thisEnd;
                currentEnd = cdsEnd;
            }
            slAddHead(&sfList, sf);
        }
        else
            if(posStrand)
                currentStart = thisStart;
            else 
                currentEnd = thisEnd;


        /*get dna for entire coding block. this is faster than
          getting it for each codon, but suprisingly not faster
          than getting it for each linked feature*/
	if (thisStart < cdsStart) thisStart = cdsStart;
	if (thisEnd > cdsEnd) thisEnd = cdsEnd;
        codonDna = hDnaFromSeq( chrom, thisStart, thisEnd, dnaUpper );
        base = thisStart;

        //break each block by codon and set color code to denote codon
        while (TRUE)
        {
            int codonInc = frame;
            if (frame == 0)  
            {
                codon++; codon %= 2; codonInc = 3;
            }
            
            if(posStrand)
                currentEnd = currentStart + codonInc;
            else
                currentStart = currentEnd - codonInc; 

            AllocVar(sf);
            sf->start = currentStart;
            sf->end = currentEnd;

            //we've gone off the end of the current block
            if ((posStrand && currentEnd > thisEnd) ||
                    (!posStrand && currentStart < thisStart ))
            {

                if (posStrand)
                {
                    frame = currentEnd - thisEnd;
                    sf->end = currentEnd = thisEnd;
                }
                else
                {
                    frame = thisStart - currentStart;
                    sf->start = currentStart = thisStart;
                }

                if(frame == 3) 
                {
                    frame = 0;
                    break;
                }

                /*accumulate partial codon in case of 
                  one base exon, or start a new one.*/
                updatePartialCodon(partialCodonSeq, chrom, sf->start,
                                sf->end, !posStrand, codonDna, base);

                /*get next 'frame' nt's to see what codon will be 
                  (skipping intron sequence)*/
                getCodonDna(theRestOfCodon, chrom, frame, starts, ends, 
                        blockCount, cdsStart, cdsEnd, i, !posStrand, NULL);

                if(posStrand)
                    safef(tempCodonSeq, 4, "%s%s", partialCodonSeq, 
                            theRestOfCodon);
                else
                    safef(tempCodonSeq, 4, "%s%s", theRestOfCodon, 
                            partialCodonSeq );

                sf->grayIx = ((posStrand && currentEnd <= cdsEnd) || 
                             (!posStrand && currentStart >= cdsStart))?
                             setColorByCds( tempCodonSeq,codon,&foundStart, 
                             !posStrand, colorStopStart):-2;
                slAddHead(&sfList, sf);
                break;
            }

            currentSize = sf->end - sf->start;
            /*inside a coding block (with 3 bases)*/
            if (currentSize == 3)
            {
                char currentCodon[5];
                char *thisDna = &codonDna->dna[sf->start - base];
                snprintf(currentCodon, currentSize+1, "%s", thisDna);
                sf->grayIx = ((posStrand && currentEnd <= cdsEnd) || 
                             (!posStrand && currentStart >= cdsStart))?
                             setColorByCds(currentCodon,codon,&foundStart, 
                                     !posStrand, colorStopStart):-2;
            }
            /*start of a coding block with less than 3 bases*/
            else if (currentSize < 3)
            {
                updatePartialCodon(partialCodonSeq,chrom,sf->start,
                        sf->end,!posStrand,codonDna, base);
                if (strlen(partialCodonSeq) == 3) 
                    sf->grayIx = setColorByCds(partialCodonSeq,codon,
                            &foundStart, !posStrand, colorStopStart);
                else
                    sf->grayIx = -2;
                strcpy(partialCodonSeq,"" );

                /*update frame based on bases appended*/
                frame -= (sf->end - sf->start);     
            }
            else
                errAbort("%s: Too much dna (%d,%s)<br>\n", lf->name, 
                        codonDna->size, codonDna->dna );

            slAddHead(&sfList, sf);
            if(posStrand)
                currentStart = currentEnd;
            else
                currentEnd = currentStart;
        }
	/* coding block to UTR block */
	if (posStrand && (thisEnd < ends[i]))
	    {
            AllocVar(sf);
            sf->start = thisEnd;
            sf->end = ends[i];
            slAddHead(&sfList, sf);
	    }
	else if (!posStrand && (thisStart > starts[i]))
	    {
            AllocVar(sf);
            sf->start = starts[i];
            sf->end = thisStart;
            slAddHead(&sfList, sf);
	    }
    }

    if(posStrand)
        slReverse(&sfList);
    return(sfList);
}

struct simpleFeature *baseColorCodonsFromGenePred(char *chrom,
	struct linkedFeatures *lf, struct genePred *gp, unsigned *gaps,
	boolean extraInfo, boolean colorStopStart)
/* Given an lf and the genePred from which the lf was constructed, 
 * return a list of simpleFeature elements, one per codon (or partial 
 * codon if the codon falls on a gap boundary.  If extraInfo is true, 
 * use the frames portion of gp (which should be from a genePredExt);
 * otherwise determine frame from genomic sequence. */
{
    if(extraInfo)
        return(splitByCodon(chrom,lf,gp->exonStarts,gp->exonEnds,gp->exonCount,
			    gp->cdsStart,gp->cdsEnd,gaps,gp->exonFrames,
			    colorStopStart));
    else
        return(splitByCodon(chrom,lf,gp->exonStarts,gp->exonEnds,gp->exonCount,
                    gp->cdsStart,gp->cdsEnd,gaps,NULL, colorStopStart));
}


static void getMrnaBases(struct psl *psl, struct dnaSeq *mrnaSeq,
			 int mrnaS, int s, int e, boolean isRc,
			 char retMrnaBases[4], boolean *retGenomicInsertion)
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
	int *gaps = NULL;
	boolean appendAtStart = FALSE;
	AllocArray(gaps, psl->blockCount+2);

	for(i=0; i<psl->blockCount; i++)
	    {
	    unsigned tStart = psl->tStarts[i];
	    unsigned tEnd = tStart + psl->blockSizes[i];
	    if (psl->strand[1] == '-') 
		reverseIntRange(&tStart, &tEnd, psl->tSize);

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
	if(idx >= 0 && gaps[idx] > 0 && retGenomicInsertion != NULL)
	    *retGenomicInsertion = TRUE;

	if (!appendAtStart)
            {
	    newIdx = mrnaS + size;
	    strncpy(retMrnaBases, &mrnaSeq->dna[mrnaS], size);
	    strncpy(retMrnaBases+size, &mrnaSeq->dna[newIdx], 3-size);
            }
	else
            {
	    newIdx = mrnaS - (3 - size);
	    strncpy(retMrnaBases, &mrnaSeq->dna[newIdx], 3);
            }
        }
    else
	{
	strncpy(retMrnaBases, "NNN", 3);
	}
    }
else
    strncpy(retMrnaBases, &mrnaSeq->dna[mrnaS], 3);
retMrnaBases[3] = '\0';
if (isRc)
    reverseComplement(retMrnaBases, strlen(retMrnaBases));
}

static void drawDiffTextBox(struct vGfx *vg, int xOff, int y, 
        double scale, int heightPer, MgFont *font, Color color, 
        char *chrom, unsigned s, unsigned e, struct psl *psl, 
        struct dnaSeq *mrnaSeq, struct linkedFeatures *lf,
        int grayIx, enum baseColorDrawOpt drawOpt,
        int maxPixels, Color *trackColors, Color ixColor)
{
int mrnaS = convertCoordUsingPsl( s, psl ); 
if(mrnaS >= 0)
    {
    struct dyString *dyMrnaSeq = newDyString(256);
    char mrnaBases[4];
    char genomicCodon[2];
    char mrnaCodon[2]; 
    Color textColor = whiteIndex();
    boolean genomicInsertion = FALSE;

    getMrnaBases(psl, mrnaSeq, mrnaS, s, e, (lf->orientation == -1),
		 mrnaBases, &genomicInsertion);

    if (e <= lf->tallEnd)
	{
        boolean startColor = FALSE;

	if (drawOpt == baseColorDrawItemCodons)
	    {
	    /* re-set color of this block based on mrna codons rather than
	     * genomic, but keep the odd/even cycle of dark/light shades. */
	    int mrnaGrayIx = setColorByCds(mrnaBases, (grayIx > 26), NULL,
					   FALSE, TRUE);
	    if (color == cdsColor[CDS_START])
                startColor = TRUE;
	    color = colorAndCodonFromGrayIx(vg, mrnaCodon, mrnaGrayIx,
					    ixColor);
	    if (startColor && sameString(mrnaCodon,"M"))
                color = cdsColor[CDS_START];
	    }
	if (drawOpt == baseColorDrawDiffCodons)
	    {
	    /* Color codons red wherever mrna differs from genomic;
	     * keep the odd/even cycle of dark/light shades. */
	    colorAndCodonFromGrayIx(vg, genomicCodon, grayIx, ixColor);
	    int mrnaGrayIx = setColorByDiff(mrnaBases, genomicCodon[0],
					    (grayIx > 26));
	    color = colorAndCodonFromGrayIx(vg, mrnaCodon, mrnaGrayIx,
					    ixColor);
	    safef(mrnaCodon, sizeof(mrnaCodon), "%c", lookupCodon(mrnaBases));
	    }
        if (genomicInsertion)
	    textColor = color = cdsColor[CDS_GENOMIC_INSERTION];
	}

    dyStringAppendN(dyMrnaSeq, (char*)&mrnaSeq->dna[mrnaS], e-s);

    if (drawOpt == baseColorDrawItemBases)
	{
	if (cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE))
	    complement(dyMrnaSeq->string, dyMrnaSeq->stringSize);
	drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score, font, dyMrnaSeq->string,
				    zoomedToBaseLevel, winStart, maxPixels);
	}
    else if (drawOpt == baseColorDrawItemCodons)
	drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score, font, mrnaCodon,
				    zoomedToCodonLevel, winStart, maxPixels);
    else if (drawOpt == baseColorDrawDiffBases)
	{
	char *diffStr = NULL;
	struct dnaSeq *genoSeq = hDnaFromSeq(chromName, s, e, dnaUpper);
	diffStr = needMem(sizeof(char) * (e - s + 1));
	maskDiffString(diffStr, dyMrnaSeq->string, (char *)genoSeq->dna,
		       ' ');
	if (cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE))
	    complement(diffStr, strlen(diffStr));
	drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score, font, diffStr, 
				    zoomedToBaseLevel, winStart, maxPixels);
	freeMem(diffStr);
	dnaSeqFree(&genoSeq);
	}
    else if (drawOpt == baseColorDrawDiffCodons)
	{
	if (genomicCodon[0] != 'X' && mrnaCodon[0] != genomicCodon[0])
	    {
	    drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, 
					heightPer, color, lf->score, font,
                                        mrnaCodon, zoomedToCodonLevel,
                                        winStart, maxPixels );
	    }
	else
	    drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
				color, lf->score );
	}
    else
        errAbort("Unknown drawOpt: %d<br>\n", drawOpt);

    dyStringFree(&dyMrnaSeq);
    }
else
    {
    /*show we have an error by coloring entire exon block yellow*/
    drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
			MG_YELLOW, lf->score );
    }
}

void baseColorDrawItem(struct track *tg,  struct linkedFeatures *lf, 
		       int grayIx, struct vGfx *vg, int xOff, 
                       int y, double scale, MgFont *font, int s, int e, 
                       int heightPer, boolean zoomedToCodonLevel, 
                       struct dnaSeq *mrnaSeq, struct psl *psl, 
		       enum baseColorDrawOpt drawOpt,
                       int maxPixels, int winStart, 
                       Color originalColor)
/* Draw codon/base-colored item. */
{
char codon[2] = " ";
Color color = colorAndCodonFromGrayIx(vg, codon, grayIx, originalColor);
/* When we are zoomed out far enough so that multiple bases/codons share the 
 * same pixel, we have to draw differences in a separate pass (baseColorOverdrawDiff)
 * so don't waste time drawing the differences here: */
boolean zoomedOutToPostProcessing =
    ((drawOpt == baseColorDrawDiffBases && !zoomedToBaseLevel) ||
     (drawOpt == baseColorDrawDiffCodons && !zoomedToCdsColorLevel));

if (drawOpt == baseColorDrawGenomicCodons)
    drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
                                color, lf->score, font, codon, 
                                zoomedToCodonLevel, winStart, maxPixels);
else if (mrnaSeq != NULL && psl != NULL && !zoomedOutToPostProcessing)
    drawDiffTextBox(vg, xOff, y, scale, heightPer, font, 
		    color, chromName, s, e, psl, mrnaSeq, lf,
		    grayIx, drawOpt, maxPixels,
		    tg->colorShades, originalColor);
else
    /* revert to normal coloring */
    drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
			color, lf->score );
}


static void drawCdsDiffCodonsOnly(struct track *tg,  struct linkedFeatures *lf,
			   struct vGfx *vg, int xOff,
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
	    boolean genomicInsertion = FALSE;
	    Color color = cdsColor[CDS_STOP];
	    getMrnaBases(psl, mrnaSeq, mrnaS, s, e, (lf->orientation == -1),
			 mrnaBases, &genomicInsertion);
	    if (genomicInsertion)
		color = cdsColor[CDS_GENOMIC_INSERTION];
	    mrnaCodon = lookupCodon(mrnaBases);
	    if (mrnaCodon == '\0')
		mrnaCodon = '*';
	    colorAndCodonFromGrayIx(vg, genomicCodon, sf->grayIx, dummyColor);
	    if (genomicInsertion || mrnaCodon != genomicCodon[0])
		drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
				    color, lf->score);
	    }
	else
	    {
	    /*show we have an error by coloring entire exon block yellow*/
	    drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
				MG_YELLOW, lf->score );
	    }
	}
    }
}

void baseColorOverdrawDiff(struct track *tg,  struct linkedFeatures *lf,
			   struct vGfx *vg, int xOff,
			   int y, double scale, int heightPer,
			   struct dnaSeq *mrnaSeq, struct psl *psl,
			   int winStart, enum baseColorDrawOpt drawOpt)
/* If we're drawing different bases/codons, and zoomed out past base/codon 
 * level, draw 1-pixel wide red lines only where bases/codons differ from 
 * genomic.  This tests drawing mode and zoom level but assumes that lf itself 
 * has been drawn already and we're not in dense mode etc. */
{
boolean showDiffBasesAllScales =
    (tg->tdb && trackDbSetting(tg->tdb, "showDiffBasesAllScales"));
if (showDiffBasesAllScales)
    {
    if (drawOpt == baseColorDrawDiffCodons && !zoomedToCdsColorLevel)
	{
	drawCdsDiffCodonsOnly(tg, lf, vg, xOff, y, scale,
			      heightPer, mrnaSeq, psl, winStart);
	}
    if (drawOpt == baseColorDrawDiffBases && !zoomedToBaseLevel)
	{
	drawCdsDiffBaseTickmarksOnly(tg, lf, vg, xOff, y, scale,
				     heightPer, mrnaSeq, psl, winStart);
	}
    }
}


enum baseColorDrawOpt baseColorDrawSetup(struct vGfx *vg, struct track *tg,
			struct linkedFeatures *lf,
			struct dnaSeq **retMrnaSeq, struct psl **retPsl)
/* Returns the CDS coloring option, allocates colors if necessary, and 
 * returns the sequence and psl record for the given item if applicable. */
{
enum baseColorDrawOpt drawOpt = baseColorGetDrawOpt(tg);

if (drawOpt == baseColorDrawOff)
    return drawOpt;

/* allocate colors for coding coloring */
if (!cdsColorsMade)
    { 
    makeCdsShades(vg, cdsColor);
    cdsColorsMade = TRUE;
    }
   
/* If we are using item sequence, fetch alignment and sequence: */
if (drawOpt != baseColorDrawOff && startsWith("psl", tg->tdb->type))
    {
    *retPsl = (struct psl *)(lf->original);
    if (*retPsl == NULL)
	return baseColorDrawOff;
    }
if (drawOpt == baseColorDrawItemBases ||
    drawOpt == baseColorDrawDiffBases ||
    drawOpt == baseColorDrawItemCodons ||
    drawOpt == baseColorDrawDiffCodons)
    {
    *retMrnaSeq = maybeGetSeqUpper(lf->name, tg->mapName);
    if (*retMrnaSeq != NULL && *retPsl != NULL)
	{
	if ((*retPsl)->strand[0] == '-' || (*retPsl)->strand[1] == '-')
	    reverseComplement((*retMrnaSeq)->dna, strlen((*retMrnaSeq)->dna));
	}
    else
	return baseColorDrawOff;
    }

return drawOpt;
}

void baseColorDrawRulerCodons(struct vGfx *vg, struct simpleFeature *sfList,
                double scale, int xOff, int y, int height, MgFont *font, 
                int winStart, int maxPixels, bool zoomedToText)
/* Draw amino acid translation of genomic sequence based on a list
   of codons. Used for browser ruler in full mode*/
{
struct simpleFeature *sf;

if (!cdsColorsMade)
    {
    makeCdsShades(vg, cdsColor);
    cdsColorsMade = TRUE;
    }

for (sf = sfList; sf != NULL; sf = sf->next)
    {
    char codon[4];
    Color color = colorAndCodonFromGrayIx(vg, codon, sf->grayIx, MG_GRAY);
    if (zoomedToText)
        drawScaledBoxSampleWithText(vg, sf->start, sf->end, scale, insideX, y,
				    height, color, 1.0, font, codon, TRUE,
				    winStart, maxPixels);
    else
        /* zoomed in just enough to see colored boxes */
        drawScaledBoxSample(vg, sf->start, sf->end, scale, xOff, y, height, 
		                color, 1.0);
    }
}


void baseColorDrawCleanup(struct linkedFeatures *lf, struct dnaSeq **pMrnaSeq,
			  struct psl **pPsl)
/* Free structures allocated just for base/cds coloring. */
{
if (lf->original != NULL)
    {
    /* Currently, lf->original is used only for coloring PSL's.  If it is 
     * used to color some other type in the future, this will remind the 
     * programmer to free it here. */
    assert(pPsl != NULL);
    assert(lf->original == *pPsl);
    }
if (pPsl != NULL)
    pslFree(pPsl);
if (pMrnaSeq != NULL)
    dnaSeqFree(pMrnaSeq);
}


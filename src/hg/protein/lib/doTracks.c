/* doTracks.c draw various tracks for Proteome Browser */
#include "common.h"
#include "hCommon.h"
#include "portable.h"
#include "memalloc.h"
#include "jksql.h"
#include "vGfx.h"
#include "memgfx.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "web.h"
#include "cheapcgi.h"
#include "hgColors.h"
#include "pbTracks.h"
#include "trashDir.h"

static char const rcsid[] = "$Id: doTracks.c,v 1.19 2008/09/03 19:20:58 markd Exp $";

int prevGBOffsetSav;
char trackOffset[20];
int pixWidth, pixHeight;
struct tempName gifTn;
char *mapName = "map";
char *trackTitle;
int trackTitleLen;
boolean showPrevGBPos = TRUE;

void calxy(int xin, int yin, int *outxp, int *outyp)
/* calxy() converts a logical drawing coordinate into an actual
   drawing coordinate with scaling and minor adjustments */
{
*outxp = xin*pbScale + 120 - trackOrigOffset;
*outyp = yin         + currentYoffset;
}

/* original calxy without trackOrigOffset, may be useful 
   if track names needed at the left */
void calxy0(int xin, int yin, int *outxp, int *outyp)
/* calxy() converts a logical drawing coordinate into an actual
   drawing coordinate with scaling and minor adjustments */
{
*outxp = xin*pbScale + 120;
*outyp = yin         + currentYoffset;
}

void relativeScroll(double amount)
/* Scroll percentage of visible window. */
{
trackOrigOffset = trackOrigOffset + (int)(amount*MAX_PB_PIXWIDTH);

/* Make sure don't scroll of ends. */
if (trackOrigOffset > ((pbScale* protSeqLen) - 600))
    trackOrigOffset = pbScale*protSeqLen - 700;
if (trackOrigOffset < 0) trackOrigOffset = 0;

safef(pbScaleStr, sizeof(pbScaleStr), "%d", pbScale);
cgiMakeHiddenVar("pbScaleStr", pbScaleStr);
}

void mapBoxTrackTitle(int x, int y, int width, int height, char *title, char *tagName)
{
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);
if (proteinInSupportedGenome)
    {
    hPrintf("HREF=\"../goldenPath/help/pbTracksHelpFiles/pb%s.shtml\"", tagName);
    }
else
    {
    hPrintf("HREF=\"../goldenPath/help/pbTracksHelpFiles/pb%s.shtml\"", tagName);
    }
hPrintf(" target=_blank ALT=\"Click here for explanation of %c%s%c\">\n", '\'', title, '\'');
}

void doAnomalies(char *aa, int len, int *yOffp)
/* draw the AA Anomalies track */
{
char res;
int index;

char cond_str[255];
char *answer;
    
int xx, yy;
int i, j;
	
char *chp;
int aaResCnt[20];
double aaResFreqDouble[20];
int abnormal;
int ia = -1;
double pctLow[20], pctHi[20];

/* count frequency for each residue for current protein */
chp = aa;
for (j=0; j<20; j++) 
    {
    aaResCnt[j] = 0;
       
    /* get cutoff threshold value pairs */
    safef(cond_str, sizeof(cond_str), "AA='%c'", aaAlphabet[j]);
    answer = sqlGetField(database, "pbAnomLimit", "pctLow", cond_str);
    pctLow[j] = (double)(atof(answer));
    answer = sqlGetField(database, "pbAnomLimit", "pctHi", cond_str);
    pctHi[j] = (double)(atof(answer));
    }

for (i=0; i<len; i++)
    {
    for (j=0; j<20; j++)
        {
        if (*chp == aaChar[j])
            {
            aaResCnt[j] ++;
            break;
	    }
        }
    chp++;
    }

for (j=0; j<20; j++)
    {
    aaResFreqDouble[j] = ((double)aaResCnt[j])/((double)len);
    }

currentYoffset = *yOffp;
    
for (index=0; index < len; index++)
    {
    res = aa[index];
    
    ia = -1;
    for (j=0; j<20; j++)
	{
	if (res == aaChar[j])
	    {
	    ia = j;
	    break;
	    }
	}

    /* skip non-standard AA alphabets */
    if (ia == -1) break;

    calxy(index, *yOffp, &xx, &yy);

    abnormalColor = pbRed;
    abnormal = chkAnomaly(aaResFreqDouble[ia], pctLow[ia], pctHi[ia]);
    if (abnormal > 0)
	{
	vgBox(g_vg, xx, yy-5, 1*pbScale, 5, abnormalColor);
	}
    else
	{
	if (abnormal < 0)
	    {
	    vgBox(g_vg, xx, yy, 1*pbScale, 5, abnormalColor);
	    }
	}
    vgBox(g_vg, xx, yy, 1*pbScale, 1, MG_BLACK);
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 20, bkgColor);

trackTitle = cloneString("AA Anomalies");
vgTextRight(g_vg, xx-25, yy-4, 10, 10, MG_BLACK, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-6, trackTitleLen*6+12, 14, trackTitle, "pepAnom");

/* update y offset */
*yOffp = *yOffp + 15;
}

void doCharge(char *aa, int len, int *yOffp)
/* draw polarity track */
{
char res;
int index;
    
int xx, yy;
	
currentYoffset = *yOffp;
    
for (index=0; index < len; index++)
    {
    res = aa[index];
    calxy(index, *yOffp, &xx, &yy);

    if (aa_attrib[(int)res] == CHARGE_POS)
	{
	vgBox(g_vg, xx, yy-9, 1*pbScale, 9, pbBlue);
	}
    else
    if (aa_attrib[(int)res] == CHARGE_NEG)
	{
	vgBox(g_vg, xx, yy, 1*pbScale, 9, pbRed);
	}
    else
    if (aa_attrib[(int)res] == POLAR)
	{
	vgBox(g_vg, xx, yy, 1*pbScale, 5, pbRed);
	}
    else
    if (aa_attrib[(int)res] == NEUTRAL)
	{
	vgBox(g_vg, xx, yy, 1*pbScale, 1, MG_BLACK);
	}
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 30, bkgColor);
trackTitle = cloneString("Polarity");
vgTextRight(g_vg, xx-25, yy-4, 10, 10, MG_BLACK, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-6, trackTitleLen*6+12, 14, trackTitle, "polarity");

vgTextRight(g_vg, xx-14, yy-10, 10, 10, pbBlue,  g_font, "+");
vgTextRight(g_vg, xx-14, yy, 10, 10, pbRed, g_font, "-");

*yOffp = *yOffp + 15;
}

void doHydrophobicity(char *aa, int len, int *yOffp)
/* draw Hydrophobicity track */
{
char res;
int index;
    
int xx, yy;
int h;
int i, i0, i9;
int l;
    
int iw = 5;
float sum, avg;
	
currentYoffset = *yOffp;
    
for (index=0; index < len; index++)
    {
    res = aa[index];
    calxy(index, *yOffp, &xx, &yy);
	{
	sum = 0;
	i=index;
		
	i0 = index - iw;
	if (i0 < 0) i0 = 0;
	i9 = index + iw;
	if (i9 >= len) i9 = len -1;

	l = 0;
	for (i=i0; i <= i9; i++)
	    {
	    sum = sum + aa_hydro[(int)aa[i]];
	    l++;
	    }
	
	avg = sum/(float)l;
		
	if (avg> 0.0)
	    {
	    h = 5 * 9 * (100.0 * avg / 9.0) / 100;
	    vgBox(g_vg, xx, yy-h, 1*pbScale, h, pbBlue);
	    }
	else
	    {
	    h = - 5 * 9 * (100.0 * avg / 9.0) / 100;
	    vgBox(g_vg, xx, yy, 1*pbScale, h, pbRed);
	    }
	}
    }
calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-17, xx, 40, bkgColor);

trackTitle = cloneString("Hydrophobicity");
vgTextRight(g_vg, xx-25, yy-4, 10, 10, MG_BLACK, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-6, trackTitleLen*6+12, 14, trackTitle, "hydroTr");

*yOffp = *yOffp + 15;
}

void doCysteines(char *aa, int len, int *yOffp)
/* draw track for Cysteines and Glycosylation */
{
char res;
int index;
    
int xx, yy;
	
currentYoffset = *yOffp;
    
for (index=0; index < len; index++)
    {
    res = aa[index];
    calxy(index, *yOffp, &xx, &yy);
    if (res == 'C')
	{
	vgBox(g_vg, xx, yy-9+1, 1*pbScale, 9, pbBlue);
	}
    else
	{
	vgBox(g_vg, xx, yy-0, 1, 1, MG_BLACK);
	}
    }

for (index=1; index < (len-2); index++)
    {
    calxy(index-1, *yOffp, &xx, &yy);
    if (aa[index-1] == 'N')
	{
	if ( (aa[index+1] == 'T') || (aa[index+1] == 'S') )
	    {
	    if ((aa[index+1] != 'C') && (aa[index+2] != 'P'))
		{
		vgBox(g_vg, xx-1, yy, 3*pbScale, 9, pbRed);
		}
	    }
	}
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-15, xx, 33, bkgColor);

trackTitle = cloneString("Cysteines");
vgTextRight(g_vg, xx-25, yy-8, 10, 10, pbBlue, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-10, trackTitleLen*6+12, 12, trackTitle, "cCntTr");

trackTitle = cloneString("Predicted");
vgTextRight(g_vg, xx-25, yy, 10, 10, pbRed, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy, trackTitleLen*6+12, 12, "Glycosylation", "glycosylation");

trackTitle = cloneString("Glycosylation");
vgTextRight(g_vg, xx-25, yy+10, 10, 10, pbRed, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy+8, trackTitleLen*6+12, 12, trackTitle, "glycosylation");
    
*yOffp = *yOffp + 15;
}

void doAAScale(int len, int *yOffp, int top_bottom)
/* draw the track to show AA scale */
{
int index;
   
int tb;	/* top or bottom flag */
  
int xx, yy;
int i;
int imax;
int interval;
   
char scale_str[20];
int markedIndex = -1;

tb = 0;
if (top_bottom < 0) tb = 1;
   
currentYoffset = *yOffp;
    
imax = len/100 * 100;
if ((len % 100) != 0) imax = imax + 100;
    
calxy(1, *yOffp, &xx, &yy);
vgBox(g_vg, xx-pbScale/2, yy-tb, (len-1)*pbScale, 1, MG_BLACK);
vgText(g_vg, xx-pbScale/2+4, yy+4-12*tb, MG_BLACK, g_font, "1");

interval = 50;
if (pbScale >= 18) interval = 10;    
	    
calxy(1, *yOffp, &xx, &yy);
vgBox(g_vg, xx-pbScale/2, yy-9*tb, 1, 9, MG_BLACK);

for (i=0; i<len; i++)
    {
    index = i+1;
    if ((index % interval) == 0)
	{
        markedIndex = index;
	if (((index % (interval*2)) == 0) || (index == len)) 
	    {
	    calxy(index, *yOffp, &xx, &yy);
	    vgBox(g_vg, xx-pbScale/2, yy-9*tb, 1, 9, MG_BLACK);
	    }
	else
	    {
	    calxy(index, *yOffp, &xx, &yy);
	    vgBox(g_vg, xx-pbScale/2, yy-5*tb, 1, 5, MG_BLACK);
	    }
    		
	safef(scale_str, sizeof(scale_str), "%d", index);
	vgText(g_vg, xx-pbScale/2+4, yy+4-12*tb, MG_BLACK, g_font, scale_str);
	}
    }

if (markedIndex != len)
    {
    calxy(len, *yOffp, &xx, &yy);
    
    /* make the end tick a little taller than regular tick */
    vgBox(g_vg, xx-pbScale/2, yy-14*tb, 1, 14, MG_BLACK);
    safef(scale_str, sizeof(scale_str), "%d", len);

    /* label the end tick with a vertical offset so that it won't overlap with regular ticks */
    vgText(g_vg, xx-pbScale/2+4, yy+12-28*tb, MG_BLACK, g_font, scale_str);
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-tb*9-1, xx, 20, bkgColor);

trackTitle = cloneString("AA Scale");
vgTextRight(g_vg, xx-25, yy-9*tb, 10, 10, MG_BLACK, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-9*tb-2, trackTitleLen*6+12, 14, trackTitle, "aaScale");

*yOffp = *yOffp + 15;
}

void mapBoxExon(int xIn, int y, int width, int height, char *mrnaID, 
		int exonNum, char *chrom, int exonGenomeStartPos, int exonGenomeEndPos)
{
int x;
/* prevent the mapBox spill over to the label */
x = xIn;
if (x < 120) x = 120;

hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);
hPrintf("HREF=\"../cgi-bin/hgTracks?db=%s&position=%s:%d-%d\"" 
	,database, chrom, exonGenomeStartPos-1, exonGenomeEndPos+3);
hPrintf(" target=_blank ALT=\"Exon %d\">\n", exonNum);
}

void mapBoxPrevGB(int x, int y, int width, int height, char *posStr)
{
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);
hPrintf("HREF=\"../cgi-bin/hgTracks?db=%s&position=%s\"", database, posStr);
hPrintf(" target=_blank ALT=\"UCSC Genome Browser %s\">\n", posStr);
}

int calPrevGB(int exonCount, char *chrom, char strand, int aaLen, int *yOffp, char *proteinID, char *mrnaID)
/* calculate the appropriate X offset for the previous Genome Browser position range */
{
int xx, yy;
int i, j;
char exonNumStr[10];
int mrnaLen;

int exonStartPos, exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
int exonNumber;
int printedExonNumber = -1;
int currentPos;
int jPrevStart, jPrevEnd;
int iPrevExon = -1;
int jPrevExonPos = 0;
int prevGBOffset;
int jcnt = 0;

/* The hypothetical mRNA length is 3 times of aaLen */
mrnaLen = aaLen * 3;

jPrevStart = mrnaLen-1;
jPrevEnd   = 0;

exonNumber = 1;

exonStartPos 	   = blockStartPositive[exonNumber-1]; 
exonEndPos 	   = blockEndPositive[exonNumber-1];
exonGenomeStartPos = blockGenomeStartPositive[exonNumber-1];
exonGenomeEndPos   = blockGenomeEndPositive[exonNumber-1];

currentYoffset = *yOffp;
    
if (strand == '-')
    {
    for (i=0; i<(exonCount-2); i++)
    	{
    	if ((prevGBStartPos < blockGenomeStartPositive[i]) &&
            (prevGBStartPos > blockGenomeEndPositive[i+1]) )
	    {
	    iPrevExon = i;
	    jPrevExonPos = blockStartPositive[i+1];
	    }
    	}

    /* handle special cases at both ends when previous GB position is outside CDS */
    if (prevGBStartPos < blockGenomeStartPositive[exonCount-1]) 
    	jPrevExonPos = blockEndPositive[exonCount-1] + 3;
    if (prevGBEndPos > blockGenomeStartPositive[0]) 
    	jPrevExonPos = blockStartPositive[0];
    }
else
    {
    for (i=0; i<(exonCount-1); i++)
    	{
    	if ((prevGBStartPos > blockGenomeEndPositive[i]) &&
            (prevGBStartPos < blockGenomeStartPositive[i+1]) )
	    {
	    iPrevExon = i;
	    jPrevExonPos = blockEndPositive[i];
	    }
    	}

    /* handle special cases at both ends when previous GB position is outside CDS */
    if (prevGBStartPos < blockGenomeStartPositive[0]) 
    	jPrevExonPos = blockStartPositive[0];
    if (prevGBEndPos > blockGenomeEndPositive[exonCount-1]) 
    	jPrevExonPos = blockEndPositive[exonCount-1];
    }

for (j = 0; j < mrnaLen; j++)
    {
    calxy(j/3, *yOffp, &xx, &yy);
    if (j > exonEndPos)
	{
	if (printedExonNumber != exonNumber)
	    {
            if ((exonEndPos - exonStartPos)*pbScale/3 > 12) 
	    	{
	    	safef(exonNumStr, sizeof(exonNumStr), "%d", exonNumber);
	    	}
 	    printedExonNumber = exonNumber;
	    }

	if (exonNumber < exonCount)
    	    {
	    exonNumber++;
	    exonStartPos       = blockStartPositive[exonNumber-1]; 
	    exonEndPos 	       = blockEndPositive[exonNumber-1];
	    exonGenomeStartPos = blockGenomeStartPositive[exonNumber-1];
	    exonGenomeEndPos   = blockGenomeEndPositive[exonNumber-1];
	    }
    	}

    if (strand == '-')
	{
    	currentPos = blockGenomeStartPositive[exonNumber-1] + (blockEndPositive[exonNumber-1]-j)+1;
    	}
    else
	{
    	currentPos = blockGenomeStartPositive[exonNumber-1]+(j - blockStartPositive[exonNumber-1])+1;
    	}
    if ((currentPos >= prevGBStartPos) && (currentPos <= prevGBEndPos))
	{
        jcnt++;
	if (j < jPrevStart) jPrevStart = j;
	if (j > jPrevEnd)   jPrevEnd   = j;
	}
    }
if (jcnt > 0)
    {
    prevGBOffset = jPrevStart/3 * pbScale ;
    }
else
    {
    prevGBOffset = jPrevExonPos/3 * pbScale ;
    }
return(prevGBOffset);
}

void doPrevGB(int exonCount, char *chrom, char strand, int aaLen, int *yOffp, char *proteinID, char *mrnaID)
/* draw the previous Genome Browser position range */
{
int xx, yy, xx0;
int i, j;
char prevPosMessage[300];
char exonNumStr[10];
int mrnaLen;
Color color;

int exonStartPos, exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
int exonNumber;
int printedExonNumber = -1;
Color exonColor[2];
int currentPos;
int jPrevStart, jPrevEnd;
int jcnt = 0;
int iPrevExon = -1;
int jPrevExonPos = 0;

Color defaultColor;
defaultColor = vgFindColorIx(g_vg, 170, 170, 170);

/* The imaginary mRNA length is 3 times of aaLen */
mrnaLen = aaLen * 3;

exonColor[0] = pbBlue;
exonColor[1] = vgFindColorIx(g_vg, 0, 180, 0);

jPrevStart = mrnaLen-1;
jPrevEnd   = 0;

exonNumber = 1;

exonStartPos 	   = blockStartPositive[exonNumber-1]; 
exonEndPos 	   = blockEndPositive[exonNumber-1];
exonGenomeStartPos = blockGenomeStartPositive[exonNumber-1];
exonGenomeEndPos   = blockGenomeEndPositive[exonNumber-1];

currentYoffset = *yOffp;

if (strand == '-')
    {
    for (i=0; i<(exonCount-2); i++)
    	{
    	if ((prevGBStartPos < blockGenomeStartPositive[i]) &&
            (prevGBStartPos > blockGenomeEndPositive[i+1]) )
	    {
	    iPrevExon = i;
	    jPrevExonPos = blockStartPositive[i+1];
    	    /*printf("<br>*%d %d %d %d\n",i,blockGenomeEndPositive[i], */
	    /*prevGBStartPos, blockGenomeStartPositive[i]);fflush(stdout); */
	    }
    	}

    /* handle special cases at both ends when previous GB position is outside CDS */
    if (prevGBStartPos < blockGenomeStartPositive[exonCount-1]) 
    	jPrevExonPos = blockEndPositive[exonCount-1] + 3;
    if (prevGBEndPos > blockGenomeStartPositive[0]) 
    	jPrevExonPos = blockStartPositive[0];
    }
else
    {
    for (i=0; i<(exonCount-1); i++)
    	{
    	if ((prevGBStartPos > blockGenomeEndPositive[i]) &&
            (prevGBStartPos < blockGenomeStartPositive[i+1]) )
	    {
	    iPrevExon = i;
	    jPrevExonPos = blockEndPositive[i];
	    }
    	}

    /* handle special cases at both ends when previous GB position is outside CDS */
    if (prevGBStartPos < blockGenomeStartPositive[0]) 
    	jPrevExonPos = blockStartPositive[0];
    if (prevGBEndPos > blockGenomeEndPositive[exonCount-1]) 
    	jPrevExonPos = blockEndPositive[exonCount-1];
    }

for (j = 0; j < mrnaLen; j++)
    {
    color = defaultColor;
    calxy(j/3, *yOffp, &xx, &yy);
    if (j > exonEndPos)
	{
	if (printedExonNumber != exonNumber)
	    {
            if ((exonEndPos - exonStartPos)*pbScale/3 > 12) 
	    	{
	    	safef(exonNumStr, sizeof(exonNumStr), "%d", exonNumber);
	    	}
 	    printedExonNumber = exonNumber;
	    }

	if (exonNumber < exonCount)
    	    {
	    exonNumber++;
	    exonStartPos       = blockStartPositive[exonNumber-1]; 
	    exonEndPos 	       = blockEndPositive[exonNumber-1];
	    exonGenomeStartPos = blockGenomeStartPositive[exonNumber-1];
	    exonGenomeEndPos   = blockGenomeEndPositive[exonNumber-1];
	    }
    	}

    if ((j >= exonStartPos) && (j <= exonEndPos))
	{
	color = exonColor[(exonNumber-1) % 2];
	}
    if (strand == '-')
	{
    	currentPos = blockGenomeStartPositive[exonNumber-1] + (blockEndPositive[exonNumber-1]-j)+1;
    	}
    else
	{
    	currentPos = blockGenomeStartPositive[exonNumber-1]+(j - blockStartPositive[exonNumber-1])+1;
    	}

    if ((currentPos >= prevGBStartPos) && (currentPos <= prevGBEndPos))
	{
	jcnt++;
	if (j < jPrevStart) jPrevStart = j;
	if (j > jPrevEnd)   jPrevEnd   = j;
	}
    }

positionStr = cloneString(cartOptionalString(cart, "position"));
if (jcnt > 0)
    {
    calxy(jPrevStart/3, *yOffp, &xx, &yy);
    if (pbScale > 6)
    	{
    	vgBox(g_vg,  xx+(jPrevStart%3)*6, yy-2, (jPrevEnd-jPrevStart+1)*pbScale/3, 2, MG_BLACK);
    	}
    else
    	{
    	vgBox(g_vg,  xx, yy-2, (jPrevEnd-jPrevStart+1)*pbScale/3, 2, MG_BLACK);
    	}

    mapBoxPrevGB(xx+(jPrevStart%3)*6, yy-2, (jPrevEnd-jPrevStart+1)*pbScale/3, 2, positionStr);
    safef(prevPosMessage, sizeof(prevPosMessage), "Previous position in UCSC Genome Browser: %s", positionStr);
    if (strand == '-') safef(prevPosMessage, sizeof(prevPosMessage), "%s (negative strand)", prevPosMessage);
    vgText(g_vg, xx+(jPrevStart%3)*pbScale/3, yy-10, MG_BLACK, g_font, prevPosMessage);
    }
else
    {
    /*calxy(jPrevExonPos/3, *yOffp, &xx, &yy); */
    if (jPrevExonPos <= 0)
	{
        calxy(jPrevExonPos/3, *yOffp, &xx, &yy);
	vgBox(g_vg,  xx, yy, 1, 5, pbRed);
	}
    else
	{
        calxy(jPrevExonPos/3+1, *yOffp, &xx, &yy);
	vgBox(g_vg,  xx, yy, 1, 5, pbRed);
	}

    mapBoxPrevGB(xx-1, yy-1, 2, 6, positionStr);
    
    safef(prevPosMessage, sizeof(prevPosMessage), "Previous position in UCSC Genome Browser: %s (not in a CDS)",positionStr);
    if (strand == '-') safef(prevPosMessage, sizeof(prevPosMessage), "%s (negative strand)", prevPosMessage);
    calxy(0, *yOffp, &xx0, &yy);
    calxy(jPrevExonPos/3, *yOffp, &xx, &yy);
    if (xx < xx0)
	{
    	vgText(g_vg, xx0, yy-8, MG_BLACK, g_font, prevPosMessage);
	}
    else
	{
 	if (jPrevExonPos/3 < aaLen/2)
	    {
    	    vgText(g_vg, xx, yy-8, MG_BLACK, g_font, prevPosMessage);
    	    }
	else
	    {
    	    vgTextRight(g_vg, xx, yy-8, 10, 10, MG_BLACK, g_font, prevPosMessage);
    	    }
	}
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 20, bkgColor);
trackTitle = cloneString("Genome Browser");
vgTextRight(g_vg, xx-25, yy-8, 10, 10, MG_BLACK, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-15, trackTitleLen*6+12, 14, trackTitle, "gb");

*yOffp = *yOffp + 7;
}

void doExon(int exonCount, char *chrom, int aaLen, int *yOffp, char *proteinID, char *mrnaID)
/* draw the track for exons */
{
int xx, yy;
int j;
char exonNumStr[10];
int mrnaLen;
Color color;

int exonStartPos, exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
int exonNumber;
int printedExonNumber = -1;
Color exonColor[2];

Color defaultColor;
defaultColor = vgFindColorIx(g_vg, 170, 170, 170);

/* The hypothetical mRNA length is 3 times of aaLen */
mrnaLen = aaLen * 3;

exonColor[0] = pbBlue;
exonColor[1] = vgFindColorIx(g_vg, 0, 180, 0);

exonNumber = 1;

exonStartPos 	   = blockStartPositive[exonNumber-1]; 
exonEndPos 	   = blockEndPositive[exonNumber-1];
exonGenomeStartPos = blockGenomeStartPositive[exonNumber-1];
exonGenomeEndPos   = blockGenomeEndPositive[exonNumber-1];

currentYoffset = *yOffp;
    
for (j = 0; j < mrnaLen; j++)
    {
    color = defaultColor;
    calxy(j/3, *yOffp, &xx, &yy);
    if (j > exonEndPos)
	{
	if (printedExonNumber != exonNumber)
	    {
            if ((exonEndPos - exonStartPos)*pbScale/3 > 12) 
	    	{
	    	safef(exonNumStr, sizeof(exonNumStr), "%d", exonNumber);
            	vgTextRight(g_vg, xx-(exonEndPos - exonStartPos)*pbScale/3/2 - 4,
                                  yy-9, 10, 10, MG_WHITE, g_font, exonNumStr);
	    	}
            mapBoxExon(xx - (exonEndPos - exonStartPos)*pbScale/3, yy-9, 
		       	(exonEndPos - exonStartPos)*pbScale/3, 9, mrnaID, 
		       	exonNumber, chrom, 
		 	blockGenomeStartPositive[exonNumber-1], 
		       	blockGenomeEndPositive[exonNumber-1]);
	    printedExonNumber = exonNumber;
	    }

	if (exonNumber < exonCount)
    	    {
	    exonNumber++;
	    exonStartPos       = blockStartPositive[exonNumber-1]; 
	    exonEndPos 	       = blockEndPositive[exonNumber-1];
	    exonGenomeStartPos = blockGenomeStartPositive[exonNumber-1];
	    exonGenomeEndPos   = blockGenomeEndPositive[exonNumber-1];
	    }
    	}

    if ((j >= exonStartPos) && (j <= exonEndPos))
	{
	color = exonColor[(exonNumber-1) % 2];
	}
    vgBox(g_vg, xx, yy-9+3*(j-(j/3)*3), pbScale, 3, color);
    }
if ((exonEndPos - exonStartPos)*pbScale/3 > 12)
    {
    safef(exonNumStr, sizeof(exonNumStr), "%d", exonNumber);
    vgTextRight(g_vg, xx-(exonEndPos - exonStartPos)*pbScale/3/2 - 5,
                      yy-9, 10, 10, MG_WHITE, g_font, exonNumStr);
    }

mapBoxExon(xx - (exonEndPos - exonStartPos)*pbScale/3, yy-9,    
	   (exonEndPos - exonStartPos)*pbScale/3, 9, mrnaID, 
	   exonNumber, chrom, 
	   blockGenomeStartPositive[exonNumber-1], 
	   blockGenomeEndPositive[exonNumber-1]);

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 12, bkgColor);

trackTitle = cloneString("Exons");
vgTextRight(g_vg, xx-25, yy-9, 10, 10, MG_BLACK, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-12, trackTitleLen*6+12, 14, trackTitle, "exon");

*yOffp = *yOffp + 10;
}

#define MAX_SF 200
#define MAXNAMELEN 256

int sfId[MAX_SF];
int sfStart[MAX_SF], sfEnd[MAX_SF];
char superfam_name[MAX_SF][256];

struct sqlConnection *conn, *conn2;

int getSuperfamilies2(char *proteinID)
/* getSuperfamilies2() superceed getSuperfamilies() starting from hg16, 
   it gets Superfamily data of a protein 
   from ensemblXref3, sfAssign, and sfDes from the proteinsXXXXXX database,
   and placed them in arrays to be used by doSuperfamily().*/
{
struct sqlConnection *conn, *conn2, *conn3;
char query[MAXNAMELEN], query2[MAXNAMELEN];
struct sqlResult *sr, *sr2;
char **row, **row2;

char cond_str[255];

char *sfID, *seqID, *sfDesc,  *region;
int  done;
int j;

char *chp, *chp2;
int  sfCnt;
int  int_start, int_end;

if (!hTableExists(protDbName, "sfAssign")) return(0);
if (!hTableExists(protDbName, "ensemblXref3")) return(0);

conn  = hAllocConn(database);
conn2 = hAllocConn(database);
conn3 = hAllocConn(database);

safef(query2, sizeof(query), 
    "select distinct sfID, seqID from %s.ensemblXref3 x, %s.sfAssign a where (swissAcc='%s' or tremblAcc='%s') and seqID=x.protein and protein != '' and evalue <= 0.02",
      protDbName, protDbName, proteinID, proteinID);
sr2  = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
sfCnt=0;    
while (row2 != NULL)
    {      
    sfID = row2[0];
    seqID= row2[1];
    
    safef(query, sizeof(query), 
    	  "select region from %s.sfAssign where sfID='%s' and seqID='%s' and evalue <=0.02", 
	  protDbName, sfID, seqID);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    
    while (row != NULL)
    	{      
  	region   = row[0];
    	
	for (j=0; j<sfCnt; j++)
	    {
	    if (sfId[j] == atoi(sfID)) goto skip;
	    }
	
	safef(cond_str, sizeof(cond_str), "id=%s;", sfID);
    	sfDesc = sqlGetField(protDbName, "sfDes", "description", cond_str);


    	/* !!! refine logic here later to be defensive against illegal syntax */
    	chp = region;
    	done = 0;
    	while (!done)
	    {
	    chp2  = strstr(chp, "-");
	    *chp2 = '\0';
	    chp2++;

	    sscanf(chp, "%d", &int_start);
	
	    chp = chp2;
	    chp2  = strstr(chp, ",");
	    if (chp2 != NULL) 
	    	{
	    	*chp2 = '\0';
	    	}
	    else
	    	{
	    	done = 1;
		}
	    chp2++;
	    sscanf(chp, "%d", &int_end);
 	    sfId[sfCnt]    = atoi(sfID);
	    sfStart[sfCnt] = int_start;
	    sfEnd[sfCnt]   = int_end;
	    strncpy(superfam_name[sfCnt], sfDesc, MAXNAMELEN-1);
	    sfCnt++;
	    chp = chp2;
	    }
skip:
    	row = sqlNextRow(sr);
    	}

    sqlFreeResult(&sr);
    row2 = sqlNextRow(sr2);
    }
	
sqlFreeResult(&sr2);
hFreeConn(&conn);
hFreeConn(&conn2);
hFreeConn(&conn3);
return(sfCnt);
}
    
int getSuperfamilies(char *proteinID)
/* preserved here for previous older genomes.
   Newer genomes should be using getSuperfamilies2(). 6/16/04 Fan*/
{
struct sqlConnection *conn, *conn2;
char query[MAXNAMELEN];
struct sqlResult *sr;
char **row;

char cond_str[255];

char *genomeID, *seqID, *modelID, *eValue, *sfID, *sfDesc;

char *region;
int  done;

char *ensPep;
char *transcriptName;

char *chp, *chp2;
int  ii = 0;
int  int_start, int_end;

   
if (!hTableExists(database, "sfAssign")) return(0);
 
conn  = hAllocConn(database);
conn2 = hAllocConn(database);

if (hTableExists(database, "ensemblXref3")) 
    {	
    /* use ensemblXref3 for Ensembl data release after ensembl34d */
    safef(cond_str, sizeof(cond_str), "tremblAcc='%s'", proteinID);
    ensPep = sqlGetField(database, "ensemblXref3", "protein", cond_str);
    if (ensPep == NULL)
	{
   	safef(cond_str, sizeof(cond_str), "swissAcc='%s'", proteinID);
   	ensPep = sqlGetField(database, "ensemblXref3", "protein", cond_str);
	if (ensPep == NULL) return(0);
	}
    }
else
    {
    if (! (hTableExists(database, "ensemblXref") || hTableExists(database, "ensTranscript") ) )
       return(0);
    
    /* two steps query needed because the recent Ensembl gene_xref 11/2003 table does not have 
       valid translation_name */
    safef(cond_str, sizeof(cond_str), "external_name='%s'", protDisplayID);
    transcriptName = sqlGetField(database, "ensGeneXref", "transcript_name", cond_str);
    if (transcriptName == NULL)
        {
        return(0); 
        }
    else
        {
        safef(cond_str, sizeof(cond_str), "transcript_name='%s';", transcriptName);
        ensPep = sqlGetField(database, "ensTranscript", "translation_name", cond_str);
        if (ensPep == NULL) 
	    {
	    hFreeConn(&conn);
    	    return(0); 
    	    }
    	}
    }

ensPepName = ensPep;

safef(query, sizeof(query), "select * from %s.sfAssign where seqID='%s' and evalue <= 0.02;", database, ensPep);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL) return(0);
    
while (row != NULL)
    {      
    genomeID = row[0];
    seqID    = row[1];
    modelID  = row[2];
    region   = row[3];
    eValue   = row[4];
    sfID     = row[5];
    /* sfDesc   = row[6]; */
    /* !!! the recent Suprefamily sfAssign table does not have valid sf description */
    safef(cond_str, sizeof(cond_str), "id=%s;", sfID);
    sfDesc = sqlGetField(database, "sfDes", "description", cond_str);

    /* !!! refine logic here later to be defensive against illegal syntax */
    chp = region;
    done = 0;
    while (!done)
	{
	chp2  = strstr(chp, "-");
	*chp2 = '\0';
	chp2++;

	sscanf(chp, "%d", &int_start);
			
	chp = chp2;
	chp2  = strstr(chp, ",");
	if (chp2 != NULL) 
	    {
	    *chp2 = '\0';
	    }
	else
	    {
	    done = 1;
	    }
	chp2++;
	sscanf(chp, "%d", &int_end);

 	sfId[ii]    = atoi(sfID);
	sfStart[ii] = int_start;
	sfEnd[ii]   = int_end;
	strncpy(superfam_name[ii], sfDesc, MAXNAMELEN-1);
	ii++;
	chp = chp2;
	}

    row = sqlNextRow(sr);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
hFreeConn(&conn2);
  
return(ii);
}
    

void mapBoxSuperfamily(int x, int y, int width, int height, char *sf_name, int sfID)
{
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);
hPrintf("HREF=\"%s?sunid=%d\"",
	"http://supfam.mrc-lmb.cam.ac.uk/SUPERFAMILY/cgi-bin/scop.cgi", sfID);
hPrintf(" target=_blank ALT=\"%s\">\n", sf_name);
}

void vgDrawBox(struct vGfx *vg, int x, int y, int width, int height, Color color)
{
vgBox(g_vg, x, y, width, height, color);
vgBox(vg, x-1, 	y-1, 		width+2, 	1, 	MG_BLACK);
vgBox(vg, x-1, 	y+height, 	width+2, 	1, 	MG_BLACK);
vgBox(vg, x-1, 	y-1, 		1, 		height+2, MG_BLACK);
vgBox(vg, x+width, 	y-1, 		1, 		height+2, MG_BLACK);
}

void doSuperfamily(char *pepName, int sf_cnt, int *yOffp)
/* draw the Superfamily track */
{
int xx, yy;
int ii, jj;
char exonNumStr[10];
int len;
int sf_len, name_len;
int show_name;
Color sfColor;
    
/* sfColor = vgFindColorIx(g_vg, 0xf7, 0xe8, 0xaa); */
sfColor = MG_YELLOW;
   
currentYoffset = *yOffp;
   
calxy(0, *yOffp, &xx, &yy);
    
jj = 0;
for (ii=0; ii<sf_cnt; ii++)
    {
    if (sfEnd[ii] != 0)
	{
	jj++;
	safef(exonNumStr, sizeof(exonNumStr), "%d", jj);

	calxy(sfStart[ii], *yOffp, &xx, &yy);

	sf_len   = sfEnd[ii] - sfStart[ii];
	name_len = strlen(superfam_name[ii]);
	if (sf_len*pbScale < name_len*6) 
	    {
	    show_name = 0;
	    }
	else
	    {
	    show_name = 1;
	    }

	len = strlen(superfam_name[ii]);
	vgDrawBox(g_vg, xx, yy-9+(jj%4)*5, (sfEnd[ii] - sfStart[ii])*pbScale, 9, sfColor);
	mapBoxSuperfamily(xx, yy-9+(jj%4)*5, 
			  (sfEnd[ii] - sfStart[ii])*pbScale, 9,
		 	  superfam_name[ii], sfId[ii]);
    	if (show_name) vgTextRight(g_vg, 
	    			   xx+(sfEnd[ii] - sfStart[ii])*pbScale/2 + (len/2)*5, 
				   yy-9+(jj%4)*5, 10, 10, MG_BLACK, g_font, superfam_name[ii]);
	}
    }
calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-8, xx, 22, bkgColor);

trackTitle = cloneString("Superfamily/SCOP");
vgTextRight(g_vg, xx-25, yy, 10, 10, MG_BLACK, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-2, trackTitleLen*6+12, 14, trackTitle, "superfam");

*yOffp = *yOffp + 20;
}

void doResidues(char *aa, int len, int *yOffp)
/* draw track for AA residue */
{
int index;
    
int xx, yy;
char res_str[2];
    
currentYoffset = *yOffp;
    
calxy(0, *yOffp, &xx, &yy);
    
res_str[1] = '\0';
for (index=0; index < len; index++)
    {
    res_str[0] = aa[index];
    calxy(index+1, *yOffp, &xx, &yy);
	
    /* vgTextRight(g_vg, xx-3-6, yy, 10, 10, MG_BLACK, g_font, res_str); */
    if (pbScale >= 18)
	{
	vgTextRight(g_vg, xx-3-16, yy, 10, 10, MG_BLACK, g_font, res_str);
    	}
    else
	{
        vgTextRight(g_vg, xx-3-6, yy, 10, 10, MG_BLACK, g_font, res_str);
    	}

    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 20, bkgColor);

trackTitle = cloneString("AA Sequence");
vgTextRight(g_vg, xx-25, yy, 10, 10, MG_BLACK, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-2, trackTitleLen*6+12, 14, trackTitle, "aaSeq");
 
*yOffp = *yOffp + 12;
}

void doDnaTrack(char *chrom, char strand, int exonCount, int len, int *yOffp)
/* draw track for AA residue */
{
int xx, yy;
int j;
int mrnaLen;
                       
int exonStartPos, exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
int exonNumber;
int printedExonNumber = -1;
Color exonColor[2];
Color color;
int k;
struct dnaSeq *dna;

char base[2];
char baseComp[2];
int dnaLen;

Color defaultColor;
defaultColor = vgFindColorIx(g_vg, 170, 170, 170);

/* exonColor[0] = pbBlue; */
exonColor[0] = vgFindColorIx(g_vg, 0x00, 0x00, 0xd0);
exonColor[1] = vgFindColorIx(g_vg, 0, 180, 0);

base[1] = '\0';
baseComp[1] = '\0';
currentYoffset = *yOffp;

calxy(0, *yOffp, &xx, &yy);

/* The hypothetical mRNA length is 3 times of aaLen */
mrnaLen = len * 3;
            
exonNumber = 1;

exonStartPos       = blockStartPositive[exonNumber-1];
exonEndPos         = blockEndPositive[exonNumber-1];
exonGenomeStartPos = blockGenomeStartPositive[exonNumber-1];
exonGenomeEndPos   = blockGenomeEndPositive[exonNumber-1];
dna = hChromSeq(database, chrom, exonGenomeStartPos, exonGenomeEndPos+1);
dnaLen = strlen(dna->dna);

k=0;
for (j = 0; j < mrnaLen; j++)
    {
    if (j > exonEndPos)
        {

        if (printedExonNumber != exonNumber)
            {
            printedExonNumber = exonNumber;
            }

        if (exonNumber < exonCount)
            {
            exonNumber++;
            exonStartPos       = blockStartPositive[exonNumber-1];
            exonEndPos         = blockEndPositive[exonNumber-1];
            exonGenomeStartPos = blockGenomeStartPositive[exonNumber-1];
            exonGenomeEndPos   = blockGenomeEndPositive[exonNumber-1];
	    dna = hChromSeq(database, chrom, exonGenomeStartPos, exonGenomeEndPos+1);
    	    dnaLen = strlen(dna->dna);
            k=0;
	    }
        }

    if ((j >= exonStartPos) && (j <= exonEndPos))
        {
	if (strand == '+')
	    {
	    base[0] = toupper(*(dna->dna + k));
	    }
	else
	    {
	    base[0]     = toupper(ntCompTable[(int)*(dna->dna + dnaLen - k -1 )]);
	    baseComp[0] = toupper(*(dna->dna + dnaLen - k -1 ));
	    }

	k++;
        color = exonColor[(exonNumber-1) % 2];
        calxy(j/3, *yOffp, &xx, &yy);

	if (strand == '-') 
	    {
            vgTextRight(g_vg, xx-3+(j%3)*6, yy-3, 10, 10, color, g_font, base);
	    vgTextRight(g_vg, xx-3+(j%3)*6, yy+9, 10, 10, color, g_font, baseComp);
       	    }
	else
	    {
	    vgTextRight(g_vg, xx-3+(j%3)*6, yy-3, 10, 10, color, g_font, base);
            }
        }
    color = pbBlue;
    }
   
calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 30, bkgColor);

if (strand == '-') 
    {
    trackTitle = cloneString("Coding Sequence");
    }
else
    {
    trackTitle = cloneString("Genomic Sequence");
    }
vgTextRight(g_vg, xx-25, yy-4, 10, 10, MG_BLACK, g_font, trackTitle);
trackTitleLen = strlen(trackTitle);
mapBoxTrackTitle(xx-25-trackTitleLen*6, yy-6, trackTitleLen*6+12, 14, trackTitle, "dna");

if (strand == '-') 
    {
    trackTitle = cloneString("Genomic Sequence");
    vgTextRight(g_vg, xx-25, yy+9, 10, 10, MG_BLACK, g_font, trackTitle);
    trackTitleLen = strlen(trackTitle);
    mapBoxTrackTitle(xx-25-trackTitleLen*6, yy+7, trackTitleLen*6+12, 14, trackTitle, "dna");
    }

if (strand == '-')
    {
    *yOffp = *yOffp + 20;
    }
else
    {
    *yOffp = *yOffp + 12;
    }
}

boolean doExonTrack = FALSE;

void doTracks(char *proteinID, char *mrnaID, char *aa, int *yOffp, char *psOutput)
/* draw various protein tracks */
{
int l;

char aaOrigOffsetStr[20];
int hasResFreq;
char uniProtDbName[50];
char *protDbDate;
char *chrom;
char strand;
char *kgId, *kgPep, *protPep;
char cond_str[255];
char *answer;
//int i, ll;
//char *chp1, *chp2;

g_font = mgSmallFont();
safef(pbScaleStr, sizeof(pbScaleStr), "%d", pbScale);

if (psOutput != NULL)
    {
    pbScale         = atoi(cartOptionalString(cart, "pbt.pbScaleStr"));
    }

if (cgiOptionalString("trackOffset") != NULL)
	{
	trackOrigOffset = atoi(cgiOptionalString("trackOffset")); 
	}

if (cgiOptionalString("pbScaleStr") != NULL)
	{
	pbScale  = atoi(cgiOptionalString("pbScaleStr")); 
	}

if (cgiOptionalString("pbScale") != NULL)
    {
    scaleButtonPushed = TRUE;
    if (strcmp(cgiOptionalString("pbScale"), "1/6")  == 0) pbScale = 1;
    if (strcmp(cgiOptionalString("pbScale"), "1/2")  == 0) pbScale = 3;
    if (strcmp(cgiOptionalString("pbScale"), "FULL") == 0) pbScale = 6;
    if (strcmp(cgiOptionalString("pbScale"), "DNA")  == 0) pbScale =22;
    safef(pbScaleStr, sizeof(pbScaleStr), "%d", pbScale);
    cgiMakeHiddenVar("pbScaleStr", pbScaleStr);
    }
else
    {
    scaleButtonPushed = FALSE;
    }

if (psOutput == NULL)
{
if (cgiVarExists("pbt.left3"))
    {
    relativeScroll(-0.95);
    initialWindow = FALSE;
    }
else if (cgiVarExists("pbt.left2"))
    {
    relativeScroll(-0.475);
    initialWindow = FALSE;
    }
else if (cgiVarExists("pbt.left1"))
    {
    relativeScroll(-0.02);
    initialWindow = FALSE;
    }
else if (cgiVarExists("pbt.right1"))
    {
    relativeScroll(0.02);
    initialWindow = FALSE;
    }
else if (cgiVarExists("pbt.right2"))
    {
    relativeScroll(0.475);
    initialWindow = FALSE;
    }
else if (cgiVarExists("pbt.right3"))
    {
    relativeScroll(0.95);
    initialWindow = FALSE;
    }
}

dnaUtilOpen();

l=strlen(aa);

/* initialize AA properties */
aaPropertyInit(&hasResFreq);
sfCount = getSuperfamilies2(proteinID);
if (sfCount == 0)
    {
    sfCount = getSuperfamilies(proteinID);
    }
if (mrnaID != NULL)
    {
    if (kgVersion == KG_III)
    	{
	doExonTrack = FALSE;
	safef(cond_str, sizeof(cond_str), "spId='%s'", proteinID);
        kgId = sqlGetField(database, "kgXref", "kgId", cond_str);
	if (kgId != NULL)
	    {
	    safef(cond_str, sizeof(cond_str), "name='%s'", kgId);
            kgPep = sqlGetField(database, "knownGenePep", "seq", cond_str);
      	    //printf("<pre><br>%s", kgPep);fflush(stdout);
	    if (kgPep != NULL)
	    	{
		if (strstr(protDbName, "proteins") != NULL)
		    {
		    protDbDate = strstr(protDbName, "proteins") + strlen("proteins");
		    safef(uniProtDbName, sizeof(uniProtDbName),"sp%s", protDbDate);
		
		    safef(cond_str, sizeof(cond_str), "acc='%s'", proteinID);
            	    protPep = sqlGetField(uniProtDbName, "protein", "val", cond_str);
            	    //printf("<br>%s\n", protPep);fflush(stdout);
            	    if (protPep != NULL)
		    	{
			if (sameWord(kgPep, protPep))
			    {
			    //printf("<br>MATCH!\n");fflush(stdout);
		    	    safef(cond_str, sizeof(cond_str), "qName='%s'", kgId);
            	    	    answer = sqlGetField(database, kgProtMapTableName, 
			    			 "qName", cond_str);
            	    	    if (answer != NULL)
			    	{
    			    	/* NOTE: passing in kgId instead of proteinID because
					 kgProtMap2's qName uses kgId instead of 
					 protein display ID */
    			    	getExonInfo(kgId, &exCount, &chrom, &strand);
			    	assert(exCount > 0);
				doExonTrack = TRUE;
			    	}
			    }
			/*
			else
			    {
			    chp1 = kgPep;
			    printf("<br>");
			    chp2 = protPep;
			    ll = strlen(kgPep);
			    if (strlen(protPep) < ll) ll= strlen(protPep);
			    for (i=0; i<ll; i++)
			    	{
				if (*chp1 != *chp2)
					{
					printf("%c", *chp1);
					}
				else
					{
					printf(".");
					}
				chp1++; chp2++;
				}
			    }
			    //printf("</pre>");fflush(stdout);
			*/
			}
		    }
		}
	    }
	}
    else
    	{
	doExonTrack = TRUE;
    	getExonInfo(proteinID, &exCount, &chrom, &strand);
    	assert(exCount > 0);
	}
    /* do the following only if pbTracks called doTracks() */
    if (initialWindow && IAmPbTracks)
	{
	prevGBOffsetSav = calPrevGB(exCount, chrom, strand, l, yOffp, proteinID, mrnaID);
	trackOrigOffset = prevGBOffsetSav;
    	if (trackOrigOffset > (protSeqLen*pbScale - 600))
	    trackOrigOffset = protSeqLen*pbScale - 600;
	/* prevent negative value */
	if (trackOrigOffset < 0) trackOrigOffset = 0;
	}

    /* if this if for PDF/Postscript, the trackOrigOffset is already calculated previously,
        use the saved value */
    if (psOutput != NULL)
    	{
    	trackOrigOffset = atoi(cartOptionalString(cart, "pbt.trackOffset"));
    	}
    }

/*printf("<br>%d %d<br>%d %d\n", prevGBStartPos, prevGBEndPos, 
	blockGenomeStartPositive[exCount-1], blockGenomeStartPositive[0]); fflush(stdout);
*/
if (strand == '-')
    {
    if ((prevGBStartPos <= blockGenomeStartPositive[exCount-1]) && (prevGBEndPos >= blockGenomeStartPositive[0]))
    	{
    	showPrevGBPos = FALSE;
    	}
    }
else
    {
    if ((prevGBStartPos <= blockGenomeStartPositive[0]) && (prevGBEndPos >= blockGenomeStartPositive[exCount-1]))
    	{
    	showPrevGBPos = FALSE;
    	}
    }

if ((cgiOptionalString("aaOrigOffset") != NULL) && scaleButtonPushed)
     {
     trackOrigOffset = atoi(cgiOptionalString("aaOrigOffset"))*pbScale;
     }

pixWidth = 160+ protSeqLen*pbScale;
if (pixWidth > MAX_PB_PIXWIDTH)
   {
   pixWidth = MAX_PB_PIXWIDTH;
   }

if ((protSeqLen*pbScale - trackOrigOffset) < MAX_PB_PIXWIDTH)
    {
    pixWidth = protSeqLen*pbScale - trackOrigOffset + 160;
    }

if (pixWidth < 550) pixWidth = 550;

insideWidth = pixWidth-gfxBorder;

if (proteinInSupportedGenome)
    {
    pixHeight = 250;
    }
else
    {
    pixHeight = 215;
    }

if (sfCount > 0) pixHeight = pixHeight + 20;

/* make room for individual residues display */
if (pbScale >=6)  pixHeight = pixHeight + 20;
if (pbScale >=18) pixHeight = pixHeight + 30;

if (psOutput)
    {
    vg = vgOpenPostScript(pixWidth, pixHeight, psOutput);
    suppressHtml = TRUE;
    hideControls = TRUE;
    }
else
    {
    trashDirFile(&gifTn, "pbt", "pbt", ".gif");
    vg = vgOpenGif(pixWidth, pixHeight, gifTn.forCgi);
    }

/* Put up horizontal scroll controls. */
hWrites("Move ");
hButton("pbt.left3", "<<<");
hButton("pbt.left2", " <<");
hButton("pbt.left1", " < ");
hButton("pbt.right1", " > ");
hButton("pbt.right2", ">> ");
hButton("pbt.right3", ">>>");

hPrintf(" &nbsp &nbsp ");

/* Put up scaling controls. */
hPrintf("Current scale: ");
if (pbScale == 1)  hPrintf("1/6 ");
if (pbScale == 3)  hPrintf("1/2 ");
if (pbScale == 6)  hPrintf("FULL ");
if (pbScale == 22) hPrintf("DNA ");

hPrintf(" &nbsp&nbsp Rescale to ");
hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"1/6\">\n");
hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"1/2\">\n");
hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"FULL\">\n");
if (kgVersion == KG_III)
    {
    /* for KG III, the protein has to exist in the kgProtMap2 table 
       (which will turn on doExonTrack flag)
       to provide the genomic position data needed for DNA sequence display */
    if ((proteinInSupportedGenome) && (doExonTrack))
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"DNA\">\n");
    }
else
    {
    if (proteinInSupportedGenome) 
   	hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"DNA\">\n");
    }
hPrintf("<FONT SIZE=1><BR><BR></FONT>\n");

g_vg = vg;

pbRed    = vgFindColorIx(g_vg, 0xf9, 0x51, 0x59);
pbBlue   = vgFindColorIx(g_vg, 0x00, 0x00, 0xd0);
bkgColor = vgFindColorIx(vg, 255, 254, 232);

vgBox(vg, 0, 0, insideWidth, pixHeight, bkgColor);

/* Start up client side map. */
hPrintf("<MAP Name=%s>\n", mapName);

vgSetClip(vg, 0, gfxBorder, insideWidth, pixHeight - 2*gfxBorder);

/* start drawing indivisual tracks */

doAAScale(l, yOffp, 1);

if (pbScale >= 6)  doResidues(aa, l, yOffp);

if (pbScale >= 18) doDnaTrack(chrom, strand, exCount, l, yOffp);

if ((mrnaID != NULL) && showPrevGBPos)
    {
    doPrevGB(exCount, chrom, strand, l, yOffp, proteinID, mrnaID);
    }

if (mrnaID != NULL)
    {
    if (doExonTrack) doExon(exCount, chrom, l, yOffp, proteinID, mrnaID);
    }

doCharge(aa, l, yOffp);

doHydrophobicity(aa, l, yOffp);

doCysteines(aa, l, yOffp);

if (sfCount > 0) doSuperfamily(ensPepName, sfCount, yOffp); 

if (hasResFreq) doAnomalies(aa, l, yOffp);

doAAScale(l, yOffp, -1);

vgClose(&vg);

/* Finish map and save out picture and tell html file about it. */
hPrintf("</MAP>\n");

/* put tracks image here */

hPrintf(
"\n<IMG SRC=\"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s><BR>",
        gifTn.forCgi, pixWidth, pixHeight, mapName);

if (proteinInSupportedGenome)
    {
    hPrintf("<A HREF=\"../goldenPath/help/pbTracksHelpFiles/pbTracksHelp.shtml#tracks\" TARGET=_blank>");
    }
else
    {
    hPrintf("<A HREF=\"../goldenPath/help/pbTracksHelpFiles/pbTracksHelp.shtml#tracks\" TARGET=_blank>");
    }

hPrintf("Explanation of Protein Tracks</A><br>");

safef(trackOffset, sizeof(trackOffset), "%d", trackOrigOffset);
cgiMakeHiddenVar("trackOffset", trackOffset);

/* remember where the AA base origin is so that it can be passed to next PB page */
aaOrigOffset = trackOrigOffset/pbScale;
safef(aaOrigOffsetStr, sizeof(aaOrigOffsetStr), "%d", aaOrigOffset);
cgiMakeHiddenVar("aaOrigOffset", aaOrigOffsetStr);

/* save the following state variables, to be used by PDF/Postcript processing */
cartSetString(cart,"pbt.pbScaleStr", pbScaleStr);
cartSetString(cart,"pbt.trackOffset", trackOffset);
cartSaveSession(cart);
fflush(stdout);
}


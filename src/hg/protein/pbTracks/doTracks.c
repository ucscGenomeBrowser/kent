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

int prevGBOffsetSav;
char trackOffset[20];
int pixWidth, pixHeight;
struct tempName gifTn;
char *mapName = "map";

void calxy(int xin, int yin, int *outxp, int *outyp)
/* calxy() converts a logical drawing coordinate into an actual
   drawing coordinate with scaling and minor adjustments */
{
//*outxp = xin*pbScale + 120;
*outxp = xin*pbScale + 120 - trackOrigOffset;
*outyp = yin         + currentYoffset;
}

// original calxy without trackOrigOffset, may be useful if track names needed at the left
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
int offset;
int newStart, newEnd;

trackOrigOffset = trackOrigOffset + (int)(amount*MAX_PB_PIXWIDTH);

/* Make sure don't scroll of ends. */
//if (trackOrigOffset > ((pbScale* protSeqLen) - MAX_PB_PIXWIDTH))
//    trackOrigOffset = pbScale*protSeqLen - MAX_PB_PIXWIDTH + 150;
if (trackOrigOffset > ((pbScale* protSeqLen) - 600))
    trackOrigOffset = pbScale*protSeqLen - 700;
if (trackOrigOffset < 0) trackOrigOffset = 0;
//printf("<br>pbScale=%d\n", pbScale);
//printf("<br>amount=%f offset=%d\n", amount, trackOrigOffset);fflush(stdout);
//printf("<br>protSeqLen=%d\n", protSeqLen*pbScale);fflush(stdout);

//sprintf(trackOffset, "%d", trackOrigOffset);
//printf("<br>marking Hiden var trackOffset %d\n", trackOrigOffset);fflush(stdout);
//cgiMakeHiddenVar("trackOffset", trackOffset);

sprintf(pbScaleStr, "%d", pbScale);
cgiMakeHiddenVar("pbScaleStr", pbScaleStr);
}

void doAnomalies(char *aa, int len, int *yOffp)
/* draw the AA Anomalies track */
{
char res;
int index;

struct sqlConnection *conn;
char query[56];
struct sqlResult *sr;
char **row;
    
int xx, yy;
int h;
int i, j;
	
float sum;
char *chp;
int aaResFound;
int totalResCnt;
int aaResCnt[20];
double aaResFreqDouble[20];
int abnormal;
int ia = -1;

// count frequency for each residue for current protein
chp = aa;
for (j=0; j<20; j++) aaResCnt[j] = 0;
   
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
    for (j=0; j<20; j++)
	{
	if (res == aaChar[j])
	    {
	    ia = j;
	    break;
	    }
	}
    calxy(index, *yOffp, &xx, &yy);
    
    abnormal = chkAnomaly(aaResFreqDouble[ia], avg[ia], stddev[ia]);
    if (abnormal > 0)
	{
	vgBox(g_vg, xx, yy-5, 1*pbScale, 5, MG_RED);
	}
    else
	{
	if (abnormal < 0)
	    {
	    vgBox(g_vg, xx, yy, 1*pbScale, 5, MG_BLUE);
	    }
	}
    vgBox(g_vg, xx, yy, 1*pbScale, 1, MG_BLACK);
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 20, bkgColor);
vgTextRight(g_vg, xx-25, yy-4, 10, 10, MG_BLACK, g_font, "AA Anomalies");

// update y offset
*yOffp = *yOffp + 15;
}

void doCharge(char *aa, int len, int *yOffp)
// draw polarity track
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
	vgBox(g_vg, xx, yy-9, 1*pbScale, 9, MG_RED);
	}
    else
    if (aa_attrib[(int)res] == CHARGE_NEG)
	{
	vgBox(g_vg, xx, yy, 1*pbScale, 9, MG_BLUE);
	}
    else
    if (aa_attrib[(int)res] == POLAR)
	{
	vgBox(g_vg, xx, yy, 1*pbScale, 5, MG_BLUE);
	}
    else
    if (aa_attrib[(int)res] == NEUTRAL)
	{
	vgBox(g_vg, xx, yy, 1*pbScale, 1, MG_BLUE);
	}
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 30, bkgColor);
vgTextRight(g_vg, xx-25, yy-4, 10, 10, MG_BLACK, g_font, "Polarity");
vgTextRight(g_vg, xx-14, yy-10, 10, 10, MG_RED,  g_font, "+");
vgTextRight(g_vg, xx-14, yy, 10, 10, MG_BLUE, g_font, "-");

*yOffp = *yOffp + 15;
}

void doHydrophobicity(char *aa, int len, int *yOffp)
// draw Hydrophobicity track
{
char res;
int index;
    
int xx, yy;
int h;
int i, i0, i9, j;
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
	    vgBox(g_vg, xx, yy-h, 1*pbScale, h, MG_BLUE);
	    }
	else
	    {
	    h = - 5 * 9 * (100.0 * avg / 9.0) / 100;
	    vgBox(g_vg, xx, yy, 1*pbScale, h, MG_RED);
	    }
	}
    }
calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-17, xx, 40, bkgColor);
vgTextRight(g_vg, xx-25, yy-7, 10, 10, MG_BLACK, g_font, "Hydrophobicity");
*yOffp = *yOffp + 15;
}

void doCysteines(char *aa, int len, int *yOffp)
// draw track for Cysteines and Glycosylation
{
char res;
int index;
    
int xx, yy;
int h;
int iw = 5;
float sum;
	
currentYoffset = *yOffp;
    
for (index=0; index < len; index++)
    {
    res = aa[index];
    calxy(index, *yOffp, &xx, &yy);
    if (res == 'C')
	{
	vgBox(g_vg, xx, yy-9+1, 1*pbScale, 9, MG_RED);
	}
    else
	{
	vgBox(g_vg, xx, yy-0, 1, 1, MG_BLACK);
	}
    }

for (index=1; index < (len-1); index++)
    {
    calxy(index, *yOffp, &xx, &yy);
    if (aa[index-1] == 'N')
	{
	if ( (aa[index+1] == 'T') || (aa[index+1] == 'S') )
	    {
	    if (aa[index] != 'P')
		{
		vgBox(g_vg, xx-1, yy, 3*pbScale, 9, MG_BLUE);
		}
	    }
	}
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-15, xx, 33, bkgColor);
vgTextRight(g_vg, xx-25, yy-8, 10, 10, MG_RED, g_font, "Cysteines");
vgTextRight(g_vg, xx-25, yy, 10, 10, MG_BLUE, g_font, "Glycosylation");
vgTextRight(g_vg, xx-25, yy+10, 10, 10, MG_BLUE, g_font, "(potential)");
    
*yOffp = *yOffp + 15;
}

void doAAScale(int len, int *yOffp, int top_bottom)
// draw the track to show AA scale
{
char res;
int index;
   
int tb;	// top or bottom flag
  
int xx, yy;
int h;
int i, i0, i9, j;
int imax;
int interval;
   
char scale_str[20];
int iw = 5;
float sum;

tb = 0;
if (top_bottom < 0) tb = 1;
   
currentYoffset = *yOffp;
    
imax = len/100 * 100;
if ((len % 100) != 0) imax = imax + 100;
    
calxy(1, *yOffp, &xx, &yy);
vgBox(g_vg, xx-pbScale/2, yy-tb, (len-1)*pbScale+pbScale/2, 1, MG_BLACK);

interval = 50;
if (pbScale >= 18) interval = 10;    
for (i=0; i<len; i++)
    {
    index = i+1;
    if ((index % interval) == 1)
	{
	if (((index % (interval*2)) == 1) || (index == len)) 
	    {
	    calxy(index, *yOffp, &xx, &yy);
	    vgBox(g_vg, xx-pbScale/2, yy-9*tb, 1, 9, MG_BLACK);
	    }
	else
	    {
	    calxy(index, *yOffp, &xx, &yy);
	    vgBox(g_vg, xx-pbScale/2, yy-5*tb, 1, 5, MG_BLACK);
	    }
    		
	sprintf(scale_str, "%d", index);
	vgText(g_vg, xx-pbScale/2+4, yy+4-12*tb, MG_BLACK, g_font, scale_str);
	}
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-tb*9-1, xx, 20, bkgColor);
vgTextRight(g_vg, xx-25, yy-9*tb, 10, 10, MG_BLACK, g_font, "AA Scale");

*yOffp = *yOffp + 12;
}

void mapBoxExon(int x, int y, int width, int height, char *mrnaID, 
		int exonNum, char *chrom, int exonGenomeStartPos, int exonGenomeEndPos)
{
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);
hPrintf("HREF=\"../cgi-bin/hgTracks?db=%s&position=%s:%d-%d\"" 
	,database, chrom, exonGenomeStartPos-1, exonGenomeEndPos+3);
//hPrintf("HREF=\"http://hgwdev-markd.cse.ucsc.edu/cgi-bin/hgTracks?db=%s&position=%s:%d-%d&%s=full" 
//	,database, chrom, exonGenomeStartPos-1, exonGenomeEndPos+3, kgProtMapTableName);
//hPrintf("&knownGene=full&mrna=full\"");
hPrintf(" target=_blank ALT=\"Exon %d\">\n", exonNum);
}

void mapBoxPrevGB(int x, int y, int width, int height, char *posStr)
{
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);
hPrintf("HREF=\"../cgi-bin/hgTracks?db=%s&position=%s\"", database, posStr);
hPrintf(" target=_blank ALT=\"UCSC Genome Browser %s\">\n", posStr);
}

int calPrevGB(int exonCount, char *chrom, char strand, int aaLen, int *yOffp, char *proteinID, char *mrnaID)
// calculate the appropriate X offset for the previous Genome Browser position range
{
int xx, yy, xx0, xxSave;
int i, j;
char prevPosMessage[200];
char exonNumStr[10];
int mrnaLen;

int exonStartPos, exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
int exonNumber;
int printedExonNumber = -1;
int currentPos;
int currentPBPos;
int jPrevStart, jPrevEnd;
int iPrevExon = -1;
int jPrevExonPos = 0;
int prevGBOffset;
int jcnt = 0;

// The imaginary mRNA length is 3 times of aaLen
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

    // handle special cases at both ends when previous GB position is outside CDS
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

    // handle special cases at both ends when previous GB position is outside CDS
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
	    	sprintf(exonNumStr, "%d", exonNumber);
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
// draw the previous Genome Browser position range
{
int xx, yy, xx0, xxSave;
int i, j;
char prevPosMessage[200];
char exonNumStr[10];
int mrnaLen;
Color color;

int exonStartPos, exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
int exonNumber;
int printedExonNumber = -1;
int exonColor[2];
int currentPos;
int currentPBPos;
int jPrevStart, jPrevEnd;
int jcnt = 0;
int iPrevExon = -1;
int jPrevExonPos = 0;

int defaultColor;
defaultColor = vgFindColorIx(g_vg, 170, 170, 170);

// The imaginary mRNA length is 3 times of aaLen
mrnaLen = aaLen * 3;

exonColor[0] = MG_BLUE;
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
    	    //printf("<br>*%d %d %d %d\n",i,blockGenomeEndPositive[i], 
	    //prevGBStartPos, blockGenomeStartPositive[i]);fflush(stdout);
	    }
    	//printf("<br>%d %d %d",i,blockGenomeStartPositive[i],
	//	blockGenomeEndPositive[i]);fflush(stdout);
    	}

    // handle special cases at both ends when previous GB position is outside CDS
    if (prevGBStartPos < blockGenomeStartPositive[exonCount-1]) 
    	jPrevExonPos = blockEndPositive[exonCount-1] + 3;
    if (prevGBEndPos > blockGenomeStartPositive[0]) 
    	jPrevExonPos = blockStartPositive[0];

    //printf("<br>jPrevExonPos = %d\n", jPrevExonPos);fflush(stdout);
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

    // handle special cases at both ends when previous GB position is outside CDS
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
	    	sprintf(exonNumStr, "%d", exonNumber);
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

positionStr = strdup(cartOptionalString(cart, "position"));
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
    sprintf(prevPosMessage, "You were at: %s", positionStr);
    vgText(g_vg, xx+(jPrevStart%3)*pbScale/3, yy-10, MG_BLACK, g_font, prevPosMessage);
    /*if (jPrevStart < (mrnaLen/2))
   	{
   	vgText(g_vg, xx+(jPrevStart%3)*pbScale/3, yy-10, MG_BLACK, g_font, prevPosMessage);
   	}
    else
   	{
   	calxy(jPrevEnd/3, *yOffp, &xx, &yy);
   	vgTextRight(g_vg, xx-6, yy-10, 10, 10, MG_BLACK, g_font, prevPosMessage);
   	}
    */
    }
else
    {
    //calxy(jPrevExonPos/3, *yOffp, &xx, &yy);
    if (jPrevExonPos <= 0)
	{
        calxy(jPrevExonPos/3, *yOffp, &xx, &yy);
	vgBox(g_vg,  xx, yy, 1, 5, MG_RED);
	}
    else
	{
        calxy(jPrevExonPos/3+1, *yOffp, &xx, &yy);
	vgBox(g_vg,  xx, yy, 1, 5, MG_RED);
	}

    mapBoxPrevGB(xx-1, yy-1, 2, 6, positionStr);
    
    sprintf(prevPosMessage, "You were at: %s (not in a CDS)", positionStr);
    calxy(0, *yOffp, &xx0, &yy);
    calxy(jPrevExonPos/3, *yOffp, &xx, &yy);
    xxSave = xx - 6*strlen(prevPosMessage);
    if (xxSave < xx0) 
	{
    	vgText(g_vg, xx0, yy-8, MG_BLACK, g_font, prevPosMessage);
	}
    else
	{
    	vgTextCentered(g_vg, xx, yy-8, 10, 10, MG_BLACK, g_font, prevPosMessage);
    	}
    }

calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 20, bkgColor);
vgTextRight(g_vg, xx-25, yy-8, 10, 10, MG_BLACK, g_font, "Genome Browser");

*yOffp = *yOffp + 7;
}

void doExon(int exonCount, char *chrom, int aaLen, int *yOffp, char *proteinID, char *mrnaID)
// draw the track for exons
{
int xx, yy;
int i, j;
char exonNumStr[10];
int mrnaLen;
Color color;

int exonStartPos, exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
int exonNumber;
int printedExonNumber = -1;
int exonColor[2];

int defaultColor;
defaultColor = vgFindColorIx(g_vg, 170, 170, 170);

// The imaginary mRNA length is 3 times of aaLen
mrnaLen = aaLen * 3;

exonColor[0] = MG_BLUE;
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
	    	sprintf(exonNumStr, "%d", exonNumber);
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
    sprintf(exonNumStr, "%d", exonNumber);
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
vgTextRight(g_vg, xx-25, yy-9, 10, 10, MG_BLACK, g_font, "Exons");

*yOffp = *yOffp + 10;
}

#define MAX_SF 200
#define MAXNAMELEN 256

int sfId[MAX_SF];
int sfStart[MAX_SF], sfEnd[MAX_SF];
char superfam_name[MAX_SF][256];

struct sqlConnection *conn, *conn2;

int getSuperfamilies(char *proteinID)
{
char *before, *after = "", *s;
char startString[64], endString[64];

struct sqlConnection *conn, *conn2;
char query[MAXNAMELEN], query2[MAXNAMELEN];
struct sqlResult *sr, *sr2;
char **row, **row2;

char cond_str[255];

char *genomeID, *seqID, *modelID, *start, *end, *eValue, *sfID, *sfDesc;

char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd,
     *exonCount, *exonStarts, *exonEnds;
char *region;
int  done;

char *gene_name;
char *ensPep;
char *transcriptName;

char *chp, *chp2;
int  i,l;
int  ii = 0;
int  int_start, int_end;
    
conn  = hAllocConn();
conn2 = hAllocConn();

// two steps query needed because the recent Ensembl gene_xref 11/2003 table does not have 
// valid translation_name
sprintf(cond_str, "external_name='%s'", protDisplayID);
transcriptName = sqlGetField(conn, database, "ensGeneXref", "transcript_name", cond_str);
if (transcriptName == NULL)
    {
    return(0); 
    }
else
    {
    sprintf(cond_str, "transcript_name='%s';", transcriptName);
    ensPep = sqlGetField(conn, database, "ensTranscript", "translation_name", cond_str);
    if (ensPep == NULL) 
	{
	hFreeConn(&conn);
    	return(0); 
    	}
    }

ensPepName = ensPep;

sprintf(query, "select * from %s.sfAssign where seqID='%s' and evalue <= 0.02;", database, ensPep);
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
    //sfDesc   = row[6];
    // !!! the recent Suprefamily sfAssign table does not have valid sf description
    sprintf(cond_str, "id=%s;", sfID);
    sfDesc = sqlGetField(conn2, database, "sfDes", "description", cond_str);

    //!!! refine logic here later to be defensive against illegal syntax
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

hFreeConn(&conn);
hFreeConn(&conn2);
sqlFreeResult(&sr);
  
return(ii);
}
    

void mapBoxSuperfamily(int x, int y, int width, int height, char *sf_name, int sfID)
{
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);

hPrintf("HREF=\"%s?sunid=%d\"",
	"http://supfam.org/SUPERFAMILY/cgi-bin/scop.cgi", sfID);
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
// draw the Superfamily track
{
int xx, yy;
int h;
int i, ii, jj, i0, i9, j;
char exonNumStr[10];
int len;
int sf_len, name_len;
int show_name;
    
Color color2;
    
color2 = vgFindColorIx(g_vg, 0, 180, 0);
   
currentYoffset = *yOffp;
   
calxy(0, *yOffp, &xx, &yy);
    
jj = 0;
for (ii=0; ii<sf_cnt; ii++)
    {
    if (sfEnd[ii] != 0)
	{
	jj++;
	sprintf(exonNumStr, "%d", jj);
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
	vgDrawBox(g_vg, xx, yy-9+(jj%3)*4, (sfEnd[ii] - sfStart[ii])*pbScale, 9, MG_YELLOW);
	mapBoxSuperfamily(xx, yy-9+(jj%3)*4, 
			  (sfEnd[ii] - sfStart[ii])*pbScale, 9,
		 	  superfam_name[ii], sfId[ii]);
    	if (show_name) vgTextRight(g_vg, 
	    			   //xx+(sfEnd[ii] - sfStart[ii])*pbScale/2 + (len/2)*5 - 5, 
	    			   xx+(sfEnd[ii] - sfStart[ii])*pbScale/2 + (len/2)*5, 
				   yy-9+(jj%3)*4, 10, 10, MG_BLACK, g_font, superfam_name[ii]);
	}
    }
calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 20, bkgColor);
vgTextRight(g_vg, xx-25, yy-9, 10, 10, MG_BLACK, g_font, "Superfamily/SCOP");

*yOffp = *yOffp + 20;
}

void doResidues(char *aa, int len, int *yOffp)
// draw track for AA residue
{
char res;
int index;
    
int xx, yy;
int h;
int i, i0, i9, j;

char res_str[2];
    
int iw = 5;
float sum;
	
currentYoffset = *yOffp;
    
calxy(0, *yOffp, &xx, &yy);
    
res_str[1] = '\0';
for (index=0; index < len; index++)
    {
    res_str[0] = aa[index];
    calxy(index+1, *yOffp, &xx, &yy);
	
    //vgTextRight(g_vg, xx-3-6, yy, 10, 10, MG_BLACK, g_font, res_str);
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
vgTextRight(g_vg, xx-25, yy, 10, 10, MG_BLACK, g_font, "Protein Sequence");
 
*yOffp = *yOffp + 12;
}

void doDnaTrack(char *chrom, char strand, int exonCount, int len, int *yOffp)
// draw track for AA residue
{
char res;
int index;

int xx, yy;
int h;
int i, i0, i9, j;
int mrnaLen;
char exonNumStr[10];
                       
int exonStartPos, exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
int exonNumber;
int printedExonNumber = -1;
int exonColor[2];
int color;
int k;
struct dnaSeq *dna;

char res_str[2];
char base[2];
char baseComp[2];
int iw = 5;
float sum;
int dnaLen;

int defaultColor;
defaultColor = vgFindColorIx(g_vg, 170, 170, 170);

exonColor[0] = MG_BLUE;
exonColor[1] = vgFindColorIx(g_vg, 0, 180, 0);

base[1] = '\0';
baseComp[1] = '\0';
currentYoffset = *yOffp;

calxy(0, *yOffp, &xx, &yy);

// The imaginary mRNA length is 3 times of aaLen
mrnaLen = len * 3;
            
exonNumber = 1;

exonStartPos       = blockStartPositive[exonNumber-1];
exonEndPos         = blockEndPositive[exonNumber-1];
exonGenomeStartPos = blockGenomeStartPositive[exonNumber-1];
exonGenomeEndPos   = blockGenomeEndPositive[exonNumber-1];
dna = hChromSeq(chrom, exonGenomeStartPos, exonGenomeEndPos+1);
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
	    dna = hChromSeq(chrom, exonGenomeStartPos, exonGenomeEndPos+1);
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
	    base[0]     = toupper(ntCompTable[*(dna->dna + dnaLen - k -1 )]);
	    baseComp[0] = toupper(*(dna->dna + dnaLen - k -1 ));
	    }

	k++;
        color = exonColor[(exonNumber-1) % 2];
        calxy(j/3, *yOffp, &xx, &yy);
        vgTextRight(g_vg, xx-3+(j%3)*6, yy, 10, 10, color, g_font, base);
        if (strand == '-') vgTextRight(g_vg, xx-3+(j%3)*6, yy+9, 10, 10, color, g_font, baseComp);
        }
    color = MG_BLUE;
    }
   
calxy0(0, *yOffp, &xx, &yy);
vgBox(g_vg, 0, yy-10, xx, 30, bkgColor);
vgTextRight(g_vg, xx-25, yy, 10, 10, MG_BLACK, g_font, "DNA Sequence");
if (strand == '-') vgTextRight(g_vg, xx-25, yy+9, 10, 10, MG_BLACK, g_font, "& complement");
 
if (strand == '-')
    {
    *yOffp = *yOffp + 20;
    }
else
    {
    *yOffp = *yOffp + 12;
    }
}

void doTracks(char *proteinID, char *mrnaID, char *aa, int *yOffp)
/* draw various protein tracks */
{
int i,j,l;
char *exonNumStr;
int exonNum;

double pI, aaLen;
double exonCount;
char *chp;
int len;
int cCnt;

int xPosition;
int yPosition;

int aaResCnt[30];
double aaResFreqDouble[30];
int aaResFound;
int totalResCnt;
int hasResFreq;

char *aap;
double molWeight, hydroSum;
struct pbStamp *stampDataPtr;
char *chrom;
char strand;

g_font = mgSmallFont();

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
	if (strcmp(cgiOptionalString("pbScale"), "1/6")  == 0) pbScale = 1;
	if (strcmp(cgiOptionalString("pbScale"), "1/2")  == 0) pbScale = 3;
	if (strcmp(cgiOptionalString("pbScale"), "FULL") == 0) pbScale = 6;
	if (strcmp(cgiOptionalString("pbScale"), "DNA")  == 0) pbScale =22;
	sprintf(pbScaleStr, "%d", pbScale);
	cgiMakeHiddenVar("pbScaleStr", pbScaleStr);
	}

if (cgiVarExists("hgt.left3"))
    {
    relativeScroll(-0.95);
    initialWindow = FALSE;
    }
else if (cgiVarExists("hgt.left2"))
    {
    relativeScroll(-0.475);
    initialWindow = FALSE;
    }
else if (cgiVarExists("hgt.left1"))
    {
    //relativeScroll(-0.1);
    relativeScroll(-0.02);
    initialWindow = FALSE;
    }
else if (cgiVarExists("hgt.right1"))
    {
    //relativeScroll(0.1);
    relativeScroll(0.02);
    initialWindow = FALSE;
    }
else if (cgiVarExists("hgt.right2"))
    {
    relativeScroll(0.475);
    initialWindow = FALSE;
    }
else if (cgiVarExists("hgt.right3"))
    {
    relativeScroll(0.95);
    initialWindow = FALSE;
    }

dnaUtilOpen();

l=strlen(aa);

// initialize AA properties
aaPropertyInit(&hasResFreq);

sfCount = getSuperfamilies(proteinID);

if (mrnaID != NULL)
    {
    getExonInfo(proteinID, &exCount, &chrom, &strand);
    if (initialWindow) 
	{
	prevGBOffsetSav = calPrevGB(exCount, chrom, strand, l, yOffp, proteinID, mrnaID);
	trackOrigOffset = prevGBOffsetSav;
    	}
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

pixHeight = 260;

if (sfCount > 0) pixHeight = pixHeight + 20;

//make room for individual residues display
if (pbScale >=6)  pixHeight = pixHeight + 20;
if (pbScale >=18) pixHeight = pixHeight + 30;

makeTempName(&gifTn, "hgt", ".gif");
vg = vgOpenGif(pixWidth, pixHeight, gifTn.forCgi);
g_vg = vg;

bkgColor = vgFindColorIx(vg, 255, 254, 232);
vgBox(vg, 0, 0, insideWidth, pixHeight, bkgColor);

/* Start up client side map. */
hPrintf("<MAP Name=%s>\n", mapName);

vgSetClip(vg, 0, gfxBorder, insideWidth, pixHeight - 2*gfxBorder);

// start drawing indivisual tracks

doAAScale(l, yOffp, 1);

if (pbScale >= 6)  doResidues(aa, l, yOffp);

if (pbScale >= 18) doDnaTrack(chrom, strand, exCount, l, yOffp);

if (mrnaID != NULL)
    {
    doPrevGB(exCount, chrom, strand, l, yOffp, proteinID, mrnaID);
    }

sprintf(trackOffset, "%d", trackOrigOffset);
cgiMakeHiddenVar("trackOffset", trackOffset);

if (mrnaID != NULL)
    {
    doExon(exCount, chrom, l, yOffp, proteinID, mrnaID);
    }

doCharge(aa, l, yOffp);

doHydrophobicity(aa, l, yOffp);

doCysteines(aa, l, yOffp);

if (sfCount > 0) doSuperfamily(ensPepName, sfCount, yOffp); 

if (hasResFreq) doAnomalies(aa, l, yOffp);

doAAScale(l, yOffp, -1);

/* Finish map and save out picture and tell html file about it. */
hPrintf("</MAP>\n");
vgClose(&vg);

hPrintf(
"<IMG SRC=\"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s onMouseOut=\"javascript:popupoff();\"><BR>",
//hPrintf("<IMG SRC=\"%s\" BORDER=1 WIDTH=%d HEIGHT=%d USEMAP=#%s><BR>",
        gifTn.forCgi, pixWidth, pixHeight, mapName);

hPrintf("<A HREF=\"../goldenPath/help/pbTracksHelp.html\" TARGET=_blank>");
hPrintf("Explanation of Tracks</A><br>");

/* Put up horizontal scroll controls. */
hWrites("Move ");
hButton("hgt.left3", "<<<");
hButton("hgt.left2", " <<");
hButton("hgt.left1", " < ");
hButton("hgt.right1", " > ");
hButton("hgt.right2", ">> ");
hButton("hgt.right3", ">>>");

hPrintf(" &nbsp &nbsp &nbsp &nbsp ");

/* Put up scaling controls. */
hPrintf("Current scale: ");
if (pbScale == 1) hPrintf("1/6 ");
if (pbScale == 3) hPrintf("1/2 ");
if (pbScale == 6) hPrintf("FULL ");

hPrintf("&nbsp&nbsp&nbsp Rescale to ");
hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"1/6\">\n");
hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"1/2\">\n");
hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"FULL\">\n");
hPrintf("<INPUT TYPE=SUBMIT NAME=\"pbScale\" VALUE=\"DNA\">\n");
hPrintf("<P>");
}

/* doStamps.c draw various stamps for Proteome Browser */
#include "common.h"
#include "hCommon.h"
#include "portable.h"
#include "memalloc.h"
#include "jksql.h"
#include "vGfx.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "web.h"
#include "hgColors.h"
#include "pbStamp.h"
#include "pbStampPict.h"
#include "pbTracks.h"

Color boundaryColor;

void setPbStampPict(struct pbStampPict *stampPictPtr, struct pbStamp *stampDataP, int ix, int iy, int iw, int ih)
/* set drawing parameters for a stamp */
{
if (stampPictPtr == NULL) errAbort("In setPbStampPict(), stampPictPtr is NULL.");

stampPictPtr->xOrig        = ix;
stampPictPtr->yOrig        = iy;
stampPictPtr->width        = iw;
stampPictPtr->height       = ih;
stampPictPtr->stampDataPtr = stampDataP;
}

void calStampXY(struct pbStampPict *PictPtr, double xin, double yin, int *outxp, int *outyp)
/* coordinate conversion from stamp data space to picture space */
{
double xScale0, yScale0;
xScale0 = (double)(PictPtr->width) /(PictPtr->stampDataPtr->xmax - PictPtr->stampDataPtr->xmin);
yScale0 = (double)(PictPtr->height)/(PictPtr->stampDataPtr->ymax - PictPtr->stampDataPtr->ymin);
*outxp =  (int)((xin - PictPtr->stampDataPtr->xmin)*xScale0) + PictPtr->xOrig;
*outyp = -(int)((yin - PictPtr->stampDataPtr->ymin)*yScale0) + PictPtr->yOrig;
}

void pbBox(double fx, double fy, double fw, double fh, int color)
/* draw a box based on data coordinates */
{
int x, y, w, h;
calStampXY(stampPictPtr, fx, fy, &x, &y);

w = (int)(fw * xScale);
h = (int)(fh * yScale);
vgBox(g_vg, x, y-h, w+1, h, color);
}

void hLine(double fx, double fy, double fw, double ih, int color)
/* draw a horizontal line based on data coordinates */
{
int x, y, w, h;
calStampXY(stampPictPtr, fx, fy, &x, &y);

w = (int)(fw * xScale);
h = ih;
vgBox(g_vg, x, y-h, w+1, h, color);
}

void vLine(double fx, double fy, double fh, int iw, int color)
/* draw a vertical line based on data coordinates */
{
int x, y, w, h;
calStampXY(stampPictPtr, fx, fy, &x, &y);

w = iw, 
h = (int)(fh * yScale);
vgBox(g_vg, x, y-h-1, w, h+1, color);
}

void vLineM(double fx, double fy, double fh, int iw, int color)
/* draw a vertical line based on data coordinates, for Marker lines */
{
int x, y, w, h;
calStampXY(stampPictPtr, fx, fy, &x, &y);

w = iw, 
h = (int)(fh * yScale);
vgBox(g_vg, x, y-h-1, w, h-1, color);
}

void drawXScale(struct pbStamp *pbStampPtr, struct pbStampPict *stampPictPtr, int increment)
/* mark the X axis scale */
{
int i;
double txmin, tymin, txmax, tymax;
int xx,  yy;
char labelStr[20];
   
txmin	= pbStampPtr->xmin;
txmax	= pbStampPtr->xmax;
tymin       = pbStampPtr->ymin;
tymax       = pbStampPtr->ymax;
 
for (i=(int)txmin; i<(int)txmax; i=i+increment)
    {
    vLineM((double)i+0.5, 0.0, (tymax-tymin)*0.05, 1, boundaryColor);
    calStampXY(stampPictPtr, (double)i+0.5, 0, &xx, &yy);
    sprintf(labelStr, "%d", i);
    vgTextCentered(g_vg, xx-5, yy+5, 10, 10, MG_BLACK, g_font, labelStr);
    }
}

void drawXScaleMW(struct pbStamp *pbStampPtr, struct pbStampPict *stampPictPtr, int increment)
/* mark the X axis scale */
{
int i;
double txmin, tymin, txmax, tymax;
int xx,  yy;
char labelStr[20];
   
txmin	= pbStampPtr->xmin;
txmax	= pbStampPtr->xmax;
tymin   = pbStampPtr->ymin;
tymax   = pbStampPtr->ymax;
 
for (i=(int)txmin; i<=(int)txmax; i=i+increment)
    {
    vLineM((double)i+0.5, 0.0, (tymax-tymin)*0.05, 1, boundaryColor);
    calStampXY(stampPictPtr, (double)i+0.5, 0, &xx, &yy);
    sprintf(labelStr, "%dK", i/1000);
    vgTextCentered(g_vg, xx-5, yy+5, 10, 10, MG_BLACK, g_font, labelStr);
    }
}

void drawXScaleHydro(struct pbStamp *pbStampPtr, struct pbStampPict *stampPictPtr, double increment)
/* mark the X axis scale */
{
int i;
double txmin, tymin, txmax, tymax;
double xValue;
int xx,  yy;
char labelStr[20];
int i0, i9;

txmin	= pbStampPtr->xmin;
txmax	= pbStampPtr->xmax;
tymin       = pbStampPtr->ymin;
tymax       = pbStampPtr->ymax;
    
i0 =0;
i9 =(int)((txmax - txmin)/increment);
 
for (i=0; i<=i9; i++)
    {
    vLineM((double)i*increment+txmin, 0.0, (tymax-tymin)*0.05, 1, boundaryColor);
    xValue = txmin + increment*(double)i;
    calStampXY(stampPictPtr, xValue, 0, &xx, &yy);
    sprintf(labelStr, "%.1f", xValue);
    vgTextCentered(g_vg, xx-5, yy+5, 10, 10, MG_BLACK, g_font, labelStr);
    }
}

void markResStamp(struct pbStamp *pbStampPtr, struct pbStampPict *stampPictPtr,
 		  int iTarget, double yValue, double tx[], double ty[], 
		  double avg[], double stddev[])
/* mark the AA residual stamp */
{
char res;
int index;
int ix, iy; 
double txmin, tymin, txmax, tymax;
double yPlotValue;
int len;
int xx,  yy;
   
len	= pbStampPtr->len; 
txmin	= pbStampPtr->xmin;
txmax	= pbStampPtr->xmax;
tymin	= pbStampPtr->ymin;
tymax	= pbStampPtr->ymax;
    
ix	= stampPictPtr->xOrig;
iy	= stampPictPtr->yOrig;
   
calStampXY(stampPictPtr, (txmax-txmin)/2.0, tymax, &xx, &yy);
  
if (yValue > tymax) 
    {
    yPlotValue = tymax;
    }
else
    {
    yPlotValue = yValue;
    }
    
if (yValue >= (avg[iTarget] + 2.0*stddev[iTarget]))
    {
    vLineM(tx[iTarget]+0.4, 0, yPlotValue, 3, MG_RED);
    }
else
    {
    if (yValue <= (avg[iTarget] - 2.0*stddev[iTarget]))
	{
	vLineM(tx[iTarget]+0.4, 0, yPlotValue, 3, MG_GREEN);
	}
    else
	{
	vLineM(tx[iTarget]+0.5, 0, yPlotValue, 2, MG_BLUE);
	}
    }
}

void markResStdvStamp(struct pbStamp *pbStampPtr, struct pbStampPict *stampPictPtr,
 		  int iTarget, double yValueIn, double tx[], double ty[], 
		  double avg[], double stddev[])
/* mark the AA residual stddev stamp */
{
char res;
int index;
int ix, iy; 
double txmin, tymin, txmax, tymax;
double yValue, yPlotValue;
int len;
int xx,  yy;
   
len	= pbStampPtr->len; 
txmin	= pbStampPtr->xmin;
txmax	= pbStampPtr->xmax;

// force fit for the stddev stamp plot
tymin	= -4.0;
tymax	=  4.0;
   
ix	= stampPictPtr->xOrig;
iy	= stampPictPtr->yOrig;
   
yScale = (double)(120)/8.0;
calStampXY(stampPictPtr, (txmax-txmin)/2.0, tymax, &xx, &yy);
  
yValue = (yValueIn - avg[iTarget])/stddev[iTarget];
if (yValue > tymax)
    {
    yPlotValue = tymax;
    }
else
    {
    if (yValue < tymin)
        {
        yPlotValue = tymin;
        }
    else
        {
        yPlotValue = yValue;
        }
    }
    
if (yPlotValue > 0.0)
    {
    vLine(tx[iTarget]+0.4, 0.0, yPlotValue, 3, MG_BLUE);
    }
else
    {
    vLine(tx[iTarget]+0.4, 0.0+yPlotValue, -yPlotValue, 3, MG_BLUE);
    }

if (yPlotValue >= 2.0)
    {
    vLine(tx[iTarget]+0.4, 0.0, yPlotValue, 3, MG_RED);
    }
else
    {
    if (yPlotValue <= -2.0)
  	{
	vLine(tx[iTarget]+0.4, 0.0+yPlotValue, -yPlotValue, 3, MG_GREEN);
	}
    }
}

void markStamp(struct pbStamp *pbStampPtr, struct pbStampPict *stampPictPtr,
   	       double xValue, char *valStr, double tx[], double ty[])
/* mark the the stamp with a vertical line */
{
int ix, iy;
int index;
  
double txmin, tymin, txmax, tymax;
double ytop=0.0;
int len;
int xx,  yy;
int i, iTarget;

len   = pbStampPtr->len; 
txmin = pbStampPtr->xmin;
txmax = pbStampPtr->xmax;
tymin = pbStampPtr->ymin;
tymax = pbStampPtr->ymax;
  
calStampXY(stampPictPtr, txmin+(txmax-txmin)/2.0, tymax, &xx, &yy);
    vgTextCentered(g_vg, xx-5, yy+3, 10, 10, MG_BLUE, g_font, valStr);
    
ix	= stampPictPtr->xOrig;
iy	= stampPictPtr->yOrig;

calStampXY(stampPictPtr, (txmax-txmin)/2.0, tymax, &xx, &yy);
   
iTarget = -1;

for (i=0; i<(len-1); i++)
    {
    if ((xValue >= tx[i]) && (xValue <= tx[i+1]))
        {
	iTarget = i;
	}
    }
if (iTarget != -1)
    {
    if (ty[iTarget] > 0.1 * (tymax - tymin))
	{
	ytop = ty[iTarget];
	}
    else
  	{
	ytop = 0.1 * (tymax - tymin) + tymin;
	}
    }
vLineM(tx[iTarget]+(tx[iTarget+1]-tx[iTarget])/2.0, 0, ytop, 2,  MG_BLUE);
}

void markStamp0(struct pbStamp *pbStampPtr, struct pbStampPict *stampPictPtr,
	        double xValue, char *valStr, double tx[], double ty[])
/* mark the the stamp with valStr only, used for 'N/A' case only */
{
int ix, iy;
int index;
  
double txmin, tymin, txmax, tymax;
double ytop=0.0;
int len;
int xx,  yy;
int i, iTarget;
 
len   = pbStampPtr->len; 
txmin = pbStampPtr->xmin;
txmax = pbStampPtr->xmax;
tymin = pbStampPtr->ymin;
tymax = pbStampPtr->ymax;
 
calStampXY(stampPictPtr, txmin+(txmax-txmin)/2.0, tymax, &xx, &yy);
vgTextCentered(g_vg, xx-5, yy+3, 10, 10, MG_BLUE, g_font, valStr);
}

struct pbStamp *getStampData(char *stampName)
/* get data for a stamp */
{
struct sqlConnection *conn2;
char query2[256];
struct sqlResult *sr2;
char **row2;
struct pbStamp *pbStampPtr;
char *stampTable, *stampTitle, *stampDesc;
int i, n, index;

conn2= hAllocConn();
sprintf(query2,"select * from %s.pbStamp where stampName ='%s'", database, stampName);
    	
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
pbStampPtr = pbStampLoad(row2);

if (row2 == NULL)
    {
    errAbort("%s stamp data not found.", stampName);
    }
sqlFreeResult(&sr2);
    
sprintf(query2,"select * from %s.%s;", database, pbStampPtr->stampTable);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
    	
i=0;
while (row2 != NULL)
    {
    tx[i] = atof(row2[0]);
    ty[i] = atof(row2[1]);
    i++;
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);
hFreeConn(&conn2);

return(pbStampPtr);
}

void mapBoxStampTitle(int x, int y, int width, int height, char *title, char *tagName)
{
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);
hPrintf("HREF=\"../goldenPath/help/pb%s.shtml\"", tagName);
hPrintf(" target=_blank ALT=\"Click here for explanation of %c%s%c\">\n", '\'', title, '\'');
//hPrintf(" target=\"HistogramHelp\" ALT=\"Click here for explanation of %c%s%c\">\n", '\'', title, '\'');
}

void drawPbStamp(struct pbStamp *pbStampPtr, struct pbStampPict *stampPictPtr)
/* draw the stamp */
{
int ix, iy, iw, ih;
char *stampName, *stampTable, *stampTitle, *stampDesc;
double txmin, tymin, txmax, tymax;
int i, n, index;
int xx, yy;
char charStr[2];
int edgeColor;
Color stampColor;
int titleLen;

stampName   = cloneString(pbStampPtr->stampName);
stampTable  = cloneString(pbStampPtr->stampTable);
stampTitle  = cloneString(pbStampPtr->stampTitle);
n           = pbStampPtr->len;
txmin       = pbStampPtr->xmin;
txmax       = pbStampPtr->xmax;
tymin       = pbStampPtr->ymin;
tymax       = pbStampPtr->ymax;
stampDesc   = cloneString(pbStampPtr->stampDesc);

ix = stampPictPtr->xOrig;
iy = stampPictPtr->yOrig;
iw = stampPictPtr->width;
ih = stampPictPtr->height;
    
xScale = (double)(iw)/(txmax - txmin);
yScale = (double)(ih)/(tymax - tymin);
   
calStampXY(stampPictPtr, txmin+(txmax-txmin)/2.0, tymax, &xx, &yy);
vgTextCentered(g_vg, xx-5, yy-12, 10, 10, MG_BLACK, g_font, stampTitle);
    
titleLen = strlen(stampTitle);
mapBoxStampTitle(xx-5-titleLen*6/2-6, yy-14, titleLen*6+12, 14,  stampTitle, stampName);
    
edgeColor  = boundaryColor;
stampColor = vgFindColorIx(g_vg, 220, 220, 220);
for (index=0; index < (n-1); index++)
    {
    pbBox(tx[index], tymin, tx[index+1]-tx[index], ty[index], stampColor);
    hLine(tx[index], ty[index], tx[index+1]-tx[index], 1, edgeColor);
    }
vLine(tx[0], 0, ty[0], 1,  edgeColor);
for (index=0; index < (n-1); index++)
    {
    if (ty[index+1] > ty[index])
	{
	vLine(tx[index+1], ty[index], ty[index+1]-ty[index], 1,  edgeColor);
	}
    else
	{
	vLine(tx[index+1], ty[index+1], ty[index]-ty[index+1], 1, edgeColor);
	}
    }
    
hLine(txmin, tymin, txmax-txmin, 2, boundaryColor);
hLine(txmin, tymax, txmax-txmin, 2, boundaryColor);
vLine(txmin, tymin, tymax-tymin, 2, boundaryColor);
vLine(txmax, tymin, tymax-tymin, 2, boundaryColor);
    
if (strcmp(pbStampPtr->stampName, "pepRes") == 0)
    {
    for (i=0; i<20; i++)
	{
    	calStampXY(stampPictPtr, tx[i], tymin, &xx, &yy);
	sprintf(charStr, "%c", aaAlphabet[i]);
	vgTextCentered(g_vg, xx+1, yy+5, 10, 10, MG_BLACK, g_font, charStr);
	}
    } 
}

void drawPbStampB(struct pbStamp *pbStampPtr, struct pbStampPict *stampPictPtr)
/* draw the stamp, especially for the AA Anomaly */
{
int ix, iy, iw, ih;
char *stampTable, *stampTitle, *stampDesc;
double txmin, tymin, txmax, tymax;
int i, n, index;
int xx, yy;
char charStr[2];
int titleLen;

stampTable  = cloneString(pbStampPtr->stampTable);
stampTitle  = cloneString(pbStampPtr->stampTitle);
n           = pbStampPtr->len;
txmin       = pbStampPtr->xmin;
txmax       = pbStampPtr->xmax;
tymin       = pbStampPtr->ymin;
tymax       = pbStampPtr->ymax;
stampDesc   = cloneString(pbStampPtr->stampDesc);

ix = stampPictPtr->xOrig;
iy = stampPictPtr->yOrig;
iw = stampPictPtr->width;
ih = stampPictPtr->height;

xScale = (double)(iw)/(txmax - txmin);
yScale = (double)(ih)/(tymax - tymin);
 
calStampXY(stampPictPtr, txmin+(txmax-txmin)/2.0, tymax, &xx, &yy);
vgTextCentered(g_vg, xx-5, yy-12, 10, 10, MG_BLACK, g_font, "Amino Acid Anomalies");
    
titleLen = strlen("Amino Acid Anomoly");
mapBoxStampTitle(xx-5-titleLen*6/2-6, yy-14, titleLen*6+12, 14,  
	         "Amino Acid Anomolies", "pepAnomalies");
    
calStampXY(stampPictPtr, txmax-(txmax-txmin)*.25, tymin+(tymax-tymin)/4.0*3.0, &xx, &yy);
vgTextCentered(g_vg, xx, yy-10, 10, 10, MG_BLACK, g_font, "+2 stddev");
calStampXY(stampPictPtr, txmax-(txmax-txmin)*.25, tymin+(tymax-tymin)/4.0, &xx, &yy);
vgTextCentered(g_vg, xx, yy-10, 10, 10, MG_BLACK, g_font, "-2 stddev");
   
for (index=0; index < (n-1); index++)
    {
    hLine(tx[index], (tymax-tymin)/4.0,     (tx[index+1]-tx[index])/2.0, 1, MG_BLACK);
    hLine(tx[index], (tymax-tymin)/2.0,     tx[index+1]-tx[index], 1, MG_BLACK);
    hLine(tx[index], (tymax-tymin)/4.0*3.0, (tx[index+1]-tx[index])/2.0, 1, MG_BLACK);
    }
   
vLine(tx[0], 0, ty[0], 1,  MG_BLUE);
    
hLine(txmin, tymin, txmax-txmin, 2, boundaryColor);
hLine(txmin, tymax, txmax-txmin, 2, boundaryColor);
vLine(txmin, tymin, tymax-tymin, 2, boundaryColor);
vLine(txmax, tymin, tymax-tymin, 2, boundaryColor);
 
if (strcmp(pbStampPtr->stampName, "pepRes") == 0)
    {
    for (i=0; i<20; i++)
    	{
    	calStampXY(stampPictPtr, tx[i], tymin, &xx, &yy);
	sprintf(charStr, "%c", aaAlphabet[i]);
	vgTextCentered(g_vg, xx+1, yy+5, 10, 10, MG_BLACK, g_font, charStr);
	}
    } 
}

void drawResStamp(struct pbStamp *pbStampPtr, int ix, int iy, int iw, int ih)
/* draw the AA residue stamp */
{
char *stampTable, *stampTitle, *stampDesc;
double txmin, tymin, txmax, tymax;
int i, n, index;
int xx, yy;

stampTable	= cloneString(pbStampPtr->stampTable);
stampTitle	= cloneString(pbStampPtr->stampTitle);
n		= pbStampPtr->len;
txmin		= pbStampPtr->xmin;
txmax		= pbStampPtr->xmax;
tymin		= pbStampPtr->ymin;
tymax		= pbStampPtr->ymax;
stampDesc	= cloneString(pbStampPtr->stampDesc);

xScale = (double)(iw)/(txmax - txmin);
yScale = (double)(ih)/(tymax - tymin);
    
calStampXY(stampPictPtr, (txmax-txmin)/2.0, tymax, &xx, &yy);
vgTextCentered(g_vg, xx-5, yy+3, 10, 10, MG_BLACK, g_font, stampTitle);
   
for (index=0; index < (n-1); index++)
    {
    pbBox(tx[index], tymin, tx[index+1]-tx[index]-0.4, ty[index], MG_YELLOW);
    }
   
hLine(txmin, tymin, txmax-txmin, 1, MG_BLACK);
hLine(txmin, tymax, txmax-txmin, 1, MG_BLACK);
vLine(txmin, tymin, tymax-tymin, 1, MG_BLACK);
vLine(txmax, tymin, tymax-tymin, 1, MG_BLACK);
}

void doStamps(char *proteinID, char *mrnaID, char *aa, struct vGfx *vg, int *yOffp)
/* draw proteome browser stamps */
{
int i,j,l;

char *exonNumStr;
int exonNum;
char cond_str[200];
char *valStr;
char valStr2[50];
char *vertLabel;
int sf_cnt;
char *answer;
double pI=0.0;
double aaLen;
double exonCount;
char *chp;
int len;
int cCnt;

int xPosition;
int yPosition;
int stampWidth, stampHeight;

int aaResCnt[30];
double aaResFreqDouble[30];
int aaResFound;
int totalResCnt;

char *aap;
double molWeight=0.0;
double hydroSum;
struct pbStamp *stampDataPtr;

Color bkgColor;

for (j=0; j<23; j++)
    {
    aaResCnt[j] = 0;
    }

l=len = strlen(aa);
chp = aa;
for (i=0; i<l; i++)
    {
    aaResFound = 0;
    for (j=0; j<23; j++)
    	{
        if (*chp == aaAlphabet[j])
            {
            aaResFound = 1;
            aaResCnt[j] ++;
            }
        }
    chp++;
    }

totalResCnt = 0;
for (i=0; i<23; i++)
    {
    totalResCnt = totalResCnt + aaResCnt[i];
    }

for (i=0; i<20; i++)
    {
    aaResFreqDouble[i] = ((double)aaResCnt[i])/((double)totalResCnt);
    }

AllocVar(stampPictPtr);

stampWidth  = 75*(1+pbScale/3);
stampHeight = 60*(1+pbScale/3);
xPosition   = 15;
yPosition   = *yOffp + 135;
if (pbScale >= 6) yPosition = yPosition + 20;

boundaryColor = vgFindColorIx(g_vg, 170, 170, 170);

// draw pI stamp
sprintf(cond_str, "accession='%s'", proteinID);
answer = sqlGetField(NULL, database, "pepPiMw", "pI", cond_str);
if (answer != NULL)
    {
    pI      = (double)atof(answer);
    stampDataPtr = getStampData("pepPi");
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, stampWidth, stampHeight);
    drawPbStamp(stampDataPtr, stampPictPtr);
    drawXScale(stampDataPtr, stampPictPtr, 2);
    sprintf(valStr2, "%.1f", pI);
    markStamp(stampDataPtr, stampPictPtr, pI, valStr2, tx, ty);
    pbStampFree(&stampDataPtr);
    }
else
    {
    stampDataPtr = getStampData("pepPi");
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, stampWidth, stampHeight);
    drawPbStamp(stampDataPtr, stampPictPtr);
    drawXScale(stampDataPtr, stampPictPtr, 2);
    sprintf(valStr2, "N/A");
    markStamp0(stampDataPtr, stampPictPtr, pI, valStr2, tx, ty);
    pbStampFree(&stampDataPtr);
    }

// draw Mol Wt stamp
sprintf(cond_str, "accession='%s'", proteinID);
answer = sqlGetField(NULL, database, "pepMwAa", "MolWeight", cond_str);
if (answer != NULL)
    {
    sprintf(valStr2, "%s Da", answer);
    molWeight  = (double)atof(answer);
    stampDataPtr = getStampData("pepMolWt");
    xPosition = xPosition + stampWidth + stampWidth/8;
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, stampWidth, stampHeight);
    drawPbStamp(stampDataPtr, stampPictPtr);
    drawXScaleMW(stampDataPtr, stampPictPtr, 50000);
    markStamp(stampDataPtr, stampPictPtr, molWeight, valStr2, tx, ty);
    pbStampFree(&stampDataPtr);
    }
else
    {
    sprintf(valStr2, "N/A");
    stampDataPtr = getStampData("pepMolWt");
    xPosition = xPosition + stampWidth + stampWidth/8;
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, stampWidth, stampHeight);
    drawPbStamp(stampDataPtr, stampPictPtr);
    drawXScaleMW(stampDataPtr, stampPictPtr, 50000);
    markStamp0(stampDataPtr, stampPictPtr, molWeight, valStr2, tx, ty);
    pbStampFree(&stampDataPtr);
    }

// draw exon count stamp
sprintf(cond_str, "qName='%s'", proteinID);
answer = sqlGetField(NULL, database, kgProtMapTableName, "blockCount", cond_str);
if (answer != NULL)
    {
    valStr       = cloneString(answer);
    exonCount    = (double)atoi(answer);
    stampDataPtr = getStampData("exonCnt");
    xPosition = xPosition + stampWidth + stampWidth/8;
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, stampWidth, stampHeight);
    drawPbStamp(stampDataPtr, stampPictPtr);
    drawXScale(stampDataPtr, stampPictPtr, 5);
    markStamp(stampDataPtr, stampPictPtr, exonCount, valStr, tx, ty);
    pbStampFree(&stampDataPtr);
    }

// draw AA residual anomolies stamp
if (answer != NULL)
    {
    stampDataPtr = getStampData("pepRes");
    xPosition = xPosition + stampWidth + stampWidth/8;
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, 3*stampWidth/2, stampHeight);
    drawPbStamp(stampDataPtr, stampPictPtr);
    for (i=0; i<20; i++)
	{
        markResStamp(stampDataPtr, stampPictPtr, i, aaResFreqDouble[i], tx, ty, avg, stddev);
	}
    pbStampFree(&stampDataPtr);
    }

xPosition = 15;
yPosition = yPosition + 170;

// draw family size stamp
sprintf(cond_str, "accession='%s'", proteinID);
answer = sqlGetField(NULL, protDbName, "swInterPro", "count(*)", cond_str);
if (answer != NULL)
    {
    valStr       = cloneString(answer);
    stampDataPtr = getStampData("intPCnt");
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, stampWidth, stampHeight);
    drawPbStamp(stampDataPtr, stampPictPtr);
    drawXScale(stampDataPtr, stampPictPtr, 1);
    markStamp(stampDataPtr, stampPictPtr, (double)(atoi(answer)), valStr, tx, ty);
    pbStampFree(&stampDataPtr);
    }
else
    {
    valStr       = cloneString("N/A");
    stampDataPtr = getStampData("intPCnt");
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, stampWidth, stampHeight);
    drawPbStamp(stampDataPtr, stampPictPtr);
    drawXScale(stampDataPtr, stampPictPtr, 1);
    markStamp0(stampDataPtr, stampPictPtr, (double)(atoi(answer)), valStr, tx, ty);
    pbStampFree(&stampDataPtr);
    }

// draw hydrophobicity stamp
chp      = protSeq;
hydroSum = 0;
for (i=0; i<protSeqLen; i++)
    {
    hydroSum = hydroSum + aa_hydro[(int)(*chp)];
    chp++;
    }
stampDataPtr = getStampData("hydro");
xPosition = xPosition + stampWidth + stampWidth/8;
setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, stampWidth, stampHeight);
drawPbStamp(stampDataPtr, stampPictPtr);
drawXScaleHydro(stampDataPtr, stampPictPtr, 1.0);
sprintf(valStr2, "%.1f", hydroSum/(double)len);
markStamp(stampDataPtr, stampPictPtr, hydroSum/(double)len, valStr2, tx, ty);
pbStampFree(&stampDataPtr);

// draw Cystein Count stamp
chp  = protSeq;
cCnt = 0;
for (i=0; i<len; i++)
    {
    if (*chp == 'C') cCnt ++;
    chp++;
    }
stampDataPtr = getStampData("cCnt");
xPosition = xPosition + stampWidth + stampWidth/8;
setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, stampWidth, stampHeight);
drawPbStamp(stampDataPtr, stampPictPtr);
drawXScale(stampDataPtr, stampPictPtr, 10);
sprintf(valStr2, "%d", cCnt);
markStamp(stampDataPtr, stampPictPtr, (double)cCnt, valStr2, tx, ty);
pbStampFree(&stampDataPtr);

// draw AA residual anomolies stddev stamp
if (answer != NULL)
    {
    exonCount    = (double)atof(answer);
    stampDataPtr = getStampData("pepRes");
    xPosition = xPosition + stampWidth + stampWidth/8;
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, 3*stampWidth/2, stampHeight);
    
    stampDataPtr->ymin = -4.0;
    stampDataPtr->ymax =  4.0;
    for (i=0; i<20; i++)
	{
        markResStdvStamp(stampDataPtr, stampPictPtr, i, aaResFreqDouble[i], tx, ty, avg, stddev);
	}

    // draw background after bars drawn so that "... stddev" labels do not get covered by bars
    stampDataPtr = getStampData("pepRes");
    setPbStampPict(stampPictPtr, stampDataPtr, xPosition, yPosition, 3*stampWidth/2, stampHeight);
    drawPbStampB(stampDataPtr, stampPictPtr);

    pbStampFree(&stampDataPtr);
    }

vertLabel = cloneString("Frequency");
for (i=strlen(vertLabel)-1; i>=0; i--)
    {
    vertLabel[i+1] = '\0';
    vgTextCentered(g_vg, 3, 45+i*10, 10, 10, MG_BLACK, g_font, vertLabel+i);
    vgTextCentered(g_vg, 3, 215+i*10, 10, 10, MG_BLACK, g_font, vertLabel+i);
    }
}

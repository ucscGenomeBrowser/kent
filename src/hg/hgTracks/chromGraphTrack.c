/* chromGraphTrack - displays track that is just a line
 * connecting certain data points.  This does not handle the
 * data volume that the wiggle track does.  It's meant for
 * things like graphs for gene association studies. */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"
#include "hgTracks.h"
#include "vGfx.h"
#include "chromGraph.h"

static void cgDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct vGfx *vg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chromosome graph. */
{
struct chromGraphSettings *cgs = tg->customPt;
struct sqlConnection *conn = hAllocConn();
char query[512];
struct sqlResult *sr;
char **row;
int x,y,lastX=0,lastY=0, llastX = 0, llastY = 0;
int height = tg->height;
int maxGapToFill = cgs->maxGapToFill;
int lastPos = -maxGapToFill-1;
double minVal = cgs->minVal;
double yScale = (height-1)/(cgs->maxVal - minVal);
double xScale = scaleForPixels(width);

/* Draw background lines in full mode. */
if (vis == tvFull && cgs->linesAtCount != 0)
    {
    int i;
    Color lightBlue = vgFindRgb(vg, &guidelineColor);
    for (i=0; i<cgs->linesAtCount; ++i)
        {
	y = height - 1 - (cgs->linesAt[i] - minVal)*yScale + yOff;
	vgBox(vg, xOff, y, width, 1, lightBlue);
	}
    }

/* Construct query.  Set up a little more than window so that
 * we can draw connecting lines. */
safef(query, sizeof(query), 
    "select chromStart,val from %s "
    "where chrom='%s' and chromStart>=%d and chromStart<%d",
    tg->mapName, chromName, 
    seqStart - cgs->maxGapToFill, seqEnd + cgs->maxGapToFill);
sr = sqlGetResult(conn, query);

/* Loop through drawing lines from one point to another unless
 * the points are too far apart. */
while ((row = sqlNextRow(sr)) != NULL)
    {
    int pos = sqlUnsigned(row[0]);
    double val = atof(row[1]);
    x = (pos - seqStart)*xScale + xOff;
    y = height - (val - minVal)*yScale + yOff;
    if (x >= xOff)
        {
	if (pos - lastPos <= maxGapToFill)
	    {
	    if (llastX != lastX || llastY != lastY || lastX != x || lastY != y)
		vgLine(vg, lastX, lastY, x, y, color);
	    }
	else
	    vgDot(vg, x, y, color);
	}
    llastX = lastX;
    llastY = lastY;
    lastX = x;
    lastY = y;
    lastPos = pos;
    if (pos >= seqEnd)
        break;
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
}

int cgTotalHeight(struct track *tg, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height 
 * they use. */
{
struct chromGraphSettings *cgs = tg->customPt;
switch (vis)
    {
    case tvFull:
    case tvPack:
        tg->height = cgs->pixels;
	break;
    case tvDense:
    default:
        tg->height = tl.fontHeight;
	break;
    }
return tg->height;
}

void wrapTextAndCenter(struct vGfx *vg, int xOff, int yOff, int width,
	int height,  MgFont *font, Color color, char *text)
/* Try and word-wrap text into box more or less. */
{
struct slName *word, *wordList = slNameListFromString(text, ' ');
int wordCount = slCount(wordList);
int fontHeight = tl.fontHeight;
int lineCount = height / fontHeight;
int yUsed, y;
if (lineCount > wordCount) lineCount = wordCount;
yUsed = lineCount * fontHeight;
y = (height - yUsed)/2 + yOff;
uglyf("height %d, yUsed %d, y %d, lineCount %d<BR>\n", height, yUsed, y, lineCount);
for (word = wordList; word != NULL; word = word->next)
    {
    vgText(vg, xOff, y, color, font, word->name);
    y += fontHeight;
    }
}

void cgLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct vGfx *vg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
/* Draw labels to the left. */
{
struct chromGraphSettings *cgs = tg->customPt;
if (vis == tvDense || cgs->linesAtCount == 0)
    {
    vgTextRight(vg, xOff, yOff, width - 1, height,
	color, font, tg->shortLabel);
    }
else
    {
    int i;
    int realHeight = height;
    int realYoff = yOff;
    double minVal = cgs->minVal;
    double yScale;
    int tickSize = tl.nWidth;
    int numPos = tickSize * 1.25;

    if (withCenterLabels)
        {
	int centerLabelHeight = tl.fontHeight;
	realHeight -= centerLabelHeight;
	realYoff += centerLabelHeight;
	}
    yScale = (realHeight-1)/(cgs->maxVal - minVal);
    for (i=0; i<cgs->linesAtCount; ++i)
        {
	double val = cgs->linesAt[i];
	int y = realHeight - 1 - (val - minVal)*yScale + realYoff;
	char numText[16];
	vgBox(vg, xOff + width - tickSize, y, tickSize, 1, color);
	safef(numText, sizeof(numText), "%g", val);
	vgTextRight(vg, xOff, y - tl.fontHeight+2, 
		width - numPos, tl.fontHeight,
		color, font, numText);
	}
    wrapTextAndCenter(vg, xOff+1, yOff, width*5*tl.nWidth, height,
    	font, color, tg->shortLabel);
    }
}

void chromGraphMethods(struct track *tg)
/* Return track with methods filled in */
{
struct sqlConnection *conn = hAllocConn();
tg->loadItems = tgLoadNothing;
tg->drawItems = cgDrawItems;
tg->totalHeight = cgTotalHeight;
tg->freeItems = tgFreeNothing;
tg->drawLeftLabels = cgLeftLabels;
tg->customPt = chromGraphSettingsGet(tg->mapName, conn,
	tg->tdb, cart);
tg->mapsSelf = TRUE;
hFreeConn(&conn);
}


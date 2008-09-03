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
#include "hvGfx.h"
#include "chromGraph.h"

Color colorFromAscii(struct hvGfx *hvg, char *asciiColor)
/* Get color index for a named color. */
/* Copy/pasted from hgGenome/mainPage.c ... should be in a library */
/* somewhere! */
{
if (sameWord("red", asciiColor))
    return MG_RED;
else if (sameWord("blue", asciiColor))
    return MG_BLUE;
else if (sameWord("yellow", asciiColor))
    return hvGfxFindColorIx(hvg, 220, 220, 0);
else if (sameWord("purple", asciiColor))
    return hvGfxFindColorIx(hvg, 150, 0, 200);
else if (sameWord("orange", asciiColor))
    return hvGfxFindColorIx(hvg, 230, 120, 0);
else if (sameWord("green", asciiColor))
    return hvGfxFindColorIx(hvg, 0, 180, 0);
else if (sameWord("gray", asciiColor))
    return MG_GRAY;
else
    return MG_BLACK;
}

static Color cgColorLikeHgGenome(struct track *tg, struct hvGfx *hvg)
/* Search the cart variables and use the colors corresponding to the hgGenome */
/* graph. */
{
Color ret;
struct hashEl *matchingCartSettings = NULL;
struct hashEl *el;
char cartGenomeWildStr[256];
char *graphCartVarName = NULL;
char *colorCartVarSetting = "black";
safef(cartGenomeWildStr, sizeof(cartGenomeWildStr), "hgGenome_graph_%s_*", database);
/* for ex. hgGenome_graph_hg18_1_1 */
matchingCartSettings = cartFindLike(cart, cartGenomeWildStr);
for (el = matchingCartSettings; el != NULL; el = el->next)
    {
    char *val = (char *)el->val;
    if (val && sameString(val, tg->mapName))
	{
	graphCartVarName = el->name;
	break;
	}
    }
if (graphCartVarName)
    {
    char *colorCartVarName = replaceChars(graphCartVarName, "_graph_", "_graphColor_");
    colorCartVarSetting = cartUsualString(cart, colorCartVarName, "black");
    freeMem(colorCartVarName);
    }
hashElFreeList(&matchingCartSettings);
ret = colorFromAscii(hvg, colorCartVarSetting);
return ret;
}

static void cgDrawEither(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis,
	char *binFileName)
/* Draw chromosome graph - either built in or not. */
{
struct chromGraphSettings *cgs = tg->customPt;
int x,y,lastX=0,lastY=0, llastX = 0, llastY = 0;
int height = tg->height;
int maxGapToFill = cgs->maxGapToFill;
int lastPos = -maxGapToFill-1;
double minVal = cgs->minVal;
double yScale = (height-1)/(cgs->maxVal - minVal);
double xScale = scaleForPixels(width);
Color myColor = cgColorLikeHgGenome(tg, hvg);

/* Draw background lines in full mode. */
if (vis == tvFull && cgs->linesAtCount != 0)
    {
    int i;
    Color lightBlue = hvGfxFindRgb(hvg, &guidelineColor);
    for (i=0; i<cgs->linesAtCount; ++i)
        {
	y = height - 1 - (cgs->linesAt[i] - minVal)*yScale + yOff;
	hvGfxBox(hvg, xOff, y, width, 1, lightBlue);
	}
    }

if (binFileName)
    {
    struct chromGraphBin *cgb = chromGraphBinOpen(binFileName);
    if (chromGraphBinSeekToChrom(cgb, chromName))
	{
	int seqStartMinus = seqStart - cgs->maxGapToFill;
	while (chromGraphBinNextVal(cgb))
	    {
	    int pos = cgb->chromStart;
	    if (pos >= seqStartMinus)
		{
		double val = cgb->val;
		x = (pos - seqStart)*xScale + xOff;
		y = height - 1 - (val - minVal)*yScale + yOff;
		if (x >= xOff)
		    {
		    if (pos - lastPos <= maxGapToFill)
			{
			if (llastX != lastX || llastY != lastY || lastX != x || lastY != y)
			    hvGfxLine(hvg, lastX, lastY, x, y, myColor);
			}
		    else
			hvGfxDot(hvg, x, y, myColor);
		    }
		llastX = lastX;
		llastY = lastY;
		lastX = x;
		lastY = y;
		lastPos = pos;
		if (pos >= seqEnd)
		    break;
		}
	    }
	}
    }
else
    {
    struct sqlConnection *conn = hAllocConn(database);
    char query[512];
    struct sqlResult *sr;
    char **row;
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
	y = height - 1 - (val - minVal)*yScale + yOff;
	if (x >= xOff)
	    {
	    if (pos - lastPos <= maxGapToFill)
		{
		if (llastX != lastX || llastY != lastY || lastX != x || lastY != y)
		    hvGfxLine(hvg, lastX, lastY, x, y, myColor);
		}
	    else
		hvGfxDot(hvg, x, y, myColor);
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

/* Do map box */
xOff = hvGfxAdjXW(hvg, xOff, width);

char *encodedTrack = cgiEncode(tg->mapName);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", xOff, yOff, xOff+width,
	yOff+height);
hPrintf("HREF=\"%s&o=%d&t=%d&g=%s&c=%s&l=%d&r=%d&db=%s&pix=%d\">\n", 
	    hgcNameAndSettings(), winStart, winEnd, encodedTrack, chromName, winStart, winEnd, 
	    database, tl.picWidth);
}

static void cgDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chromosome graph for built-in track. */
{
cgDrawEither(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis, NULL);
}

static void cgDrawItemsCt(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw chromosome graph for customTrack. */
{
cgDrawEither(tg, seqStart, seqEnd, hvg, xOff, yOff, width,
	font, color, vis, trackDbRequiredSetting(tg->tdb, "binaryFile"));
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

void wrapTextAndCenter(struct hvGfx *hvg, int xOff, int yOff, int width,
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
for (word = wordList; word != NULL; word = word->next)
    {
    hvGfxText(hvg, xOff, y, color, font, word->name);
    y += fontHeight;
    }
}

void cgLeftLabels(struct track *tg, int seqStart, int seqEnd,
	struct hvGfx *hvg, int xOff, int yOff, int width, int height,
	boolean withCenterLabels, MgFont *font, Color color,
	enum trackVisibility vis)
/* Draw labels to the left. */
{
struct chromGraphSettings *cgs = tg->customPt;
if (vis == tvDense || cgs->linesAtCount == 0)
    {
    hvGfxTextRight(hvg, xOff, yOff, width - 1, height,
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
	hvGfxBox(hvg, xOff + width - tickSize, y, tickSize, 1, color);
	safef(numText, sizeof(numText), "%g", val);
	hvGfxTextRight(hvg, xOff, y - tl.fontHeight+2, 
		width - numPos, tl.fontHeight,
		color, font, numText);
	}
    wrapTextAndCenter(hvg, xOff+1, yOff, width*5*tl.nWidth, height,
    	font, color, tg->shortLabel);
    }
}

static void chromGraphMethodsCommon(struct track *tg)
/* Get chromGraph methods common to built in and custom tracks. */
{
tg->loadItems = tgLoadNothing;
tg->totalHeight = cgTotalHeight;
tg->freeItems = tgFreeNothing;
tg->drawLeftLabels = cgLeftLabels;
tg->mapsSelf = TRUE;
}

void chromGraphMethods(struct track *tg)
/* Fill in chromGraph methods for built in track. */
{
chromGraphMethodsCommon(tg);
tg->drawItems = cgDrawItems;
struct sqlConnection *conn = hAllocConn(database);
tg->customPt = chromGraphSettingsGet(tg->mapName, conn,
	tg->tdb, cart);
hFreeConn(&conn);
}

void chromGraphMethodsCt(struct track *tg)
/* Fill in chromGraph methods for custom track. */
{
tg->drawItems = cgDrawItemsCt;
chromGraphMethodsCommon(tg);
tg->customPt = chromGraphSettingsGet(tg->mapName, NULL, tg->tdb, cart);
}


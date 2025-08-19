
#include "common.h"
#include "hgTracks.h"
#include "chromAlias.h"
#include "hgConfig.h"
#include "bigChain.h"
#include "bigLink.h"
#include "trackHub.h"

struct highRegions
// store highlight information
{
struct highRegions *next;
long chromStart;
long chromEnd;
long oChromStart;
long oChromEnd;
char strand;
unsigned hexColor;
char *otherBases;
unsigned otherCount;
};

#define INSERT_COLOR     0
#define DEL_COLOR      1
#define DOUBLE_COLOR     2
#define MISMATCH_COLOR     3

static Color *highlightColors;
static unsigned lengthLimit;

static Color getColor(char *confVariable, char *defaultRgba)
{
Color color = 0xff0000ff;

char *str = cloneString(cfgOptionDefault(confVariable, defaultRgba));
char *words[10];
if (chopCommas(str, words) != 4)
    errAbort("conf variable '%s' is expected to have a string like  r,g,b,a ", confVariable);

color = MAKECOLOR_32_A(atoi(words[0]),atoi(words[1]),atoi(words[2]),atoi(words[3]));
return color;
}

static void initializeHighlightColors()
{
if (highlightColors == NULL)
    {
    highlightColors = needMem(sizeof(Color) * 3);
 
    lengthLimit = atoi(cfgOptionDefault("quickLift.lengthLimit", "10000"));

    highlightColors[INSERT_COLOR] = getColor("quickLift.insColor","255,0,0,255");
    highlightColors[DEL_COLOR] = getColor("quickLift.delColor","0,255,0,255");
    highlightColors[DOUBLE_COLOR] = getColor("quickLift.doubleColor","0,0,255,255");
    highlightColors[MISMATCH_COLOR] = getColor("quickLift.mismatchColor","255,0,0,255");
    }
}

struct highRegions *getQuickLiftLines(char *liftDb, char *quickLiftFile, int seqStart, int seqEnd)
/* Figure out the highlight regions and cache them. */
{
static struct hash *highLightsHash = NULL;
struct highRegions *hrList = NULL;

initializeHighlightColors();
if (seqEnd - seqStart > lengthLimit)
    return hrList;

if (highLightsHash != NULL)
    {
    if ((hrList = (struct highRegions *)hashFindVal(highLightsHash, quickLiftFile)) != NULL)
        return hrList;
    }
else
    {
    highLightsHash = newHash(0);
    }


struct bbiFile *bbiChain = bigBedFileOpenAlias(quickLiftFile, chromAliasFindAliases);
struct lm *lm = lmInit(0);
struct bigBedInterval *bbChain, *bbChainList =  bigBedIntervalQuery(bbiChain, chromName, seqStart, seqEnd, 0, lm);
char *links = bigChainGetLinkFile(quickLiftFile);
struct bbiFile *bbiLink = bigBedFileOpenAlias(links, chromAliasFindAliases);
struct bigBedInterval  *bbLink, *bbLinkList =  bigBedIntervalQuery(bbiLink, chromName, seqStart, seqEnd, 0, lm);

char *chainRow[1024];
char *linkRow[1024];
char startBuf[16], endBuf[16];

for (bbChain = bbChainList; bbChain != NULL; bbChain = bbChain->next)
    {
    bigBedIntervalToRow(bbChain, chromName, startBuf, endBuf, chainRow, ArraySize(chainRow));
    struct bigChain *bc = bigChainLoad(chainRow);

    int previousTEnd = -1;
    int previousQEnd = -1;
    for (bbLink = bbLinkList; bbLink != NULL; bbLink = bbLink->next)
        {
        bigBedIntervalToRow(bbLink, chromName, startBuf, endBuf, linkRow, ArraySize(linkRow));
        struct bigLink *bl = bigLinkLoad(linkRow);

        if (!sameString(bl->name, bc->name))
            continue;

        int tStart = bl->chromStart;
        int tEnd = bl->chromEnd;
        int qStart = bl->qStart;
        int qEnd = qStart + (tEnd - tStart);
        // crop the chain block if it's bigger than the window
        int tMin, tMax;
        int qMin, qMax;
        tMin = bl->chromStart;
        tMax = bl->chromEnd;
        qMin = bl->qStart;
        if (seqStart > bl->chromStart) 
            {
            tMin = seqStart;
            qMin = qStart + (seqStart - bl->chromStart);
            }
        if (seqEnd < bl->chromEnd) 
            {
            tMax = seqEnd;
            }
        qMax = qMin + (tMax - tMin);

        if (bc->strand[0] == '-')
            {
            qMin = bc->qSize - qMax;
            qMax = qMin + (tMax - tMin);
            }

        struct dnaSeq *tSeq = hDnaFromSeq(database, chromName, tMin, tMax, dnaUpper);
        struct dnaSeq *qSeq = hDnaFromSeq(liftDb, bc->qName, qMin, qMax, dnaUpper);
        if (bc->strand[0] == '-')
            reverseComplement(qSeq->dna, qSeq->size);
        struct highRegions *hr;
        if ((previousTEnd != -1) && (previousTEnd == tStart))
            {
            AllocVar(hr);
            slAddHead(&hrList, hr);
        //    hr->strand = 
            hr->chromStart = previousTEnd;
            hr->chromEnd = tStart;
            hr->oChromStart = previousQEnd;
            hr->oChromEnd = qStart;
            hr->hexColor = highlightColors[DEL_COLOR];
            hr->otherBases = &qSeq->dna[qStart - qMin];
            hr->otherCount = hr->oChromEnd - hr->oChromStart;
            }
        if ( (previousQEnd != -1) && (previousQEnd == qStart))
            {
            AllocVar(hr);
            slAddHead(&hrList, hr);
            hr->chromStart = previousTEnd;
            hr->chromEnd = tStart;
            hr->oChromStart = previousQEnd;
            hr->oChromEnd = qStart;
            hr->hexColor = highlightColors[INSERT_COLOR];
            }
        if ( ((previousQEnd != -1) && (previousQEnd != qStart)) 
             && ((previousTEnd != -1) && (previousTEnd != tStart)))
            {
            AllocVar(hr);
            slAddHead(&hrList, hr);
            hr->chromStart = previousTEnd;
            hr->chromEnd = tStart;
            hr->oChromStart = previousQEnd;
            hr->oChromEnd = qStart;
            hr->hexColor = highlightColors[DOUBLE_COLOR];
            }
        previousQEnd = qEnd;
        previousTEnd = tEnd;


        unsigned tAddr = tMin;
        unsigned qAddr = qMin;
        int count = 0;
        for(; tAddr < tEnd; tAddr++, qAddr++, count++)
            {
            if (tSeq->dna[count] != qSeq->dna[count])
                {
                AllocVar(hr);
                slAddHead(&hrList, hr);
                hr->chromStart = tAddr;
                hr->chromEnd = tAddr + 1;
                hr->oChromStart = qAddr;
                hr->oChromEnd = qAddr + 1;
                hr->otherBases = &qSeq->dna[count];
                hr->otherCount = 1;
                hr->hexColor = highlightColors[MISMATCH_COLOR];
                }
            }
        }
    }

hashAdd(highLightsHash, quickLiftFile, hrList);

return hrList;
}

static void drawTri(struct hvGfx *hvg, int x1, int x2, int y, Color color)
/* Draw traingle. */
{
struct gfxPoly *poly = gfxPolyNew();
int half = (x2 - x1) / 2;

gfxPolyAddPoint(poly, x1, y);
gfxPolyAddPoint(poly, x1+half, y+2*half);
gfxPolyAddPoint(poly, x2, y);
hvGfxDrawPoly(hvg, poly, color, TRUE);
gfxPolyFree(&poly);
}

void maybeDrawQuickLiftLines( struct track *tg, int seqStart, int seqEnd,
                      struct hvGfx *hvg, int xOff, int yOff, int width,
                      MgFont *font, Color color, enum trackVisibility vis)
/* Draw the indel regions in quickLifted tracks as a highlight. */
{
char *quickLiftFile = cloneString(trackDbSetting(tg->tdb, "quickLiftUrl"));
if (quickLiftFile == NULL)
    return;

boolean drawTriangle = FALSE;
if (startsWith("quickLiftChain", trackHubSkipHubName(tg->track)))
    drawTriangle = TRUE;
char *liftDb = trackDbSetting(tg->tdb, "quickLiftDb");
struct highRegions *regions = getQuickLiftLines(liftDb, quickLiftFile, seqStart, seqEnd);
struct highRegions *hr = regions;

int fontHeight = mgFontLineHeight(tl.font);
int height = tg->height;
if (!drawTriangle && isCenterLabelIncluded(tg))
    {
    height += fontHeight;
    yOff -= fontHeight;
    }

for(; hr; hr = hr->next)
    {
    unsigned int hexColor = hr->hexColor;
    double scale = scaleForWindow(width, seqStart, seqEnd);
    int x1 = xOff + scale * (hr->chromStart - seqStart);
    int w =  scale * (hr->chromEnd - hr->chromStart);
    if (w == 0) 
        w = 1;
    hvGfxSetClip(hvg, xOff, yOff, width, height);  // we're drawing in the center label at the moment
    int startX, endX;
    if (drawTriangle)
        {
        startX = x1 + w/2 - fontHeight/2;
        endX = x1 + w/2 + fontHeight/2 ;
        drawTri(hvg, startX, endX , yOff, hexColor);
        }
    else
        {
        startX = x1;
        endX = x1 + w;
        hvGfxBox(hvg, startX, yOff, w, height, hexColor);
        }

    char mouseOver[4096];

    if (hr->hexColor == highlightColors[MISMATCH_COLOR])
        safef(mouseOver, sizeof mouseOver, "mismatch %.*s", hr->otherCount, hr->otherBases);
    else if (hr->chromStart == hr->chromEnd)
        safef(mouseOver, sizeof mouseOver, "deletion %ldbp (%.*s)", hr->oChromEnd - hr->oChromStart, hr->otherCount, hr->otherBases);
    else if (hr->oChromStart == hr->oChromEnd)
        safef(mouseOver, sizeof mouseOver, "insertion %ldbp", hr->chromEnd - hr->chromStart);
    else
        safef(mouseOver, sizeof mouseOver, "double %ldbp", hr->oChromEnd - hr->oChromStart);
    mapBoxHc(hvg, seqStart, seqEnd, startX, yOff, endX - startX, height, tg->track, "indel", mouseOver);

    }
}

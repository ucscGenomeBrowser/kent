
#include "common.h"
#include "hgTracks.h"
#include "chromAlias.h"
#include "hgConfig.h"
#include "bigChain.h"

struct highRegions
// store highlight information
{
struct highRegions *next;
long chromStart;
long chromEnd;
unsigned hexColor;
};

#define INSERT_COLOR     0
#define DEL_COLOR      1
#define DOUBLE_COLOR     2

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
    }
}

struct highRegions *getQuickLiftLines(char *quickLiftFile, int seqStart, int seqEnd)
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


char *links = bigChainGetLinkFile(quickLiftFile);
struct bbiFile *bbi = bigBedFileOpenAlias(links, chromAliasFindAliases);
struct lm *lm = lmInit(0);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chromName, seqStart, seqEnd, 0, lm);

char *bedRow[5];
char startBuf[16], endBuf[16];

int previousTEnd = -1;
int previousQEnd = -1;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    int tStart = atoi(bedRow[1]);
    int tEnd = atoi(bedRow[2]);
    int qStart = atoi(bedRow[4]);
    int qEnd = qStart + (tEnd - tStart);
    struct highRegions *hr;
    if ((previousTEnd != -1) && (previousTEnd == tStart))
        {
        AllocVar(hr);
        slAddHead(&hrList, hr);
        hr->chromStart = previousTEnd;
        hr->chromEnd = tStart;
        hr->hexColor = highlightColors[DEL_COLOR];
        }
    if ( (previousQEnd != -1) && (previousQEnd == qStart))
        {
        AllocVar(hr);
        slAddHead(&hrList, hr);
        hr->chromStart = previousTEnd;
        hr->chromEnd = tStart;
         hr->hexColor = highlightColors[INSERT_COLOR];
        }
    if ( ((previousQEnd != -1) && (previousQEnd != qStart)) 
         && ((previousTEnd != -1) && (previousTEnd != tStart)))
        {
        AllocVar(hr);
        slAddHead(&hrList, hr);
        hr->chromStart = previousTEnd;
        hr->chromEnd = tStart;
        hr->hexColor = highlightColors[DOUBLE_COLOR];
        }
    previousQEnd = qEnd;
    previousTEnd = tEnd;
    }

hashAdd(highLightsHash, quickLiftFile, hrList);

return hrList;
}

void maybeDrawQuickLiftLines( struct track *tg, int seqStart, int seqEnd,
                      struct hvGfx *hvg, int xOff, int yOff, int width,
                      MgFont *font, Color color, enum trackVisibility vis)
/* Draw the indel regions in quickLifted tracks as a highlight. */
{
char *quickLiftFile = cloneString(trackDbSetting(tg->tdb, "quickLiftUrl"));
if (quickLiftFile == NULL)
    return;

struct highRegions *regions = getQuickLiftLines(quickLiftFile, seqStart, seqEnd);
struct highRegions *hr = regions;

int fontHeight = mgFontLineHeight(tl.font);
int height = tg->height;
if (isCenterLabelIncluded(tg))
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
    hvGfxBox(hvg, x1, yOff, w, height, hexColor);
    }
}

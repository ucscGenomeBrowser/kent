#include "common.h"
#include "psl.h"
#include "cutter.h"
#include "hgTracks.h"
#include "hdb.h"
#include "cutterTrack.h"

void cuttersDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw the restriction enzyme at position. */
{
struct bed *bed = item;
int heightPer, x1, x2, w;
struct trackDb *tdb = tg->tdb;
int scoreMin, scoreMax;
if (!zoomedToBaseLevel)
    bedDrawSimpleAt(tg, item, hvg, xOff, y, scale, font, color, vis);
else 
    {
    heightPer = tg->heightPer;
    x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
    scoreMin = atoi(trackDbSettingOrDefault(tdb, "scoreMin", "0"));
    scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));    
    color = hvGfxFindColorIx(hvg, 0,0,0);
    w = x2-x1;
    if (w < 1)
	w = 1;
    y += (heightPer >> 1) - 1;
    if (color)
	{
	struct sqlConnection *conn = hAllocConn("hgFixed");
	struct cutter *cut;
	char query[80];
	char strand = bed->strand[0];
	char *s = bed->name;
	int letterWidth, cuts[4];
	int tickHeight = (heightPer>>1) - 1, tickWidth = 2;
	int i, xH;
	Color baseHighlight = getBlueColor();

	baseHighlight = lighterColor(hvg, baseHighlight);
	safef(query, sizeof(query), "select * from cutters where name=\'%s\'", s);
	cut = cutterLoadByQuery(conn, query);
	letterWidth = round(((double)w)/cut->size);
	cuts[0] = x1 + cut->cut * letterWidth;
	cuts[1] = cuts[0] + cut->overhang * letterWidth;
	cuts[2] = x2 - (cut->cut * letterWidth);
	cuts[3] = cuts[2] - cut->overhang * letterWidth;
	for (i = 0; i < 4; i++)
	    {
	    if (cuts[i] < x1)
		cuts[i] = x1;
	    else if (cuts[i] >= x2)
		cuts[i] = x2 - tickWidth;
	    }
	if (strand == '+')
	    {
	    hvGfxBox(hvg, cuts[0], y - tickHeight, tickWidth, tickHeight, color);
	    hvGfxBox(hvg, cuts[1], y + 2, tickWidth, tickHeight, color);	    
	    }
	else if (strand == '-')
	    {
	    hvGfxBox(hvg, cuts[2], y - tickHeight, tickWidth, tickHeight, color);
	    hvGfxBox(hvg, cuts[3], y + 2, tickWidth, tickHeight, color);	    	    
	    }
	hvGfxBox(hvg, x1, y, w, 2, color);
	/* Draw highlight of non-N bases. */
	xH = x1;
	for (i = 0; i < strlen(cut->seq); i++)
	    {
	    char c = (strand == '+') ? cut->seq[i] : cut->seq[strlen(cut->seq)-i-1];
	    if ((c != 'A') && (c != 'C') && (c != 'G') && (c != 'T'))
		hvGfxBox(hvg, xH, y, letterWidth, 2, baseHighlight);
	    xH += letterWidth;
	    }
	if (tg->drawName && vis != tvSquish)
	    {
	    /* Clip here so that text will tend to be more visible... */
	    w = x2-x1;
	    if (w > mgFontStringWidth(font, s))
		{
		Color textColor = hvGfxContrastingColor(hvg, color);
		hvGfxTextCentered(hvg, x1, y, w, heightPer, textColor, font, s);
		}
	    mapBoxHc(hvg, bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
		     tg->mapName, tg->mapItemName(tg, bed), NULL);
	    }
	hFreeConn(&conn);
	cutterFree(&cut);
	}
    }
}

void cuttersLoad(struct track *tg)
{
struct sqlConnection *conn;
struct cutter *cutters;
struct dnaSeq *windowDna = NULL;
struct bed *bedList = NULL;
int winSize = winEnd - winStart;

conn = hAllocConn("hgFixed");
cutters = cutterLoadByQuery(conn, "select * from cutters");
windowDna = hDnaFromSeq(database, chromName, winStart, winEnd, dnaUpper);

/* Do different things based on window size. */

if (winSize < 250000)
    {
    char *enz = cartUsualString(cart, cutterVar, cutterDefault);
    struct slName *cartCutters = NULL;

    if (enz && (strlen(enz) > 0))
	{
	eraseWhiteSpace(enz);
	cartCutters = slNameListFromComma(enz);
	}

    if (cartCutters)
	cullCutters(&cutters, TRUE, cartCutters, 0);
    else
	{
	if (zoomedToBaseLevel)
	    cullCutters(&cutters, FALSE, NULL, 0);
	else if (winSize >= 20000 && winSize < 250000)
	    {	
	    struct slName *popularCutters = slNameListFromComma(CUTTERS_POPULAR);
	    cullCutters(&cutters, TRUE, popularCutters, 0);
	    }
	else if (winSize < 3000)
	    {
	    cullCutters(&cutters, FALSE, NULL, 5);
	    }
	else 
	    cullCutters(&cutters, FALSE, NULL, 6);
	}
    bedList = matchEnzymes(cutters, windowDna, winStart);
    if (bedList)
	tg->items = bedList;
    }
cutterFreeList(&cutters);
freeDnaSeq(&windowDna);
hFreeConn(&conn);
}

struct track *cuttersTg()
/* Track group for the restriction enzymes. */
{
struct track *tg = trackNew();
struct trackDb *tdb;

bedMethods(tg);
AllocVar(tdb);
tg->mapName = CUTTERS_TRACK_NAME;
tg->canPack = TRUE;
tg->visibility = tvHide;
tg->hasUi = TRUE;
tg->shortLabel = cloneString(CUTTERS_TRACK_LABEL);
tg->longLabel = cloneString(CUTTERS_TRACK_LONGLABEL);			    
tg->loadItems = cuttersLoad;
tg->drawItemAt = cuttersDrawAt;
tg->priority = 99.9;
tg->defaultPriority = 99.9;
tg->groupName = cloneString("map");
tg->defaultGroupName = cloneString("map");
tdb->tableName = cloneString(CUTTERS_TRACK_NAME);
tdb->shortLabel = cloneString(tg->shortLabel);
tdb->longLabel = cloneString(tg->longLabel);
tdb->grp = cloneString(tg->groupName);
tdb->priority = tg->priority;
trackDbPolish(tdb);
tg->tdb = tdb;
return tg;
}

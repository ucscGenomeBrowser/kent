#include "common.h"
#include "psl.h"
#include "cutter.h"
#include "hgTracks.h"
#include "hdb.h"
#include "cutterTrack.h"

void cuttersDrawAt(struct track *tg, void *item,
	struct vGfx *vg, int xOff, int y, double scale, 
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw the restriction enzyme at position. */
{
struct bed *bed = item;
int heightPer, x1, x2, w;
struct trackDb *tdb = tg->tdb;
int scoreMin, scoreMax;
if (!zoomedToBaseLevel)
    bedDrawSimpleAt(tg, item, vg, xOff, y, scale, font, color, vis);
else
    {
    heightPer = tg->heightPer;
    x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
    x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
    scoreMin = atoi(trackDbSettingOrDefault(tdb, "scoreMin", "0"));
    scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));

    if (tg->itemColor != NULL)
	color = tg->itemColor(tg, bed, vg);
    else
	{
	if (tg->colorShades)
	    color = tg->colorShades[grayInRange(bed->score, scoreMin, scoreMax)];
	}
    w = x2-x1;
    if (w < 1)
	w = 1;
    y += (heightPer >> 1) - 1;
    if (color)
	{
	struct sqlConnection *conn = hAllocConn2();
	struct cutter *cut;
	char query[80];
	char strand = bed->strand[0];
	char *s = bed->name;
	int letterWidth, cuts[4];
	int tickHeight = (heightPer>>1) - 1, tickWidth = 2;
	int i;

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
	if (cartUsualBoolean(cart,"complement",FALSE))
	    {
	    if (strand == '+') 
		strand = '-';
	    else if (strand == '-')
		strand = '+';
	    }
	if (strand == '+')
	    {
	    vgBox(vg, cuts[0], y - tickHeight, tickWidth, tickHeight, color);
	    vgBox(vg, cuts[1], y + 2, tickWidth, tickHeight, color);	    
	    }
	else if (strand == '-')
	    {
	    vgBox(vg, cuts[2], y - tickHeight, tickWidth, tickHeight, color);
	    vgBox(vg, cuts[3], y + 2, tickWidth, tickHeight, color);	    	    
	    }
	if (cut->palindromic)
	    {
	    vgBox(vg, cuts[0], y - tickHeight, tickWidth, tickHeight * 2 + 2, color);
	    vgBox(vg, cuts[1], y - tickHeight, tickWidth, tickHeight * 2 + 2, color);	    
	    }
	vgBox(vg, x1, y, w, 2, color);
	if (tg->drawName && vis != tvSquish)
	    {
	    /* Clip here so that text will tend to be more visible... */
	    w = x2-x1;
	    if (w > mgFontStringWidth(font, s))
		{
		Color textColor = contrastingColor(vg, color);
		vgTextCentered(vg, x1, y, w, heightPer, textColor, font, s);
		}
	    mapBoxHc(bed->chromStart, bed->chromEnd, x1, y, x2 - x1, heightPer,
		     tg->mapName, tg->mapItemName(tg, bed), NULL);
	    }
	hFreeConn2(&conn);
	cutterFree(&cut);
	}
    }
}

void cuttersLoad(struct track *tg)
{
struct sqlConnection *conn;
struct cutter *cutters;
struct slName *includes, *excludes;
struct dnaSeq *windowDna = NULL;
struct bed *plus = NULL, *minus = NULL, *bedList = NULL;

/* Do different things based on window size. */
if (winEnd - winStart < 1000)
    {
    windowDna = hDnaFromSeq(chromName, winStart, winEnd, dnaUpper);
    hSetDb2("hgFixed");
    conn = hAllocConn2();
    cutters = cutterLoadByQuery(conn, "select * from cutters");
    includes = sqlQuickList(conn, "select name from cuttersIncluded");
    excludes = sqlQuickList(conn, "select name from cuttersExcluded");
    /* Make the FALSE for the semicolon an hui.h default.  */ 
    cullCutters(cutters, FALSE, includes, excludes, 6);
    plus = matchEnzymes(cutters, windowDna, '+', TRUE, winStart);
    reverseComplement(windowDna->dna, windowDna->size);
    minus = matchEnzymes(cutters, windowDna, '-', FALSE, winStart);
    bedList = slCat(plus, minus);
    tg->items = bedList;
    hFreeConn2(&conn);
    slFreeList(&includes);
    slFreeList(&excludes);
    }
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
tg->hasUi = FALSE;
tg->shortLabel = cloneString(CUTTERS_TRACK_LABEL);
tg->longLabel = cloneString(CUTTERS_TRACK_LONGLABEL);			    
tg->loadItems = cuttersLoad;
tg->drawItemAt = cuttersDrawAt;
tg->priority = 99.9;
tg->groupName = "map";
tdb->tableName = CUTTERS_TRACK_NAME;
tdb->shortLabel = tg->shortLabel;
tdb->longLabel = tg->longLabel;
trackDbPolish(tdb);
tg->tdb = tdb;
return tg;
}

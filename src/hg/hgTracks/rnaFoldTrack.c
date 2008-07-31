/* rnaStructure - stuff to handle loading and coloring of rna
   secondary structure predictions.
*/

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "bed.h"
#include "rnaSecStr.h"
#include "rnautil.h"

static char const rcsid[] = "$Id: rnaFoldTrack.c,v 1.5.26.1 2008/07/31 02:24:16 markd Exp $";


void bedLoadItemBySqlResult(struct track *tg, struct sqlResult *sr, int rowOffset, ItemLoader loader)
/* Generic tg->item loader. */
{
char **row = NULL;
struct slList *itemList = NULL, *item = NULL;

if (NULL == sr)
    errAbort("While loading track %s, bedLoadItemSqlResult was given empty sqlResult", tg->mapName);

while ((row = sqlNextRow(sr)) != NULL)
    {
    item = loader(row + rowOffset);
    slAddHead(&itemList, item);
    }
slSort(&itemList, bedCmp);
tg->items = itemList;
}


void scoreFilterSqlClause(struct track *tg, char *extraWhere, size_t size, int defaultFilterScore)
/* Make a 'where' clause for the sql query if the scoreFilter has been set. */
{
char option[128]; /* Option -  score filter */
char *optionScoreVal;
int  optionScore = defaultFilterScore;

safef(option, sizeof(option), "%s.scoreFilter", tg->mapName);
optionScoreVal = trackDbSetting(tg->tdb, "scoreFilter");
if (optionScoreVal != NULL)
    optionScore = atoi(optionScoreVal);
optionScore = cartUsualInt(cart, option, optionScore);

safef(extraWhere, size, "score >= %d",optionScore);
}			   		   


void loadRnaSecStr(struct track *tg)
/* Load up rnaSecStr from database table to track items. */
{
struct sqlConnection *conn = hAllocConn(database);
int rowOffset = 0;
struct sqlResult *sr = NULL;
char extraWhere[128];

scoreFilterSqlClause(tg, extraWhere, sizeof(extraWhere), 0);
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, extraWhere, &rowOffset);
bedLoadItemBySqlResult(tg, sr, rowOffset, (ItemLoader)rnaSecStrLoad);

sqlFreeResult(&sr);
hFreeConn(&conn);
}

void freeRnaSecStr(struct track *tg)
/* Free up rnaSecStr items. */
{
rnaSecStrFreeList((struct rnaSecStr**)&tg->items);
}

void spreadAndColorString(struct hvGfx *hvg, int x, int y, int width, int height,
			  Color *colorIxs, int maxShade, MgFont *font, char *text, int count, 
			  double *scores, double minScore, double maxScore)
/* Draw evenly spaced letters in string. The scores array contains a
 * score for each letter in the text string. The score will determine
 * which color in the colorIxs will be used for a given
 * letter. ColorIxs is an array of size maxShade+1. The count param is
 * the number of bases to print, not length of the input line
 * (text) */
{
char c[2] = "";
int i;
int x1, x2;
Color clr;

for (i=0; i<count; i++, text++, scores++)
    {
    x1         = i * width / count;
    x2         = (i+1) * width/count;
    clr        = colorIxs[assignBin(*scores, minScore, maxScore, maxShade+1)];
    c[0]       = *text;
    hvGfxTextCentered(hvg, x1+x, y, x2-x1, height, clr, font, c);
    }
}

void spreadAndGrayShadeString(struct hvGfx *hvg, int x, int y, int width, int height,
			  MgFont *font, char *text, int count, double *scores, double minScore, double maxScore)
/* Draw evenly spaced letters in string and gray shade letters according to scores. */
{
spreadAndColorString(hvg, x, y, width, height, shadesOfGray+2, maxShade-2, font, text, count, scores, minScore, maxScore);
}

void spreadRnaFoldAnno(struct hvGfx *hvg, int x, int y, int width, int height, Color color, MgFont *font, struct rnaSecStr *item)
/* Draw parenthesis structure which defines rna secondary structure. */
{
char *fold     = cloneString(item->secStr);
double *scores = CloneArray(item->conf, item->size);
if (*item->strand == '-')
{
    reverseFold(fold);
    reverseDoubles(scores, item->size);
}
spreadAndGrayShadeString(hvg, x, y, width, height, font, fold, item->size, scores, 0.0, 1.0);
freeMem(fold);
freeMem(scores);
}


void drawCharBox(struct hvGfx *hvg, int x, int y, int width, int height, Color color, char *s, char c)
/* s defines a string which spans width. Draw a box in intervals
 * corresponding to positions where s has letter c */
{
double charWidth = ((double)width)/strlen(s);
int i = 0, begin = 0, length = 0;

for (;*s;s++,i++)
    {
    if (*s == c)
	{
	if (!length)
	    begin = i;
	length++;
	}
    /* Draw box if at end of interval or at end of string */
    if (*s != c || !*(s+1) )
	if (length)
	    {
	    int x1 = x + (int) begin *charWidth;
	    int w  = (int) length*charWidth;
	    hvGfxBox(hvg, x1, y, w, height, color);
	    length = 0;
	    }
    }
}


void colorSingleStranded(struct hvGfx *hvg, int x, int y, int width, int height, Color color, struct rnaSecStr *item)
{
char *fold = cloneString(item->secStr);
if (*item->strand == '-')
    reverseFold(fold);
drawCharBox(hvg, x, y, width, height, color, fold, '.');
freeMem(fold);
}


void rnaSecStrDrawAt(struct track *tg, void *item, 
		     struct hvGfx *hvg, int xOff, int y, 
		     double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw a single simple rnaSecStr item at position. */
{
struct rnaSecStr *rnaSecStr = item;
int heightPer = tg->heightPer;
int x1 = round((double)((int)rnaSecStr->chromStart-winStart)*scale) + xOff;
int x2 = round((double)((int)rnaSecStr->chromEnd-winStart)*scale) + xOff;
int w;
struct trackDb *tdb = tg->tdb;
int scoreMin = atoi(trackDbSettingOrDefault(tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingOrDefault(tdb, "scoreMax", "1000"));

if (tg->itemColor != NULL)
    color = tg->itemColor(tg, rnaSecStr, hvg);
else
    {
    if (tg->colorShades)
	color = tg->colorShades[grayInRange(rnaSecStr->score, scoreMin, scoreMax)];
    }
w = x2-x1;
if (w < 1)
    w = 1;
if (color)
    {
    if (zoomedToBaseLevel)
	spreadRnaFoldAnno(hvg, x1, y, w, heightPer, color, font, rnaSecStr);
    else {
    hvGfxBox(hvg, x1, y, w, heightPer, color);
    colorSingleStranded(hvg, x1, y, w, heightPer, lighterColor(hvg, color), rnaSecStr);
    if (tg->subType == lfWithBarbs || tg->exonArrows)
	{
	int dir = 0;
	if (rnaSecStr->strand[0] == '+')
	    dir = 1;
	else if(rnaSecStr->strand[0] == '-') 
	    dir = -1;
	if (dir != 0 && w > 2)
	    {
	    int midY = y + (heightPer>>1);
	    Color textColor = hvGfxContrastingColor(hvg, color);
	    clippedBarbs(hvg, x1, midY, w, 2, 5, dir, textColor, TRUE);
	    }
	}
    }
    if (tg->drawName && vis != tvSquish)
	{
	char *s = tg->itemName(tg, rnaSecStr);
	w = x2-x1;
	if (w > mgFontStringWidth(font, s))
	    {
	    Color textColor = hvGfxContrastingColor(hvg, color);
	    hvGfxTextCentered(hvg, x1, y, w, heightPer, textColor, font, s);
	    }
	mapBoxHc(hvg, rnaSecStr->chromStart, rnaSecStr->chromEnd, x1, y, x2 - x1, heightPer,
		 tg->mapName, tg->mapItemName(tg, rnaSecStr), NULL);
	}
    }
}

void rnaSecStrMethods(struct track *tg)
/* Make track which visualizes RNA secondary structure annotation. */
{
tg->loadItems  = loadRnaSecStr;
tg->freeItems  = freeRnaSecStr;
tg->drawItemAt = rnaSecStrDrawAt;
}


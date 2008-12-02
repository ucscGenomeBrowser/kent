/* encode.c - hgTracks routines that are specific to the ENCODE project */

#include "common.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgTracks.h"
#include "customTrack.h"
#include "encode.h"
#include "encode/encodeRna.h"
#include "encode/encodePeak.h"

static char const rcsid[] = "$Id: encode.c,v 1.13 2008/12/02 13:29:18 aamp Exp $";

#define SMALLBUF 128

char *encodeErgeName(struct track *tg, void *item)
/* return the actual data name, in form xx/yyyy cut off xx/ return yyyy */
{
char *name;
struct linkedFeatures *lf = item;
name = strstr(lf->name, "/");
if (name != NULL)
    name ++;
if (name != NULL)
    return name;
return "unknown";
}

void encodeErgeMethods(struct track *tg)
/* setup special methods for ENCODE dbERGE II tracks */
{
tg->itemName = encodeErgeName;
}

Color encodeStanfordNRSFColor(struct track *tg, void *item, struct hvGfx *hvg)
/* color by strand */
{
struct bed *thisItem = item;
int r = tg->color.r;
int g = tg->color.g;
int b = tg->color.b;

if (thisItem->strand[0] == '-')
    {
    r = g;
    g = b;
    b = tg->color.r;
    }
return hvGfxFindColorIx(hvg, r, g, b);
}

void encodeStanfordNRSFMethods(struct track *tg)
/* custom methods for ENCODE Stanford NRSF data */
{
tg->itemColor = encodeStanfordNRSFColor;
tg->itemNameColor = encodeStanfordNRSFColor;
}



void loadEncodeRna(struct track *tg)
/* Load up encodeRna from database table to track items. */
{
bedLoadItem(tg, "encodeRna", (ItemLoader)encodeRnaLoad);
}

void freeEncodeRna(struct track *tg)
/* Free up encodeRna items. */
{
encodeRnaFreeList((struct encodeRna**)&tg->items);
}

Color encodeRnaColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color of encodeRna track item. */
{
struct encodeRna *el = item;

if(el->isRmasked)     return MG_BLACK;
if(el->isTranscribed) return hvGfxFindColorIx(hvg, 0x79, 0xaa, 0x3d);
if(el->isPrediction)  return MG_RED;
return MG_BLUE;
}

char *encodeRnaName(struct track *tg, void *item)
/* Return RNA gene name. */
{
struct encodeRna *el = item;
char *full = el->name;
static char abbrev[SMALLBUF];
char *e;

strcpy(abbrev, skipChr(full));
subChar(abbrev, '_', ' ');
abbr(abbrev, " pseudogene");
if ((e = strstr(abbrev, "-related")) != NULL)
    strcpy(e, "-like");
return abbrev;
}

void encodeRnaMethods(struct track *tg)
/* Make track for rna genes . */
{
tg->loadItems = loadEncodeRna;
tg->freeItems = freeEncodeRna;
tg->itemName = encodeRnaName;
tg->itemColor = encodeRnaColor;
tg->itemNameColor = encodeRnaColor;
}

struct linkedFeatures *lfFromEncodePeak(struct slList *item)
/* Translate a switchDbTss thing into a linkedFeatures. */
{
struct encodePeak *peak = (struct encodePeak *)item;
struct linkedFeatures *lf;
struct simpleFeature *sfList = NULL;
if (!peak)
    return NULL;
AllocVar(lf);
lf->start = peak->chromStart;
lf->end = peak->chromEnd;
if (peak->peak > -1)
    {
    lf->tallStart = peak->chromStart + peak->peak;
    lf->tallEnd = lf->tallStart + 1;
    }
lf->filterColor = -1;
lf->orientation = orientFromChar(peak->strand[0]);
lf->grayIx = grayInRange((int)peak->score, 0, 1000);
safecpy(lf->name, sizeof(lf->name), peak->name);
if (peak->blockCount > 0)
    {
    int i;
    for (i = 0; i < peak->blockCount; i++)
	{
	struct simpleFeature *sf;
	AllocVar(sf);
	sf->start = lf->start + peak->blockStarts[i];
	sf->end = lf->start + peak->blockStarts[i] + peak->blockSizes[i];
	sf->grayIx = lf->grayIx;
	slAddHead(&sfList, sf);
	}
    slReverse(&sfList);
    }
else
    {
    AllocVar(sfList);
    sfList->start = lf->start;
    sfList->end = lf->end;
    sfList->grayIx = lf->grayIx;
    }
lf->components = sfList;
return lf;
}

static char *encodePeakFilter(char *trackName, struct trackDb *tdb, boolean isCustom)
{
struct dyString *extraWhere = newDyString(128);
char pValVarName[256];
char qValVarName[256];
char scoreVarName[256];
boolean useScore = FALSE;
safef(pValVarName, sizeof(pValVarName), "%s.%s", trackName, ENCODE_PEAK_PVAL_FILTER_SUFFIX);
safef(qValVarName, sizeof(qValVarName), "%s.%s", trackName, ENCODE_PEAK_QVAL_FILTER_SUFFIX);
safef(scoreVarName, sizeof(scoreVarName), "%s.%s", trackName, ENCODE_PEAK_SCORE_FILTER_SUFFIX);
if (!trackDbSettingOn(tdb, "filterPvalQval"))
    useScore = TRUE;
if (useScore && cartVarExists(cart, scoreVarName))
    {
    int score = cartUsualInt(cart, scoreVarName, -1);
    if ((score < 0) || (score > 1000))
	{
	warn("invalid score %s set in filter for track %s",
	     cartString(cart, scoreVarName), trackName);
	cartRemove(cart, scoreVarName);	     
	}
    else 
	dyStringPrintf(extraWhere, "(score >= %d)", score);
    }
else if (!useScore)
    {
    double pVal = cartUsualDouble(cart, pValVarName, 0);
    double qVal = cartUsualDouble(cart, qValVarName, 0);
    if (pVal < 0)
	{
	warn("invalid p-value %s set in filter for track %s", 
	     cartString(cart, pValVarName), trackName);
	cartRemove(cart, pValVarName);
	pVal = 0;
	}
    if ((qVal < 0) || (qVal > 1))
	{
	warn("invalid q-value %s set in filter for track %s", 
	     cartString(cart, qValVarName), trackName);
	cartRemove(cart, qValVarName);
	qVal = 0;
	}
    dyStringPrintf(extraWhere, "(pValue >= %f) and (qValue >= %f)", pVal, qVal);    
    }

if (sameString(extraWhere->string, ""))
    return NULL;
return dyStringCannibalize(&extraWhere);
}

static void encodePeakLoadItemsBoth(struct track *tg, struct customTrack *ct)
/* Load up an encodePeak table from the regular database or the customTrash one. */
{
char *db, *table;
struct sqlConnection *conn;
struct sqlResult *sr = NULL;
char **row;
char *filterConstraints = NULL;
int rowOffset;
struct linkedFeatures *lfList = NULL;
enum encodePeakType pt = 0;
if (ct)
    {
    db = CUSTOM_TRASH;
    table = ct->dbTableName;    
    }
else
    {
    db = database;
    table = tg->tdb->tableName;
    }
conn = hAllocConn(db);
pt = encodePeakInferTypeFromTable(db, table, tg->tdb->type);
tg->customInt = pt;
filterConstraints = encodePeakFilter(tg->tdb->tableName, tg->tdb, (ct!=NULL));
sr = hRangeQuery(conn, table, chromName, winStart, winEnd, filterConstraints, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct encodePeak *peak = encodePeakGeneralLoad(row + rowOffset, pt);
    struct linkedFeatures *lf = lfFromEncodePeak((struct slList *)peak);
    if (lf)
	slAddHead(&lfList, lf);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&lfList);
slSort(&lfList, linkedFeaturesCmp);
tg->items = lfList;
}

static void encodePeakLoadItemsNormal(struct track *tg)
/* Load the encodePeak table form the database. */
{
encodePeakLoadItemsBoth(tg, NULL);
}

static void encodePeakLoadItemsCt(struct track *tg)
/* Load the encodePeak table form the customTrash database. */
{
struct customTrack *ct = tg->customPt;
encodePeakLoadItemsBoth(tg, ct);
}

static void encodePeakDrawAt(struct track *tg, void *item,
	struct hvGfx *hvg, int xOff, int y, double scale,
	MgFont *font, Color color, enum trackVisibility vis)
/* Draw the peak from the linkedFeature.  Currently this doesn't draw any */
/* sorta shading based on the signalValue/pValue. */
{
struct linkedFeatures *lf = item;
int heightPer = tg->heightPer;
int shortOff = heightPer/4;
int shortHeight = heightPer - 2*shortOff;
Color rangeColor = shadesOfGray[lf->grayIx];
Color peakColor = (tg->ixColor != blackIndex()) ? tg->ixColor : getOrangeColor();;
if (lf->components)
    {
    struct simpleFeature *sf;
    drawScaledBox(hvg, lf->start, lf->end, scale, xOff, y+(heightPer/2), 1, rangeColor);
    for (sf = lf->components; sf != NULL; sf = sf->next)
	drawScaledBox(hvg, sf->start, sf->end, scale, xOff, y+shortOff,
		      shortHeight, rangeColor);
    }
else
    drawScaledBox(hvg, lf->start, lf->end, scale, xOff, y+shortOff,
		  shortHeight, rangeColor);
if ((lf->tallEnd > 0) && (lf->tallStart < lf->end))
    drawScaledBox(hvg, lf->tallStart, lf->tallEnd, scale, xOff, y,
		  heightPer, peakColor);
}

char *encodePeakItemName(struct track *tg, void *item)
/* Get rid of the '.' names */
{
struct linkedFeatures *lf = item;
if (lf->name && sameString(lf->name, "."))
    return "";
else
    return lf->name;
}

void encodePeakMethods(struct track *tg)
/* Methods for ENCODE peak track uses mostly linkedFeatures. */
{
linkedFeaturesMethods(tg);
tg->loadItems = encodePeakLoadItemsNormal;
tg->drawItemAt = encodePeakDrawAt;
tg->labelNextPrevItem = linkedFeaturesLabelNextPrevItem;
tg->itemName = encodePeakItemName;
tg->canPack = TRUE;
}

void encodePeakMethodsCt(struct track *tg)
/* Methods for ENCODE peak track uses mostly linkedFeatures. */
{
encodePeakMethods(tg);
tg->loadItems = encodePeakLoadItemsCt;
}

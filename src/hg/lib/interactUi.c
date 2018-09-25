/* interact track controls */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "trackDb.h"
#include "interactUi.h"

boolean isPopup = FALSE;

boolean interactUiDirectional(struct trackDb *tdb)
/* Determine if interactions are directional */
{
return isNotEmpty(trackDbSetting(tdb, INTERACT_DIRECTIONAL));
}

char *interactUiOffset(struct trackDb *tdb)
/* Determine whether to offset source or target (or neither if NULL) */
{
char *setting = trackDbSetting(tdb, INTERACT_DIRECTIONAL);
if (setting)
    {
    char *words[8];
    int count = chopByWhite(cloneString(setting), words, ArraySize(words));
    if (count >= 1)
        {
        char *offset = words[0];
        if (sameString(offset, INTERACT_OFFSET_TARGET) ||
             sameString(offset, INTERACT_OFFSET_SOURCE))
                return offset;
        }
    }
return NULL;
}

void interactUiMinScore(struct cart *cart, char *track, struct trackDb *tdb)
/* Minimum score */
{
char buffer[1024];
safef(buffer, sizeof buffer, "%s.%s", tdb->track, INTERACT_MINSCORE);
int minScore = cartUsualInt(cart, buffer, 0);
printf("<b>Minimum score:&nbsp;</b>");
cgiMakeIntVar(buffer, minScore, 0);
}

void interactUiTrackHeight(struct cart *cart, char *track, struct trackDb *tdb)
/* Input box to change track height */
{
// track height control
char buffer[1024];      
int min, max, deflt, current;
cartTdbFetchMinMaxPixels(cart, tdb, INTERACT_MINHEIGHT, INTERACT_MAXHEIGHT, 
                                atoi(INTERACT_DEFHEIGHT),
                                &min, &max, &deflt, &current);
safef(buffer, sizeof buffer, "%s.%s", track, INTERACT_HEIGHT);
printf("<b>Track height:&nbsp;</b>");
cgiMakeIntVar(buffer, current, 3);
printf("&nbsp;<span>pixels&nbsp;(range: %d to %d, default: %d)<span>",
        min, max, deflt);
}

void interactUiDrawMode(struct cart *cart, char *track, struct trackDb *tdb)
/* Radio buttons to select drawing mode */
{
char *drawMode = cartUsualStringClosestToHome(cart, tdb, isNameAtParentLevel(tdb, track),
                                                INTERACT_DRAW, INTERACT_DRAW_DEFAULT);
char cartVar[1024];
puts("<b>Draw mode:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, INTERACT_DRAW);
cgiMakeRadioButton(cartVar, INTERACT_DRAW_CURVE , sameString(INTERACT_DRAW_CURVE, drawMode));
printf("&nbsp;%s&nbsp;", "curve");
cgiMakeRadioButton(cartVar, INTERACT_DRAW_ELLIPSE, sameString(INTERACT_DRAW_ELLIPSE, drawMode));
printf("&nbsp;%s&nbsp;", "ellipse");
cgiMakeRadioButton(cartVar, INTERACT_DRAW_LINE, sameString(INTERACT_DRAW_LINE, drawMode));
printf("&nbsp;%s&nbsp;", "rectangle");
}

void interactUiEndpointFilter(struct cart *cart, char *track, struct trackDb *tdb)
/* Radio buttons to allow excluding items lacking endpoints in window */
{
char *endsVisible = cartUsualStringClosestToHome(cart, tdb, isNameAtParentLevel(tdb, track),
                                    INTERACT_ENDS_VISIBLE, INTERACT_ENDS_VISIBLE_DEFAULT);
char cartVar[1024];
puts("<b>Show interactions:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, INTERACT_ENDS_VISIBLE);
cgiMakeRadioButton(cartVar, INTERACT_ENDS_VISIBLE_ANY, sameString(INTERACT_ENDS_VISIBLE_ANY, endsVisible));
printf("&nbsp;%s&nbsp;", "all");
//printf("&nbsp;%s&nbsp;", "none");
cgiMakeRadioButton(cartVar, INTERACT_ENDS_VISIBLE_ONE, sameString(INTERACT_ENDS_VISIBLE_ONE, endsVisible));
printf("&nbsp;%s&nbsp;", "at least one end");
//printf("&nbsp;%s&nbsp;", "only one end");
cgiMakeRadioButton(cartVar, INTERACT_ENDS_VISIBLE_TWO , sameString(INTERACT_ENDS_VISIBLE_TWO, endsVisible));
printf("&nbsp;%s&nbsp;", "both ends in window");
//printf("&nbsp;%s&nbsp;", "no ends in window");
}

void interactUiDashedLines(struct cart *cart, char *track, struct trackDb *tdb)
/* Checkbox for dashed lines on interactions in reverse direction */
{
if (!interactUiDirectional(tdb))
    return;
char cartVar[1024];
safef(cartVar, sizeof cartVar, "%s.%s", tdb->track, INTERACT_DIRECTION_DASHES);
boolean doDashes = cartCgiUsualBoolean(cart, cartVar, INTERACT_DIRECTION_DASHES_DEFAULT);
cgiMakeCheckBox(cartVar, doDashes);
printf("Draw reverse direction interactions with dashed lines");
}

static char *interactMergeDefault(struct trackDb *tdb)
/* Determine whether to merge by source or target (or neither if NULL) */
{
char *setting = trackDbSetting(tdb, INTERACT_DIRECTIONAL);
if (setting)
    {
    char *words[8];
    int count = chopByWhite(cloneString(setting), words, ArraySize(words));
    if (count >= 2)
        {
        char *merge = words[1];
        if (sameString(merge, INTERACT_TDB_MERGE_TARGET))
            return INTERACT_MERGE_TARGET;
         if (sameString(merge, INTERACT_TDB_MERGE_SOURCE))
            return INTERACT_MERGE_SOURCE;
        }
    }
return NULL;
}

char *interactUiMergeMode(struct cart *cart, char *track, struct trackDb *tdb)
/* Get merge mode from trackDb and cart */
{
char *mergeDefault = interactMergeDefault(tdb);
if (!mergeDefault)
    return NULL;
char *mergeMode = cartUsualStringClosestToHome(cart, tdb, isNameAtParentLevel(tdb, track),
                                                INTERACT_MERGE, mergeDefault);
if (sameString(INTERACT_MERGE_SOURCE, mergeMode) ||
    sameString(INTERACT_MERGE_TARGET, mergeMode))
        return mergeMode;
return NULL;
}

static void interactUiSelectMergeMode(struct cart *cart, char *track, struct trackDb *tdb)
/* Radio buttons to specify merge by source or target */
{
char *mergeMode = interactUiMergeMode(cart, track, tdb);
char cartVar[1024];
puts("<b>Merge by:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, INTERACT_MERGE);
cgiMakeRadioButton(cartVar, INTERACT_MERGE_SOURCE, sameString(INTERACT_MERGE_SOURCE, mergeMode));
printf("&nbsp;%s&nbsp;", "source");
cgiMakeRadioButton(cartVar, INTERACT_MERGE_TARGET, sameString(INTERACT_MERGE_TARGET, mergeMode));
printf("&nbsp;%s&nbsp;", "target");
}

void interactCfgUi(char *database, struct cart *cart, struct trackDb *tdb, char *track,
                        char *title, boolean boxed)
/* Configure interact track type */
{
if (cartVarExists(cart, "ajax"))
    isPopup = TRUE;
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
if (startsWith("big", tdb->type))
    labelCfgUi(database, cart, tdb);
//printf("\n<table id=interactControls style='font-size:%d%%' %s>\n<tr><td>",
        //isPopup ? 75 : 100, boxed ?" width='100%'":"");
puts("<p>");
interactUiEndpointFilter(cart, track, tdb);
puts("</p><p>");
if (interactUiMergeMode(cart, track, tdb))
    {
    interactUiSelectMergeMode(cart, track, tdb);
    }
else
    {
    interactUiTrackHeight(cart, track, tdb);
    puts("</p><p>");
    interactUiDrawMode(cart, track, tdb);
    puts("</p><p>");
    interactUiDashedLines(cart, track, tdb);
    puts("</p>");
    }
scoreCfgUi(database, cart,tdb,tdb->track,"",1000,FALSE);
cfgEndBox(boxed);
}

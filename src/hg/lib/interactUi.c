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
return trackDbSettingClosestToHomeOn(tdb, INTERACT_DIRECTIONAL);
}

void interactUiMinScore(struct cart *cart, char *track, struct trackDb *tdb)
/* Minimum score */
{
char buffer[1024];
safef(buffer, sizeof buffer, "%s.%s", tdb->track, INTERACT_MINSCORE);
int minScore = cartUsualInt(cart, buffer, INTERACT_DEFMINSCORE);
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
printf("<br><br><b>Track height:&nbsp;</b>");
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
interactUiMinScore(cart, track, tdb);
interactUiTrackHeight(cart, track, tdb);
puts("<div>");
interactUiDrawMode(cart, track, tdb);
//puts("\n</table>\n");
cfgEndBox(boxed);
}

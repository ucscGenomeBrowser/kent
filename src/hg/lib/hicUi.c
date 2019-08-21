/* hic track controls */

/* Copyright (C) 2019 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "web.h"
#include "trackDb.h"
#include "hicUi.h"
#include "Cstraw.h"
#include "regexHelper.h"
#include "obscure.h"

char *hicUiFetchNormalization(struct cart *cart, char *track, struct hicMeta *meta)
/* Return the current normalization selection, or the default if none
 * has been selected.  Right now this is a hard-coded set specifically for
 * .hic files, but in the future this list might be dynamically determined by
 * the contents and format of the Hi-C file. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_NORMALIZATION);
char *selected = cartNonemptyString(cart, cartVar);
char *menu[] = {"NONE", "VC", "VC_SQRT", "KR"};
int i, sanityCheck = 0;
for (i=0; i<4; i++)
    {
    if (sameOk(selected, menu[i]))
        sanityCheck = 1;
    }
if (!sanityCheck)
    {
    selected = menu[0];
    }
return selected;
}

void hicUiNormalizationMenu(struct cart *cart, char *track, struct hicMeta *meta)
/* Draw a menu to select the normalization method to use. */
{
char cartVar[1024];
printf("<b>Score normalization:</b> ");
char* selected = hicUiFetchNormalization(cart, track, meta);
char *menu[] = {"NONE", "VC", "VC_SQRT", "KR"};
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_NORMALIZATION);
cgiMakeDropList(cartVar, menu, 4, selected);
}

char *hicUiFetchResolution(struct cart *cart, char *track, struct hicMeta *meta)
/* Return the current resolution selection, or the default if none
 * has been selected. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_RESOLUTION);
char *selected = cartNonemptyString(cart, cartVar);
int sanityCheck = sameOk(selected, "Auto");
int i;
for (i=0; i<meta->nRes; i++)
    {
    if (sameOk(selected, meta->resolutions[i]))
        sanityCheck = 1;
    }
if (!sanityCheck)
    selected = "Auto";
return selected;
}

int hicUiFetchResolutionAsInt(struct cart *cart, char *track, struct hicMeta *meta, int windowSize)
/* Return the current resolution selection as an integer.  If there is no selection, or if "Auto"
 * has been selected, return the largest available value that still partitions the window into at
 * least 5000 bins. */
{
char *resolutionString = hicUiFetchResolution(cart, track, meta);
int result;
if (sameString(resolutionString, "Auto"))
    {
    int idealRes = windowSize/5000;
    char *autoRes = meta->resolutions[meta->nRes-1];
    int i;
    for (i=meta->nRes-1; i>= 0; i--)
        {
        if (atoi(meta->resolutions[i]) < idealRes)
            {
            autoRes = meta->resolutions[i];
            break;
            }
        }
    result = atoi(autoRes);
    }
else
    result = atoi(resolutionString);
return result;
}

void hicUiResolutionMenu(struct cart *cart, char *track, struct hicMeta *meta)
/* Draw a menu to select which binSize to use for fetching data */
{
char cartVar[1024];
char autoscale[10] = "Auto";
printf("<b>Resolution:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_RESOLUTION);
char **menu = NULL;
AllocArray(menu, meta->nRes+1);
char **values = NULL;
AllocArray(values, meta->nRes+1);
menu[0] = autoscale;
values[0] = autoscale;
int i;
for (i=1; i<meta->nRes+1; i++)
    {
    char buffer[1024];
    long long value = atoll(meta->resolutions[i-1]);
    sprintWithMetricBaseUnit(buffer, sizeof(buffer), value);
    menu[i] = cloneString(buffer);
    values[i] = cloneString(meta->resolutions[i-1]);
    }
char *selected = hicUiFetchResolution(cart, track, meta);
cgiMakeDropListWithVals(cartVar, menu, values, meta->nRes+1, selected);
free(menu);
}


char *hicUiFetchDrawMode(struct cart *cart, char *track)
/* Return the current draw mode selection, or the default if none
 * has been selected. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_MODE);
char* selected = cartNonemptyString(cart, cartVar);
if (    !sameOk(selected, HIC_DRAW_MODE_SQUARE) &&
        !sameOk(selected, HIC_DRAW_MODE_ARC) &&
        !sameOk(selected, HIC_DRAW_MODE_TRIANGLE) )
    {
    selected = HIC_DRAW_MODE_DEFAULT;
    }
return selected;
}


void hicUiDrawMenu(struct cart *cart, char *track)
/* Draw the list of draw mode options for Hi-C tracks.  Square is the
 * standard square-shaped heatmap with the chromosome position axis on
 * a diagonal from top left to bottom right.  Triangle is the top half
 * of that square, after rotating it 45 degrees to have the position
 * axis lie horizontally.  Arc draws arcs between related positions,
 * but skips over self-relations. */
{
char cartVar[1024];
printf("<b>Draw mode:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_MODE);
char *menu[] = {HIC_DRAW_MODE_SQUARE, HIC_DRAW_MODE_TRIANGLE, HIC_DRAW_MODE_ARC};
char* selected = hicUiFetchDrawMode(cart, track);
cgiMakeDropList(cartVar, menu, 3, selected);
}


char *hicUiFetchDrawColor(struct cart *cart, char *track)
/* Retrieve the HTML hex code for the color to draw the
 * track values in (e.g., #00ffa1) */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_COLOR);
char* selected = cartNonemptyString(cart, cartVar);
if (selected == NULL)
    selected = HIC_DRAW_COLOR_DEFAULT;
const char *colorExpr ="^#[0-9a-fA-F]{6}$";
if (!regexMatch(selected, colorExpr))
    {
    selected = HIC_DRAW_COLOR_DEFAULT;
    }
return selected;
}

char *hicUiFetchBgColor(struct cart *cart, char *track)
/* Retrieve the HTML hex code of the background color for the 
 * track.  This is the color associated with scores at or close to 0. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_BG_COLOR);
char* selected = cartNonemptyString(cart, cartVar);
if (selected == NULL)
    selected = HIC_DRAW_BG_COLOR_DEFAULT;
const char *colorExpr ="^#[0-9a-fA-F]{6}$";
if (!regexMatch(selected, colorExpr))
    {
    selected = HIC_DRAW_BG_COLOR_DEFAULT;
    }
return selected;
}

double hicUiFetchMaxValue(struct cart *cart, char *track)
/* Retrieve the score value at which the draw color reaches its
 * its maximum intensity.  Any scores above this value will
 * share that same draw color. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_MAX_VALUE);
return cartUsualDouble(cart, cartVar, HIC_DRAW_MAX_VALUE_DEFAULT);
}


void hicUiAddAutoScaleJS(struct cart *cart, char *track)
/* Write out a bit of javascript to associate checking/unchecking the autoscale
 * checkbox with deactivating/activating (respectively) the maximum score
 * input.  */
{
struct dyString *new = dyStringNew(0);
dyStringPrintf(new, "$('input[name=\"%s.%s\"]')[0].onclick = function() {", track, HIC_DRAW_AUTOSCALE);
dyStringPrintf(new, "if (this.checked) {$('input[name=\"%s.%s\"]')[0].disabled = true; $('span#hicMaxText').css('color', 'gray');}", track, HIC_DRAW_MAX_VALUE);
dyStringPrintf(new, "else {$('input[name=\"%s.%s\"]')[0].disabled = false; $('span#hicMaxText').css('color', '');} };\n", track, HIC_DRAW_MAX_VALUE);
dyStringPrintf(new, "if ($('input[name=\"%s.%s\"]')[0].checked) {$('input[name=\"%s.%s\"]')[0].disabled = true; $('span#hicMaxText').css('color', 'gray');}\n",
        track, HIC_DRAW_AUTOSCALE, track, HIC_DRAW_MAX_VALUE);
jsInline(dyStringContents(new));
dyStringFree(&new);
}

boolean hicUiFetchAutoScale(struct cart *cart, char *track)
/* Returns whether the track is configured to automatically scale its color range
 * depending on the scores present in the window, or if it should stick to a
 * gradient based on the user's selected maximum value. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_AUTOSCALE);
return cartUsualBoolean(cart, cartVar, TRUE);
}


void hicUiColorMenu(struct cart *cart, char *track)
/* Draw the menu inputs associated with selecting draw colors for the track. */
{
char cartVar[1024];
printf("<b>Color:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_COLOR);
char* selected = hicUiFetchDrawColor(cart, track);
printf("<input type='color' name='%s' value='%s' />\n", cartVar, selected);

// Leaving out background color options for now.  We'll see if this option is requested.
/*
printf("<b>Background color:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_BG_COLOR);
selected = hicUiFetchBgColor(cart, track);
printf("<input type='color' name='%s' value='%s' />\n", cartVar, selected);
*/
}

void hicUiMaxOptionsMenu(struct cart *cart, char *track)
/* Draw the menu inputs associated with selecting whether the track should automatically
 * scale its color gradient based on the scores present in the view window, or whether it
 * should stick to a gradient based on a user-selected maximum score. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_MAX_VALUE);
double currentMax = hicUiFetchMaxValue(cart, track);
printf("<span id='hicMaxText'><b>Maximum:</b></span> ");
cgiMakeDoubleVar(cartVar, currentMax, 6);

printf("&nbsp;&nbsp;<b>Auto-scale:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_AUTOSCALE);
boolean autoscaleChecked = hicUiFetchAutoScale(cart, track);
cgiMakeCheckBox(cartVar, autoscaleChecked);

hicUiAddAutoScaleJS(cart, track);
}

void hicCfgUi(char *database, struct cart *cart, struct trackDb *tdb, char *track,
                        char *title, boolean boxed)
/* Draw the list of track configuration options for Hi-C tracks */
{

boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
char *filename = trackDbSetting(tdb, "bigDataUrl");
if (filename == NULL)
    errAbort("Error: no bigDataUrl provided in trackDb for hic track %s", tdb->track);
struct hicMeta *trackMeta;
char *errMsg = hicLoadHeader(filename, &trackMeta, database);
if (errMsg != NULL)
    errAbort("Error retrieving file info: %s", errMsg);

puts("<p>");
printf("Items are drawn in shades of the chosen color depending on score - scores above the "
        "chosen maximum are drawn at full intensity.</p><p>\n");
hicUiNormalizationMenu(cart, track, trackMeta);
puts("&nbsp;&nbsp;");
hicUiMaxOptionsMenu(cart, track);

puts("</p><p>");
hicUiDrawMenu(cart, track);
puts("&nbsp;&nbsp;");
hicUiResolutionMenu(cart, track, trackMeta);
puts("&nbsp;&nbsp;");
hicUiColorMenu(cart, track);
cfgEndBox(boxed);
}

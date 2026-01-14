/* hic track controls */

/* Copyright (C) 2019 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "web.h"
#include "trackDb.h"
#include "hicUi.h"
#include "cStraw.h"
#include "regexHelper.h"
#include "obscure.h"
#include "htmshell.h"
#include "htmlColor.h"

boolean hicUiArcLimitEnabled(struct cart *cart, struct trackDb *tdb)
/* Returns true if the checkbox for limiting the number of displayed arcs is checked.
 * Defaults to true. */
{
return cartOrTdbBoolean(cart, tdb, HIC_ARC_LIMIT_CHECKBOX, TRUE);
}

int hicUiGetArcLimit(struct cart *cart, struct trackDb *tdb)
/* Returns the currently configured limit on the number of arcs drawn in arc mode.
 * Defaults to 10000.
 */
{
int arcLimit = cartOrTdbInt(cart, tdb, HIC_ARC_LIMIT, 10000);
return arcLimit;
}

void hicUiAddArcLimitJS(struct cart *cart, char *track)
/* Write out a bit of javascript to associate checking/unchecking the arc count
 * limit checkbox with deactivating/activating (respectively) the text
 * input.  */
{
struct dyString *new = dyStringNew(0);
dyStringPrintf(new, "$('input[name=\"%s.%s\"]')[0].onclick = function() {", track, HIC_ARC_LIMIT_CHECKBOX);
dyStringPrintf(new, "if (this.checked) {$('input[name=\"%s.%s\"]')[0].disabled = false; $('span#hicArcLimitText').css('color', '');}", track, HIC_ARC_LIMIT);
dyStringPrintf(new, "else {$('input[name=\"%s.%s\"]')[0].disabled = true; $('span#hicArcLimitText').css('color', 'gray');} };\n", track, HIC_ARC_LIMIT);
dyStringPrintf(new, "if (!$('input[name=\"%s.%s\"]')[0].checked) {$('input[name=\"%s.%s\"]')[0].disabled = true; $('span#hicArcLimitText').css('color', 'gray');}\n",
        track, HIC_ARC_LIMIT_CHECKBOX, track, HIC_ARC_LIMIT);
jsInline(dyStringContents(new));
dyStringFree(&new);
}

void hicUiArcLimitSection(struct cart *cart, struct trackDb *tdb)
/* Draw the menu inputs associated with selecting whether the track should automatically
 * scale its color gradient based on the scores present in the view window, or whether it
 * should stick to a gradient based on a user-selected maximum score. */
{
printf("\n");
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_ARC_LIMIT_CHECKBOX);
boolean arcLimitChecked = hicUiArcLimitEnabled(cart, tdb);
cgiMakeCheckBox(cartVar, arcLimitChecked);
int currentLimit = hicUiGetArcLimit(cart, tdb);
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_ARC_LIMIT);
printf("&nbsp;&nbsp;<span id='hicArcLimitText'><b>Limit arcs to the</b> ");
cgiMakeIntVar(cartVar, currentLimit, 6);
printf("<b> strongest interactions</b> </span>\n");

hicUiAddArcLimitJS(cart, tdb->track);
}


char *hicUiFetchNormalization(struct cart *cart, struct trackDb *tdb, struct hicMeta *meta)
/* Return the current normalization selection, or the default if none
 * has been selected.  Right now this is a hard-coded set specifically for
 * .hic files, but in the future this list might be dynamically determined by
 * the contents and format of the Hi-C file. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_NORMALIZATION);
char *selected = cartNonemptyString(cart, cartVar);
if (selected == NULL)
    selected = trackDbSetting(tdb, HIC_TDB_NORMALIZATION);
char **menu = meta->normOptions;
int i;
char *result = menu[0];
for (i=1; i<meta->nNormOptions; i++)
    {
    if (sameWordOk(selected, menu[i]))
        result = menu[i];
    }
return result;
}

void hicUiNormalizationDropDown(struct cart *cart, struct trackDb *tdb, struct hicMeta *meta)
{
char cartVar[1024];
char* selected = hicUiFetchNormalization(cart, tdb, meta);
char **menu = meta->normOptions;
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_NORMALIZATION);
cgiMakeDropList(cartVar, menu, meta->nNormOptions, selected);
}

void hicUiNormalizationMenu(struct cart *cart, struct trackDb *tdb, struct hicMeta *meta)
/* Draw a menu to select the normalization method to use. */
{
printf("<b>Score normalization:</b> ");
hicUiNormalizationDropDown(cart, tdb, meta);
}

char *hicUiFetchResolution(struct cart *cart, struct trackDb *tdb, struct hicMeta *meta)
/* Return the current resolution selection, or the default if none
 * has been selected. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_RESOLUTION);
char *selected = cartNonemptyString(cart, cartVar);
if (selected == NULL)
    selected = trackDbSetting(tdb, HIC_TDB_RESOLUTION);
int i;
char *result = "Auto";
for (i=0; i<meta->nRes; i++)
    {
    if (sameOk(selected, meta->resolutions[i]))
        result = selected;
    }
return result;
}

int hicUiFetchResolutionAsInt(struct cart *cart, struct trackDb *tdb, struct hicMeta *meta, int windowSize)
/* Return the current resolution selection as an integer.  If there is no selection, or if "Auto"
 * has been selected, return the largest available value that still partitions the window into at
 * least 500 bins. */
{
char *resolutionString = hicUiFetchResolution(cart, tdb, meta);
int result;
if (sameWordOk(resolutionString, "Auto"))
    {
    int idealRes = windowSize/500;
    int autoRes = atoi(meta->resolutions[meta->nRes-1]);
    int smallestRes = autoRes; // in case the ideal resolution is smaller than anything available
    int i, success = 0;
    for (i=meta->nRes-1; i>= 0; i--)
        {
        int thisRes = atoi(meta->resolutions[i]);
        if (thisRes < smallestRes)
            smallestRes = thisRes; // not sure about the sort order of the list
        if (thisRes < idealRes && thisRes >= autoRes)
            {
            autoRes = thisRes;
            success = 1;
            }
        }
    if (success)
        result = autoRes;
    else
        result = smallestRes;
    }
else
    result = atoi(resolutionString);
return result;
}

void hicUiResolutionDropDown(struct cart *cart, struct trackDb *tdb, struct hicMeta *meta)
{
char cartVar[1024];
char autoscale[10] = "Auto";
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_RESOLUTION);
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
char *selected = hicUiFetchResolution(cart, tdb, meta);
cgiMakeDropListWithVals(cartVar, menu, values, meta->nRes+1, selected);
freeMem(menu);
}

void hicUiResolutionMenu(struct cart *cart, struct trackDb *tdb, struct hicMeta *meta)
/* Draw a menu to select which binSize to use for fetching data */
{
printf("<b>Resolution:</b> ");
hicUiResolutionDropDown(cart, tdb, meta);
}


char *hicUiFetchDrawMode(struct cart *cart, struct trackDb *tdb)
/* Return the current draw mode selection, or the default if none
 * has been selected. */
{
char *selected = cartOptionalStringClosestToHome(cart, tdb, FALSE, HIC_DRAW_MODE);
if (selected == NULL)
    {
    selected = trackDbSetting(tdb, HIC_TDB_DRAW_MODE);
    }
char *result = HIC_DRAW_MODE_DEFAULT;
if (sameWordOk(selected, HIC_DRAW_MODE_SQUARE))
    result = HIC_DRAW_MODE_SQUARE;
else if (sameWordOk(selected, HIC_DRAW_MODE_ARC))
    result = HIC_DRAW_MODE_ARC;
else if (sameWordOk(selected, HIC_DRAW_MODE_TRIANGLE))
    result = HIC_DRAW_MODE_TRIANGLE;
return result;
}

boolean hicUiFetchInverted(struct cart *cart, struct trackDb *tdb)
/* Check if the user has set this track to draw in inverted mode.
 * Ideally this would also be available via a trackDb setting, but
 * this is the first pass at this feature. */
{
boolean defaultVal = FALSE;
//return cartUsualBooleanClosestToHome(cart, tdb, FALSE, HIC_DRAW_INVERTED, defaultVal);
return cartOrTdbBoolean(cart, tdb, HIC_DRAW_INVERTED, defaultVal);
}


void hicUiDrawMenu(struct cart *cart, struct trackDb *tdb)
/* Draw the list of draw mode options for Hi-C tracks.  Square is the
 * standard square-shaped heatmap with the chromosome position axis on
 * a diagonal from top left to bottom right.  Triangle is the top half
 * of that square, after rotating it 45 degrees to have the position
 * axis lie horizontally.  Arc draws arcs between related positions,
 * but skips over self-relations. */
{
char cartVar[1024];
printf("<b>Draw mode:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_DRAW_MODE);
char *menu[] = {HIC_DRAW_MODE_SQUARE, HIC_DRAW_MODE_TRIANGLE, HIC_DRAW_MODE_ARC};
char* selected = hicUiFetchDrawMode(cart, tdb);
cgiMakeDropList(cartVar, menu, 3, selected);
puts("&nbsp;&nbsp;");
printf("<b>Invert:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_DRAW_INVERTED);
cgiMakeCheckBox(cartVar, hicUiFetchInverted(cart, tdb));
}


char *hicUiFetchDrawColor(struct cart *cart, struct trackDb *tdb)
/* Retrieve the HTML hex code for the color to draw the
 * track values in (e.g., #00ffa1) */
{
// Color might have been specified in the cart, probably in #aabbcc format
char* selected = cartOptionalStringClosestToHome(cart, tdb, FALSE, HIC_DRAW_COLOR);
if (selected == NULL)
    // Or color might be in trackDb, probably in R,G,B format
    selected = trackDbSettingClosestToHomeOrDefault(tdb, HIC_TDB_COLOR, HIC_DRAW_COLOR_DEFAULT);
const char *commaColorExpr = "^[0-9]+,[0-9]+,[0-9]+$";
if (regexMatch(selected, commaColorExpr))
    {
    // Parse a color in %d,%d,%d format and convert to something the html color tool can use.
    // Don't want to use pre-parsed trackDb color values just in case string came from the cart instead
    unsigned char r, g, b;
    unsigned unifiedColor;
    parseColor(selected, &r, &g, &b);
    htmlColorFromRGB(&unifiedColor, r, g, b);
    char *hexColor = htmlColorToCode(unifiedColor);
    selected = hexColor;
    }
const char *colorExpr ="^#[0-9a-fA-F]{6}$";
if (!regexMatch(selected, colorExpr))
    {
    selected = HIC_DRAW_COLOR_DEFAULT;
    }
return selected;
}

char *hicUiFetchBgColor(struct cart *cart, struct trackDb *tdb)
/* Retrieve the HTML hex code of the background color for the 
 * track.  This is the color associated with scores at or close to 0. */
{
char* selected = cartOptionalStringClosestToHome(cart, tdb, FALSE, HIC_DRAW_BG_COLOR);
if (selected == NULL)
    selected = HIC_DRAW_BG_COLOR_DEFAULT;
const char *colorExpr ="^#[0-9a-fA-F]{6}$";
if (!regexMatch(selected, colorExpr))
    {
    selected = HIC_DRAW_BG_COLOR_DEFAULT;
    }
return selected;
}

double hicUiFetchMaxValue(struct cart *cart, struct trackDb *tdb)
/* Retrieve the score value at which the draw color reaches its
 * its maximum intensity.  Any scores above this value will
 * share that same draw color. */
{
double defaultValue = HIC_DRAW_MAX_VALUE_DEFAULT;
char *tdbString = trackDbSetting(tdb, HIC_TDB_MAX_VALUE);
if (!isEmpty(tdbString))
    defaultValue = atof(tdbString);
return cartUsualDoubleClosestToHome(cart, tdb, FALSE, HIC_DRAW_MAX_VALUE, defaultValue);
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

boolean hicUiFetchAutoScale(struct cart *cart, struct trackDb *tdb)
/* Returns whether the track is configured to automatically scale its color range
 * depending on the scores present in the window, or if it should stick to a
 * gradient based on the user's selected maximum value. */
{
boolean defaultVal = TRUE;
char *tdbSetting = trackDbSetting(tdb, HIC_TDB_AUTOSCALE);
if (tdbSetting != NULL)
    defaultVal = trackDbSettingClosestToHomeOn(tdb, HIC_TDB_AUTOSCALE);
return cartUsualBooleanClosestToHome(cart, tdb, FALSE, HIC_DRAW_AUTOSCALE, defaultVal);
}


void hicUiColorMenu(struct cart *cart, struct trackDb *tdb)
/* Draw the menu inputs associated with selecting draw colors for the track. */
{
char cartVar[1024];
printf("<b>Color:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_DRAW_COLOR);
char* selected = hicUiFetchDrawColor(cart, tdb);
printf("<input type='color' name='%s' value='%s' />\n", cartVar, selected);

// Leaving out background color options for now.  We'll see if this option is requested.
/*
printf("<b>Background color:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, HIC_DRAW_BG_COLOR);
selected = hicUiFetchBgColor(cart, track);
printf("<input type='color' name='%s' value='%s' />\n", cartVar, selected);
*/
}

void hicUiMaxOptionsMenu(struct cart *cart, struct trackDb *tdb, boolean isComposite)
/* Draw the menu inputs associated with selecting whether the track should automatically
 * scale its color gradient based on the scores present in the view window, or whether it
 * should stick to a gradient based on a user-selected maximum score. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_DRAW_MAX_VALUE);
double currentMax = hicUiFetchMaxValue(cart, tdb);
printf("<span id='hicMaxText'><b>%sMaximum:</b></span> ", isComposite ? "Score " : "");
cgiMakeDoubleVar(cartVar, currentMax, 6);

printf("&nbsp;&nbsp;<b>Auto-scale:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_DRAW_AUTOSCALE);
boolean autoscaleChecked = hicUiFetchAutoScale(cart, tdb);
cgiMakeCheckBox(cartVar, autoscaleChecked);

hicUiAddAutoScaleJS(cart, tdb->track);
}



void hicUiFileDetails(struct hicMeta *trackMeta)
{
int i;
printf("</p><hr>\nMetadata from file header:<br>\n");
printf("<div style='margin-left: 2em'>\n");
printf("<label class='trackUiHicLabel'>Genome: %s\n<br></label>", trackMeta->fileAssembly);
char scriptline[2048];
for (i=0; i<trackMeta->nAttributes-1; i+=2)
    {
    printf("<label class='trackUiHicLabelExpand trackUiHicAttrToggle%d'>%s <img src='%s' class='trackUiHicLabelArrow'></label><br>", i, htmlEncode(trackMeta->attributes[i]), "../images/ab_right.gif");
    printf("<div class='trackUiHicHiddenAttributes hicAttr%d'>\n", i);
    printf("<pre>%s</pre>", htmlEncode(trackMeta->attributes[i+1]));
    printf("</div>\n");
    
    safef(scriptline, sizeof(scriptline), "$('label.trackUiHicAttrToggle%d').click(function() {$(this).children('img').toggleClass('open'); $('div.hicAttr%d').toggle();});", i, i);
    jsInline(scriptline);
    }
printf("</div>\n");
printf("<p>For questions concerning the content of a file's metadata header, please contact the file creator.</p>\n");
}

double hicUiMaxInteractionRange(struct cart *cart, struct trackDb *tdb)
/* Retrieve the maximum range for an interaction to be drawn.  Range is
 * calculated from the left-most start to the right-most end of the interaction. */
{
double defaultValue = 0, returnVal = 0;
char *tdbString = trackDbSetting(tdb, HIC_TDB_MAX_DISTANCE);
if (!isEmpty(tdbString))
    defaultValue = atof(tdbString);
returnVal = cartUsualDoubleClosestToHome(cart, tdb, FALSE, HIC_DRAW_MAX_DISTANCE, defaultValue);
if (returnVal < 0)
    return 0;
return returnVal;
}

double hicUiMinInteractionRange(struct cart *cart, struct trackDb *tdb)
/* Retrieve the minimum range for an interaction to be drawn.  Range is
 * calculated from the left-most start to the right-most end of the interaction. */
{
double defaultValue = 0, returnVal = 0;
char *tdbString = trackDbSetting(tdb, HIC_TDB_MIN_DISTANCE);
if (!isEmpty(tdbString))
    defaultValue = atof(tdbString);
returnVal = cartUsualDoubleClosestToHome(cart, tdb, FALSE, HIC_DRAW_MIN_DISTANCE, defaultValue);
if (returnVal < 0)
    return 0;
return returnVal;
}

void hicUiMinMaxRangeMenu(struct cart *cart, struct trackDb *tdb)
{
char cartVar[2048];
double minRange = hicUiMinInteractionRange(cart, tdb);
double maxRange = hicUiMaxInteractionRange(cart, tdb);
printf("<b>Filter by interaction distance in bp (0 for no limit):</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_DRAW_MIN_DISTANCE);
printf("minimum ");
cgiMakeDoubleVarWithMin(cartVar, minRange, NULL, 0, 0);
safef(cartVar, sizeof(cartVar), "%s.%s", tdb->track, HIC_DRAW_MAX_DISTANCE);
printf(" maximum ");
cgiMakeDoubleVarWithMin(cartVar, maxRange, NULL, 0, 0);
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
hicUiNormalizationMenu(cart, tdb, trackMeta);
puts("&nbsp;&nbsp;");
hicUiMaxOptionsMenu(cart, tdb, FALSE);

puts("</p><p>");
hicUiDrawMenu(cart, tdb);
puts("&nbsp;&nbsp;");
hicUiResolutionMenu(cart, tdb, trackMeta);
puts("&nbsp;&nbsp;");
hicUiColorMenu(cart, tdb);
puts("</p><p>\n");
hicUiMinMaxRangeMenu(cart, tdb);
puts("</p><p>\n");
hicUiArcLimitSection(cart, tdb);
puts("</p><p>\n");
hicUiFileDetails(trackMeta);
cfgEndBox(boxed);
}

void hicCfgUiComposite(struct cart *cart, struct trackDb *tdb, char *track,
                        char *title, boolean boxed)
/* Draw the (empty) list of track configuration options for a composite of Hi-C tracks */
{
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);

puts("<p>");
printf("Items are drawn in shades of the chosen color depending on score - scores above the "
        "chosen maximum are drawn at full intensity.</p><p>\n");
hicUiMaxOptionsMenu(cart, tdb, TRUE);
puts("</p><p>");
hicUiDrawMenu(cart, tdb);
puts("&nbsp;&nbsp;");
hicUiColorMenu(cart, tdb);
puts("</p><p>\n");
hicUiMinMaxRangeMenu(cart, tdb);
puts("</p><p>\n");
puts("Subtracks below have additional file-specific configuration options for resolution and normalization.\n</p>");
cfgEndBox(boxed);
}



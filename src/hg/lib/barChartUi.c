/* Bar chart track controls */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "trackDb.h"
#include "jsHelper.h"
#include "hCommon.h"
#include "rainbow.h"
#include "htmlColor.h"
#include "barChartCategory.h"
#include "barChartUi.h"

/* Restrict features on right-click (popup) version */
static boolean isPopup = FALSE;

/* Convenience functions for category filter controls */


char *makeCategoryLabel(struct barChartCategory *categ)
/* Display category color and label */
{
char buf[256];
safef(buf, sizeof(buf), "<td class='bcColorPatch' bgcolor=#%06x></td>"
                        "<td>&nbsp;%s</td>", 
                                categ->color, categ->label);
return(cloneString(buf));
}

struct categorySelect
    {
    struct categorySelect *next;
    char *name;
    char *label;
    boolean checked;
    };

static void makeGroupCheckboxes(char *name, char *title, struct categorySelect *selects)
{
#define TABLE_COLUMNS 1
if (title != NULL)
    printf("<tr><td colspan=10><i><b>%s</b></i></td></tr><tr>\n", title);
int count = slCount(selects);
struct categorySelect **categArray;
AllocArray(categArray, count);
int i=0;
struct categorySelect *sel;
for (i=0, sel = selects; sel != NULL; sel = sel->next, i++)
    categArray[i] = sel;
int col=0;
int row=0;
int tableColumns=1;
for (i=0; i<count; i++)
    {
    int j = row + col*(count/tableColumns+1);
    if (j>=count)
        {
        printf("</tr><tr>");
        row++;
        col = 0;
        }
    j = row + col*(count/tableColumns+1);
    if (!isPopup)
        {
        printf("<td><input type=checkbox name=\"%s\" value=\"%s\" %s></td>" "<td>%s</td>\n",
                name, categArray[j]->name, categArray[j]->checked ? "checked" : "", 
                categArray[j]->label);
        }
    col++;
    }
if ((i % tableColumns) != 0)
    while ((i++ % tableColumns) != 0)
        printf("<td></td>");
printf("</tr><tr><td></td></tr>\n");
}

static void makeCategoryCheckboxes(char *name, struct barChartCategory *categs, 
                                        struct slName *checked)
{
puts("<style>\n");
puts(".bcColorPatch { padding: 0 10px; }\n");
puts("</style>\n");

struct hash *checkHash = hashNew(0);
struct slName *sel;
for (sel = checked; sel != NULL; sel = sel->next)
    hashAdd(checkHash, sel->name, sel->name);
puts("<table borderwidth=0 cellpadding=1 cellspacing=4><tr>");
struct categorySelect *catSel;
struct barChartCategory *categ;
struct categorySelect *all = NULL;
for (categ = categs; categ != NULL; categ = categ->next)
    {
    AllocVar(catSel);
    catSel->name = categ->name;
    catSel->label = makeCategoryLabel(categ);
    if (hashNumEntries(checkHash) == 0)
        catSel->checked = TRUE;
    else
        catSel->checked = (hashLookup(checkHash, categ->name) != NULL);
    slAddHead(&all, catSel);
    }
slReverse(&all);
makeGroupCheckboxes(name, NULL, all);
puts("</tr></table>");
char buf[512];
safef(buf, sizeof(buf), "%s%s", cgiMultListShadowPrefix(), name);
cgiMakeHiddenVar(buf, "0");
}

/* Convenience functions for hgTrackUi */

void barChartUiLogTransform(struct cart *cart, char *track, struct trackDb *tdb)
/* Checkbox to select log-transformed RPKM values */
/* NOTE: this code from gtexUi.c.  Consider sharing. */
{
char cartVar[1024];
puts("<b>Log10(x+1) transform:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_LOG_TRANSFORM);
boolean isLogTransform = cartCgiUsualBoolean(cart, cartVar, BAR_CHART_LOG_TRANSFORM_DEFAULT);
cgiMakeCheckBoxWithId(cartVar, isLogTransform, cartVar);
jsOnEventByIdF("change", cartVar, "barChartUiTransformChanged('%s');", track);
}

double barChartUiMaxMedianScore(struct trackDb *tdb)
/* Max median score, for scaling */
{
char *setting = trackDbSettingClosestToHome(tdb, BAR_CHART_MAX_LIMIT);
if (setting != NULL)
    {
    double max = sqlDouble(setting);
    if (max > 0.0)
        return max;
    }
return BAR_CHART_MAX_LIMIT_DEFAULT;
}

void barChartUiViewLimits(struct cart *cart, char *track, struct trackDb *tdb)
/* Set viewing limits if log transform not checked */
/* NOTE: this code from gtexUi.c.  Consider sharing. */
{
char cartVar[1024];
char buf[512];
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_LOG_TRANSFORM);
boolean isLogTransform = cartCgiUsualBoolean(cart, cartVar, BAR_CHART_LOG_TRANSFORM_DEFAULT);
safef(buf, sizeof buf, "%sViewLimitsMaxLabel %s", track, isLogTransform ? "disabled" : "");
printf("<span class='%s'><b>View limits maximum:</b></span>\n", buf);
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_MAX_VIEW_LIMIT);
int viewMax = cartCgiUsualInt(cart, cartVar, BAR_CHART_MAX_VIEW_LIMIT_DEFAULT);
cgiMakeIntVarWithExtra(cartVar, viewMax, 4, isLogTransform ? "disabled" : "");
char *unit = trackDbSettingClosestToHomeOrDefault(tdb, BAR_CHART_UNIT, "");
printf("<span class='%s'> %s (range 0-%d)</span>\n", buf, unit, 
                                round(barChartUiMaxMedianScore(tdb)));
}

struct barChartCategory *barChartUiGetCategories(char *database, struct trackDb *tdb)
/* Get category colors and descriptions.  Use barChartColors setting if present.
   If not, if there is a barChartBars setting, assign rainbow colors.
 * O/w look for a table naed track+Category, and use labels and colors there 
 */
{
struct barChartCategory *categs = NULL;
char *words[BAR_CHART_MAX_CATEGORIES];
char *colorWords[BAR_CHART_MAX_CATEGORIES];
char *labels = trackDbSettingClosestToHome(tdb, BAR_CHART_CATEGORY_LABELS);
char *colors = trackDbSettingClosestToHome(tdb, BAR_CHART_CATEGORY_COLORS);
struct barChartCategory *categ = NULL;

if (labels == NULL)
    {
    errAbort("barChart track %s missing required %s setting\n", tdb->track, BAR_CHART_CATEGORY_LABELS);
    }
else
    {
    int count = chopLine(cloneString(labels), words);
    struct rgbColor *rainbow = getRainbow(&saturatedRainbowAtPos, count);
    if (colors != NULL)
        {
        int colorCount = chopLine(cloneString(colors), colorWords);
        if (colorCount != count)
            warn("barChart track %s mismatch between label (%d)  and color (%d) settings", 
                    tdb->track, count, colorCount);
        }
    int i;
    char buf[6];
    for (i=0; i<count; i++)
        {
        AllocVar(categ);
        categ->id = i;
        safef(buf, sizeof buf, "%d", i);
        categ->name = cloneString(buf);
        categ->label = words[i];
        if (colors)
            {
            unsigned rgb;
            char *color = colorWords[i];
            if (htmlColorForCode(color, &rgb))
                {
                categ->color = rgb;
                }
            else if (htmlColorForName(color, &rgb))
                {
                categ->color = rgb;
                }
            else
                warn("barChart track %s unknown color %s. Must be one of %s\n",
                        tdb->track, color, slNameListToString(htmlColorNames(),','));
            }
        else
            {
            categ->color = ((rainbow[i].r & 0xff)<<16) + 
                        ((rainbow[i].g & 0xff)<<8) + 
                        ((rainbow[i].b & 0xff));
            }
        slAddHead(&categs, categ);
        }
    slReverse(&categs);
    }
return categs;
}

struct barChartCategory *barChartUiGetCategoryById(int id, char *database, 
                                                        struct trackDb *tdb)
/* Get category info by id */
{
struct barChartCategory *categ;
struct barChartCategory *categs = barChartUiGetCategories(database, tdb);
for (categ = categs; categ != NULL; categ = categ->next)
    if (categ->id == id)
        return categ;
return NULL;
}

char *barChartUiGetCategoryLabelById(int id, char *database, struct trackDb *tdb)
/* Get label for a category id */
{
struct barChartCategory *categ = barChartUiGetCategoryById(id, database, tdb);
if (categ == NULL)
    return "Unknown";
return categ->label;
}

void barChartCfgUi(char *database, struct cart *cart, struct trackDb *tdb, char *track, 
                        char *title, boolean boxed)
/* Bar chart track type */
{
if (cartVarExists(cart, "ajax"))
    isPopup = TRUE;
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
if (startsWith("big", tdb->type))
    labelCfgUi(database, cart, tdb, track);
printf("\n<table id=barChartControls style='font-size:%d%%' %s>\n<tr><td>", 
        isPopup ? 75 : 100, boxed ?" width='100%'":"");

char cartVar[1024];

/* Data transform. When selected, the next control (view limits max) is disabled */

puts("<div>");
barChartUiLogTransform(cart, track, tdb);

/* Viewing limits max.  This control is disabled if log transform is selected */
// construct class so JS can toggle
puts("&nbsp;&nbsp;");
barChartUiViewLimits(cart, track, tdb);
puts("</div>");

/* Category filter */
printf("<br>");
char *categoryLabel =  trackDbSettingClosestToHomeOrDefault(tdb, 
                    BAR_CHART_CATEGORY_LABEL, BAR_CHART_CATEGORY_LABEL_DEFAULT);
char *db = cartString(cart, "db");
struct barChartCategory *categs = barChartUiGetCategories(db, tdb);
printf("<div><b>%s:</b>\n", categoryLabel);
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_CATEGORY_SELECT);
if (isPopup)
    {
    printf("<a href='%s?g=%s'><button type='button'>Change</button><a>", 
                hTrackUiForTrack(track), track);
    }
else
    {
    jsMakeCheckboxGroupSetClearButton(cartVar, TRUE);
    puts("&nbsp;");
    jsMakeCheckboxGroupSetClearButton(cartVar, FALSE);
    }
printf("</div>");
struct slName *selectedValues = NULL;
if (cartListVarExistsAnyLevel(cart, tdb, FALSE, BAR_CHART_CATEGORY_SELECT))
    selectedValues = cartOptionalSlNameListClosestToHome(cart, tdb, FALSE, BAR_CHART_CATEGORY_SELECT);
makeCategoryCheckboxes(cartVar, categs, selectedValues);

puts("\n</table>\n");
cfgEndBox(boxed);
}

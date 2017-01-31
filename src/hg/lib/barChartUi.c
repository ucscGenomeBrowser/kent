/* Bar chart track controls */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "trackDb.h"
#include "jsHelper.h"
#include "hCommon.h"
#include "barChartCategory.h"
#include "barChartUi.h"

/* Restrict features on right-click (popup) version */
static boolean isPopup = FALSE;

/* Convenience functions for category filter controls */


static char *makeCategoryLabel(struct barChartCategory *categ)
{
/* Display category color and label */
char buf[256];
safef(buf, sizeof(buf), "<td class='bcColorPatch' bgcolor=%X></td>"
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
int tableColumns=3;
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
               name, categArray[j]->name, categArray[j]->checked ? "checked" : "", categArray[j]->label);
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
// TODO: use style sheet!
puts("<style>\n");
puts(".bcColorPatch { padding: 0 10px; }\n");
puts("</style>\n");

struct hash *checkHash = hashNew(0);
struct slName *sel;
for (sel = checked; sel != NULL; sel = sel->next)
    hashAdd(checkHash, sel->name, sel->name);
puts("<table borderwidth=0 cellspacing=4><tr>");
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
{
char cartVar[1024];
char buf[512];
puts("<b>Log10 transform:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_LOG_TRANSFORM);
boolean isLogTransform = cartCgiUsualBoolean(cart, cartVar, BAR_CHART_LOG_TRANSFORM_DEFAULT);
safef(buf, sizeof buf, "onchange='barChartUiTransformChanged(\"%s\")'", track);
cgiMakeCheckBoxJS(cartVar, isLogTransform, buf);
}

double barChartUiMaxMedianScore()
/* Max median score, for scaling */
{
//TODO: get from trackDb
return 10000;
}

void barChartUiViewLimits(struct cart *cart, char *track, struct trackDb *tdb)
/* Set viewing limits if log transform not checked */
{
char cartVar[1024];
char buf[512];
boolean isLogTransform = cartCgiUsualBoolean(cart, cartVar, BAR_CHART_LOG_TRANSFORM_DEFAULT);
safef(buf, sizeof buf, "%sViewLimitsMaxLabel %s", track, isLogTransform ? "disabled" : "");
printf("<span class='%s'><b>View limits maximum:</b></span>\n", buf);
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_MAX_LIMIT);
int viewMax = cartCgiUsualInt(cart, cartVar, BAR_CHART_MAX_LIMIT_DEFAULT);
cgiMakeIntVarWithExtra(cartVar, viewMax, 4, isLogTransform ? "disabled" : "");
printf("<span class='%s'> (range 0-%d)</span>\n", buf, round(barChartUiMaxMedianScore()));
}

void barChartCfgUi(char *database, struct cart *cart, struct trackDb *tdb, char *track, 
                        char *title, boolean boxed)
/* Bar chart track type */
{

jsIncludeFile("barChart.js", NULL);
if (cartVarExists(cart, "ajax"))
    isPopup = TRUE;
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
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

/* Color scheme */
#ifdef COLOR_SCHEME
printf("<p><b>Category colors:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_COLORS);
selected = cartCgiUsualString(cart, cartVar, BAR_CHART_COLORS_DEFAULT); 
boolean isUserColors = sameString(selected, BAR_CHART_COLORS_USER);
cgiMakeRadioButton(cartVar, BAR_CHART_COLORS_USER, isUserColors);
printf("Defined\n");
cgiMakeRadioButton(cartVar, BAR_CHART_COLORS_RAINBOW, !isUserColors);
printf("Rainbow\n");
printf("</p>");
#endif

/* Category filter */
printf("<br>");
// TODO: 
#define BAR_CHART_CATEGORY_LABEL        "categoryLabel"
#define BAR_CHART_CATEGORY_LABEL_DEFAULT        "Categories"
char *categoryLabel =  trackDbSettingClosestToHomeOrDefault(tdb, 
                                BAR_CHART_CATEGORY_LABEL, BAR_CHART_CATEGORY_LABEL_DEFAULT);
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
char *db = cartString(cart, "db");
struct barChartCategory *categs = barChartGetCategories(db, track);
struct slName *selectedValues = NULL;
if (cartListVarExistsAnyLevel(cart, tdb, FALSE, BAR_CHART_CATEGORY_SELECT))
    selectedValues = cartOptionalSlNameListClosestToHome(cart, tdb, FALSE, BAR_CHART_CATEGORY_SELECT);
makeCategoryCheckboxes(cartVar, categs, selectedValues);

puts("\n</table>\n");
cfgEndBox(boxed);
}

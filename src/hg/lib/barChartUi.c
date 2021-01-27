/* Bar chart track controls */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "net.h"
#include "errCatch.h"
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

boolean barChartIsLogTransformed(struct cart *cart, char *track, struct trackDb *tdb)
/* Return TRUE if bar chart needs to be log transformed */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_LOG_TRANSFORM);
boolean isLog = TRUE;
char *trans = trackDbSetting(tdb, "transformFunc");
if (trans != NULL)
    isLog = sameWord(trans, "LOG");
return cartCgiUsualBoolean(cart, cartVar, isLog);
}

void barChartUiLogTransform(struct cart *cart, char *track, struct trackDb *tdb)
/* Checkbox to select log-transformed RPKM values */
/* NOTE: this code from gtexUi.c.  Consider sharing. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_LOG_TRANSFORM);
puts("<b>Log10(x+1) transform:</b>\n");
boolean isLogTransform = barChartIsLogTransformed(cart, track, tdb);
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

double barChartCurViewMax(struct cart *cart, char *trackName, struct trackDb *tdb)
/* Look up max value to scale for this bar chart - consults both cart and trackDb defaults. */
{
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", trackName, BAR_CHART_MAX_VIEW_LIMIT);
char *limitString = trackDbSettingOrDefault(tdb, BAR_CHART_LIMIT, "50");
double limit = atof(limitString);
if (limit <= 0) limit = 0.001;
return cartCgiUsualDouble(cart, cartVar, limit);
}

void barChartUiViewLimits(struct cart *cart, char *track, struct trackDb *tdb)
/* Set viewing limits if log transform not checked */
/* NOTE: this code from gtexUi.c.  Consider sharing. */
{
char buf[512];
boolean isLogTransform = barChartIsLogTransformed(cart, track, tdb);
safef(buf, sizeof buf, "%sViewLimitsMaxLabel %s", track, isLogTransform ? "disabled" : "");
printf("<span class='%s'><b>View limits maximum:</b></span>\n", buf);
double viewMax = barChartCurViewMax(cart, track, tdb);
char cartVar[1024];
safef(cartVar, sizeof(cartVar), "%s.%s", track, BAR_CHART_MAX_VIEW_LIMIT);
cgiMakeDoubleVarWithExtra(cartVar, viewMax, 4, isLogTransform ? "disabled" : "");
char *unit = trackDbSettingClosestToHomeOrDefault(tdb, BAR_CHART_UNIT, "");
printf("<span class='%s'> %s (range 0-%d)</span>\n", buf, unit, 
                                round(barChartUiMaxMedianScore(tdb)));
}

// TODO: libify
static boolean isUrl(char *url)
{
return startsWith("http://", url)
   || startsWith("https://", url)
   || startsWith("ftp://", url);
}

static void getCategsFromSettings(char *track, char *labelSetting, char *colorSetting, 
                                        struct slName **labels, struct slName **colors)
/* Get category labels and optionally colors, from track settings */
{
if (!labels || !colors)
    return;
if (isEmpty(labelSetting))
    {
    errAbort("barChart track %s missing required setting: %s or %s\n",
                    track, BAR_CHART_CATEGORY_LABELS, BAR_CHART_CATEGORY_URL);
    }
char *words[BAR_CHART_MAX_CATEGORIES];
int labelCount = chopLine(cloneString(labelSetting), words);
*labels = slNameListFromStringArray(words, labelCount);
if (isNotEmpty(colorSetting))
    {
    int colorCount = chopLine(cloneString(colorSetting), words);
    if (colorCount != labelCount)
        errAbort("barChart track %s settings mismatch: %s (%d) and  %s (%d)\n",
            track, BAR_CHART_CATEGORY_LABELS, labelCount, BAR_CHART_CATEGORY_COLORS, colorCount);
    *colors = slNameListFromStringArray(words, labelCount);
    }
}

static void getCategsFromFile(char *track, char *categUrl,
                                        struct slName **labels, struct slName **colors)
/* Get category labels and optionally colors, from category file.
 * This is tab-sep file, column 1 is category label, optional column 2 is a color spec */
{
if (!labels || !colors) return;
struct lineFile *lf = NULL;

// protect against network error
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    if (isUrl(categUrl))
        lf = netLineFileOpen(categUrl);
    else
        lf = lineFileMayOpen(categUrl, TRUE);
    }
errCatchEnd(errCatch); 
if (errCatch->gotError)
    {
    if (isNotEmpty(errCatch->message->string))
        errAbort("unable to open %s track file %s: %s", 
                    track, categUrl, errCatch->message->string);
    }
errCatchFree(&errCatch);
char *line = NULL;
int cols = 0;
while (lineFileNextReal(lf, &line))
    {
    char *words[2];
    int wordCount = chopTabs(line, words);
    if (cols)
        {
        if (wordCount != cols)
            errAbort("barChart track %s category file %s expecting %d words, got %d",
                        track, categUrl, cols, wordCount);
        }
    else
        {
        cols = wordCount;
        }
    slAddHead(&labels, slNameNew(words[0]));
    if (wordCount == 2)
        slAddHead(&colors, slNameNew(words[1]));
    }
slReverse(&labels);
slReverse(&colors);
}

static struct barChartCategory *createCategs(char *track, 
                                                struct slName *labels, struct slName *colors)
/* Populate category structs from label and color lists.  Assign rainbow if no color list */
{
struct barChartCategory *categs = NULL, *categ = NULL;
int count = slCount(labels);
struct rgbColor *rainbow = NULL;
if (!colors)
    {
    rainbow = getRainbow(&saturatedRainbowAtPos, count);
    }
int i;
char buf[6];
for (i=0 ; i<count && labels; i++)
    {
    AllocVar(categ);
    categ->id = i;
    safef(buf, sizeof buf, "%d", i);
    categ->name = cloneString(buf);
    categ->label = labels->name;
    if (!colors)
        {
        // rainbow
        categ->color = ((rainbow[i].r & 0xff)<<16) + 
                    ((rainbow[i].g & 0xff)<<8) + 
                    ((rainbow[i].b & 0xff));
        }
    else
        {
        // colors from user
        unsigned rgb = 0;
        char *color = colors->name;
        if (!htmlColorForCode(color, &rgb))
            {
            if (!htmlColorForName(color, &rgb))
                {
                /* try r,g,b */
                if (index(color, ','))
                    {
                    unsigned char r, g, b;
                    parseColor(color, &r, &g, &b);
                    htmlColorFromRGB(&rgb, r, g, b);
                    }
                else
                    {
                    warn("barChart track %s unknown color %s. Must r,g,b or #ffffff or one of %s\n",
                            track, color, slNameListToString(htmlColorNames(),','));
                    }
                }
            }
        categ->color = rgb;
        }
    slAddHead(&categs, categ);
    labels = labels->next;
    if (colors)
        colors = colors->next;
    }
slReverse(&categs);
return categs;
}

struct barChartCategory *barChartUiGetCategories(char *database, struct trackDb *tdb)
/* Get category colors and descriptive labels.
   Use labels in tab-sep file specified by barChartCategoryUrl setting, o/w in barChartBars setting.
   If colors are not specified via barChartColors setting or second column in category file,
   assign rainbow colors.  Colors are specified as #fffff or r,g,b  or html color name) */
{
struct slName *labels = NULL, *colors = NULL;
char *categUrl = trackDbSetting(tdb, BAR_CHART_CATEGORY_URL);
if (isNotEmpty(categUrl))
    getCategsFromFile(tdb->track, categUrl, &labels, &colors);
else
    {
    char *labelSetting = trackDbSetting(tdb, BAR_CHART_CATEGORY_LABELS);
    char *colorSetting = trackDbSetting(tdb, BAR_CHART_CATEGORY_COLORS);
    getCategsFromSettings(tdb->track, labelSetting, colorSetting, &labels, &colors);
    }
return createCategs(tdb->track, labels, colors);
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

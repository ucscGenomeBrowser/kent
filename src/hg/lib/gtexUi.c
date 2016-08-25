/* GTEx (Genotype Tissue Expression) track controls */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "trackDb.h"
#include "jsHelper.h"
#include "gtexTissue.h"
#include "gtexInfo.h"
#include "gtexUi.h"

#define SYSTEM_BRAIN            "Brain"
#define SYSTEM_REPRODUCTIVE     "Reproductive"
#define SYSTEM_GASTRO           "Digestive"

#define SYSTEM_ENDOCRINE        "Endocrine"
#define SYSTEM_CARDIO           "Cardiovascular"
#define SYSTEM_OTHER            "Other"


/* Restrict features on right-click (popup) version */
static boolean isPopup = FALSE;

/* Convenience functions for tissue filter controls */

static char *makeTissueColorPatch(struct gtexTissue *tis)
/* Display a box colored by defined tissue color */
{
char buf[256];
safef(buf, sizeof(buf), "<td bgcolor=#%06X></td>", tis->color);
return(cloneString(buf));
}

static char *makeTissueLabel(struct gtexTissue *tis)
{
/* Display tissue color and label */
char buf[256];
safef(buf, sizeof(buf), "<td bgcolor=%X></td>"
                        "<td>&nbsp;%s</td>", 
                                tis->color, tis->description);
return(cloneString(buf));
}

static char *getSystem(struct gtexTissue *tis)
/* Rough categorization of tissues for filter presentation */
{
if (startsWith("brain", tis->name))
    return(SYSTEM_BRAIN);
else if (sameString(tis->name, "uterus") || sameString(tis->name, "testis") ||
        sameString(tis->name, "vagina") || sameString(tis->name, "prostate") ||
        sameString(tis->name, "ovary") ||
        sameString(tis->name, "breastMamTissue") || sameString(tis->name, "ectocervix") ||
        sameString(tis->name, "endocervix") || sameString(tis->name, "fallopianTube"))
    return(SYSTEM_REPRODUCTIVE);
else if (startsWith("esophagus", tis->name) || startsWith("colon", tis->name) ||
        sameString(tis->name, "stomach") || sameString("smallIntestine", tis->name) ||
        sameString("pancreas", tis->name) || sameString("liver", tis->name))
    return(SYSTEM_GASTRO);
else if (sameString("adrenalGland", tis->name) || sameString("pituitary", tis->name) ||
        sameString("thyroid", tis->name))
    return(SYSTEM_ENDOCRINE);
else if (startsWith("heart", tis->name) || startsWith("artery", tis->name))
    return(SYSTEM_CARDIO);
else
    return(SYSTEM_OTHER);
}

struct tissueSelect
    {
    struct tissueSelect *next;
    char *name;
    char *label;
    boolean checked;
    };

static void makeGroupCheckboxes(char *name, char *title, struct tissueSelect *tisSelects)
{
if (title != NULL)
    printf("<tr><td colspan=10><i><b>%s</b></i></td></tr><tr>\n", title);
int count = slCount(tisSelects);
struct tissueSelect **tisArray;
AllocArray(tisArray, count);
int i=0;
struct tissueSelect *tsel;
for (i=0, tsel = tisSelects; tsel != NULL; tsel = tsel->next, i++)
    tisArray[i] = tsel;
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
               name, tisArray[j]->name, tisArray[j]->checked ? "checked" : "", tisArray[j]->label);
        }
    col++;
    }
if ((i % tableColumns) != 0)
    while ((i++ % tableColumns) != 0)
        printf("<td></td>");
printf("</tr><tr><td></td></tr>\n");
}

static void initTissueTableStyle()
/* Reduce font in tissue table so more rows are visible.
 * Specify some colors.*/
{
puts("<style>\n"
        "#tissueTable th, #tissueTable td {font-size: 75%; margin: 0px; border: 0px; padding: 0px}\n"
        "#tissueTable input {margin: 0px .5ex; width: 8px; height: 8px}\n"
        "#gtexGeneControls div {margin-bottom: .3em;}\n"
        ".notSortable {font-color: black;}\n"
    "</style>\n");
}

static void makeTableTissueCheckboxes(char *name, struct gtexTissue *tissues, 
                        struct slName *checked, struct cart *cart, char *track, char *version)
{
initTissueTableStyle();
char *onClick = "";
// Sortable table can't be displayed when UI is activated from right-click (popup mode)
if (!isPopup)
    {
    jsIncludeFile("hui.js", NULL);
    onClick = "'tableSortAtButtonPress(this);";
    }
struct hash *checkHash = hashNew(0);
struct slName *sel;
for (sel = checked; sel != NULL; sel = sel->next)
    hashAdd(checkHash, sel->name, sel->name);
//puts("<table borderwidth=0><tr>");
puts("\n<table id='tissueTable' cellspacing='1' cellpadding='0' border='0' class='sortable'>");

/* table header */
char orderVar[256];
safef(orderVar, sizeof(orderVar), "%s.sortOrder", name);
char *sortOrder = cartCgiUsualString(cart, orderVar, "tissue=+ samples=+ organ=+ system=+");
puts("\n<thead class='sortable'>");
puts("\n<tr class='sortable'>");
char *sortableClass = isPopup ? "notSortable" : "sortable";
printf("\n<th>&nbsp;<input type=hidden name='%s' class='sortOrder' value='%s'></th>\n",
        orderVar, sortOrder);
puts("<th>&nbsp;&nbsp;&nbsp;&nbsp;</th>");
printf("<th id='tissue' class='%s sort1' %s align='left'>&nbsp;Tissue</th>", 
               sortableClass, onClick);
printf("<th id='samples' abbr='use' class='%s sort2' %s align='left'>&nbsp;Samples</th>", 
               sortableClass, onClick);
printf("<th id='organ' class='%s sort3' %s align='left'>&nbsp;Organ</th>", 
               sortableClass, onClick);
printf("<th id='system' class='%s sort4' %s align='left'>&nbsp;System</th>", 
               sortableClass, onClick);
puts("\n</tr>");
puts("</thead>");

/* table body */
printf("<tbody class='sortable noAltColors initBySortOrder'>");
struct hash *tscHash = gtexGetTissueSampleCount(version);
struct gtexTissue *tis;
boolean isChecked = FALSE;
for (tis = tissues; tis != NULL; tis = tis->next)
    {
    puts("\n<tr valign='top'>");

    // checkbox
    if (hashNumEntries(checkHash) == 0)
        isChecked = TRUE;
    else
        isChecked = (hashLookup(checkHash, tis->name) != NULL);
    printf("<td><input type=checkbox name=\"%s\" value=\"%s\" %s %s></td>",
           name, tis->name, isChecked ? "checked" : "", isPopup ? "disabled" : "");
    // color patch
    printf("\n%s", makeTissueColorPatch(tis));
    // tissue name
    printf("\n<td>&nbsp;%s</td>", tis->description);
    // sample count
    int samples = hashIntValDefault(tscHash, tis->name, 0);
    printf("\n<td abbr='%05d' style='text-align: right; padding-right: 8px'>&nbsp;%d</td>", samples, samples);
    // organ
    printf("\n<td style='padding-right: 8px'>&nbsp;%s</td>", tis->organ);
    // system
    printf("\n<td>&nbsp;%s</td>", getSystem(tis));

    puts("\n</tr>");
    }
puts("</tbody>");
puts("</table>");
char buf[512];
safef(buf, sizeof(buf), "%s%s", cgiMultListShadowPrefix(), name);
cgiMakeHiddenVar(buf, "0");
}

static void makeGroupedTissueCheckboxes(char *name, struct gtexTissue *tissues, struct slName *checked)
{
struct hash *checkHash = hashNew(0);
struct slName *sel;
for (sel = checked; sel != NULL; sel = sel->next)
    hashAdd(checkHash, sel->name, sel->name);
puts("<table borderwidth=0><tr>");
struct tissueSelect *brainTissues = NULL;
struct tissueSelect *digestiveTissues = NULL;
struct tissueSelect *reproductiveTissues = NULL;
struct tissueSelect *otherTissues = NULL;
struct tissueSelect *tsel;
struct gtexTissue *tis;
for (tis = tissues; tis != NULL; tis = tis->next)
    {
    AllocVar(tsel);
    tsel->name = tis->name;
    tsel->label = makeTissueLabel(tis);
    if (hashNumEntries(checkHash) == 0)
        tsel->checked = TRUE;
    else
        tsel->checked = (hashLookup(checkHash, tis->name) != NULL);
    char *system = getSystem(tis);
    if (sameString(SYSTEM_BRAIN, system))
        slAddHead(&brainTissues, tsel);
    else if (sameString(SYSTEM_REPRODUCTIVE, system))
        slAddHead(&reproductiveTissues, tsel);
    else if (sameString(SYSTEM_GASTRO, system))
        slAddHead(&digestiveTissues, tsel);
    else
        slAddHead(&otherTissues, tsel);
    }
slReverse(&brainTissues);
slReverse(&digestiveTissues);
slReverse(&reproductiveTissues);
slReverse(&otherTissues);
makeGroupCheckboxes(name, "Brain", brainTissues);
makeGroupCheckboxes(name, "Gastrointestinal", digestiveTissues);
makeGroupCheckboxes(name, "Reproductive", reproductiveTissues);
makeGroupCheckboxes(name, "Other", otherTissues);
puts("</tr></table>");
char buf[512];
safef(buf, sizeof(buf), "%s%s", cgiMultListShadowPrefix(), name);
cgiMakeHiddenVar(buf, "0");
}

static void makeAllTissueCheckboxes(char *name, struct gtexTissue *tissues, 
                                        struct slName *checked)
{
struct hash *checkHash = hashNew(0);
struct slName *sel;
for (sel = checked; sel != NULL; sel = sel->next)
    hashAdd(checkHash, sel->name, sel->name);
puts("<table borderwidth=0><tr>");
struct tissueSelect *tsel;
struct gtexTissue *tis;
struct tissueSelect *allTissues = NULL;
for (tis = tissues; tis != NULL; tis = tis->next)
    {
    AllocVar(tsel);
    tsel->name = tis->name;
    tsel->label = makeTissueLabel(tis);
    if (hashNumEntries(checkHash) == 0)
        tsel->checked = TRUE;
    else
        tsel->checked = (hashLookup(checkHash, tis->name) != NULL);
    slAddHead(&allTissues, tsel);
    }
slReverse(&allTissues);
makeGroupCheckboxes(name, NULL, allTissues);
puts("</tr></table>");
char buf[512];
safef(buf, sizeof(buf), "%s%s", cgiMultListShadowPrefix(), name);
cgiMakeHiddenVar(buf, "0");
}

void gtexPortalLink(char *geneId)
/* print URL to GTEX portal gene expression page using Ensembl Gene Id*/
{
printf("<a target='_blank' href='http://www.gtexportal.org/home/gene/%s'>"
        "View at GTEx portal</a>\n", geneId);
}

void gtexGeneUi(struct cart *cart, struct trackDb *tdb, char *track, char *title, boolean boxed)
/* GTEx (Genotype Tissue Expression) per gene data */
{
if (cartVarExists(cart, "ajax"))
    isPopup = TRUE;

boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
printf("\n<table id=gtexGeneControls style='font-size:%d%%' %s>\n<tr><td>", 
        isPopup ? 75 : 100, boxed ?" width='100%'":"");

char cartVar[1024];
char buf[512];

/* Gene labels  */
printf("<div>");
char *geneLabel = cartUsualStringClosestToHome(cart, tdb, isNameAtParentLevel(tdb, track), 
                        GTEX_LABEL, GTEX_LABEL_DEFAULT);
printf("<b>Label:</b> ");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_LABEL);
cgiMakeRadioButton(cartVar, GTEX_LABEL_SYMBOL , sameString(GTEX_LABEL_SYMBOL, geneLabel));
printf("%s ", "gene symbol");
cgiMakeRadioButton(cartVar, GTEX_LABEL_ACCESSION, sameString(GTEX_LABEL_ACCESSION, geneLabel));
printf("%s ", "accession");
cgiMakeRadioButton(cartVar, GTEX_LABEL_BOTH, sameString(GTEX_LABEL_BOTH, geneLabel));
printf("%s ", "both");
printf("</div>\n");

/* Filter on coding genes */
printf("<div>");
printf("<b>Limit to protein coding genes:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_CODING_GENE_FILTER);
boolean isCodingOnly = cartCgiUsualBoolean(cart, cartVar, GTEX_CODING_GENE_FILTER_DEFAULT);
cgiMakeCheckBox(cartVar, isCodingOnly);

/* Show exons in gene model */
printf("&nbsp;&nbsp;<b>Show GTEx gene model</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_SHOW_EXONS);
boolean showExons = cartCgiUsualBoolean(cart, cartVar, GTEX_SHOW_EXONS_DEFAULT);
cgiMakeCheckBox(cartVar, showExons);
printf("</div>");

/* Data transform. When selected, the next control (view limits max) is disabled */
printf("<div><b>Log10 transform:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_LOG_TRANSFORM);
boolean isLogTransform = cartCgiUsualBoolean(cart, cartVar, GTEX_LOG_TRANSFORM_DEFAULT);
safef(buf, sizeof buf, "onchange='gtexTransformChanged(\"%s\")'", track);
cgiMakeCheckBoxJS(cartVar, isLogTransform, buf);

/* Viewing limits max.  This control is disabled if log transform is selected */
// construct class so JS can toggle
safef(buf, sizeof buf, "%sViewLimitsMaxLabel %s", track, isLogTransform ? "disabled" : "");
printf("&nbsp;&nbsp;<span class='%s'><b>View limits maximum:</b></span>\n", buf);
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_MAX_LIMIT);
int viewMax = cartCgiUsualInt(cart, cartVar, GTEX_MAX_LIMIT_DEFAULT);
cgiMakeIntVarWithExtra(cartVar, viewMax, 4, isLogTransform ? "disabled" : "");
char *version = gtexVersion(tdb->table);
printf("<span class='%s'>  RPKM (range 0-%d)</span>\n", buf, round(gtexMaxMedianScore(version)));
printf("</div>");

#ifdef COMPARISON
/* Sample selection */
printf("<div><b>Samples:</b>&nbsp;");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_SAMPLES);
char *selected = cartCgiUsualString(cart, cartVar, GTEX_SAMPLES_DEFAULT); 
boolean isAllSamples = sameString(selected, GTEX_SAMPLES_ALL);
safef(buf, sizeof buf, "onchange='gtexSamplesChanged(\"%s\")'", track);
char *command = buf;
cgiMakeOnClickRadioButton(cartVar, GTEX_SAMPLES_ALL, isAllSamples, command);
printf("All\n");
cgiMakeOnClickRadioButton(cartVar, GTEX_SAMPLES_COMPARE_SEX, !isAllSamples, command);
printf("Compare by gender\n");
printf("</div>");

/* Comparison type. Disabled if All samples selected. */
safef(buf, sizeof buf, "%sComparisonLabel %s", track, isAllSamples ? "disabled" : "");
printf("<div><b><span class='%s'>Comparison display:</b></span>", buf);
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_COMPARISON_DISPLAY);
selected = cartCgiUsualString(cart, cartVar, GTEX_COMPARISON_DEFAULT); 
boolean isMirror = sameString(selected, GTEX_COMPARISON_MIRROR);

cgiMakeRadioButton(cartVar, GTEX_COMPARISON_DIFF, !isMirror);
printf("<span class='%s'>Difference graph</span>", buf);
cgiMakeRadioButton(cartVar, GTEX_COMPARISON_MIRROR, isMirror);
printf("<span class='%s'>Two graphs</span>\n", buf);
printf("</div>");
#endif

/* Color scheme */
// We don't need the rainbow color scheme, but may want another (e.g. different
// colors for brain tissues), so leaving code in for now.
#ifdef COLOR_SCHEME
printf("<p><b>Tissue colors:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_COLORS);
selected = cartCgiUsualString(cart, cartVar, GTEX_COLORS_DEFAULT); 
boolean isGtexColors = sameString(selected, GTEX_COLORS_GTEX);
cgiMakeRadioButton(cartVar, GTEX_COLORS_GTEX, isGtexColors);
printf("GTEx\n");
cgiMakeRadioButton(cartVar, GTEX_COLORS_RAINBOW, !isGtexColors);
printf("Rainbow\n");
printf("</p>");
#endif

/* Tissue filter */
printf("<br>");
printf("<div><b>Tissues:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_TISSUE_SELECT);
if (isPopup)
    {
    printf("<a href='../cgi-bin/hgTrackUi?g=%s'><button type='button'>Change</button><a>", track);
    }
else
    {
    jsMakeCheckboxGroupSetClearButton(cartVar, TRUE);
    puts("&nbsp;");
    jsMakeCheckboxGroupSetClearButton(cartVar, FALSE);
    }
printf("</div>");
struct gtexTissue *tissues = gtexGetTissues(version);
struct slName *selectedValues = NULL;
if (cartListVarExistsAnyLevel(cart, tdb, FALSE, GTEX_TISSUE_SELECT))
    selectedValues = cartOptionalSlNameListClosestToHome(cart, tdb, FALSE, GTEX_TISSUE_SELECT);
char *selectType = cgiUsualString("tis", "table");
if (sameString(selectType, "group"))
    makeGroupedTissueCheckboxes(cartVar, tissues, selectedValues);
else if (sameString(selectType, "table"))
    makeTableTissueCheckboxes(cartVar, tissues, selectedValues, cart, track, version);
else
    makeAllTissueCheckboxes(cartVar, tissues, selectedValues);

puts("\n</table>\n");
cfgEndBox(boxed);
}

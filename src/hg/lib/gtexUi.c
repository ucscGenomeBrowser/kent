/* GTEx (Genotype Tissue Expression) track controls */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "trackDb.h"
#include "jsHelper.h"
#include "gtexTissue.h"
#include "gtexUi.h"

#define SYSTEM_BRAIN            "Brain"
#define SYSTEM_REPRODUCTIVE     "Reproductive"
#define SYSTEM_GASTRO           "Digestive"

#define SYSTEM_ENDOCRINE        "Endocrine"
#define SYSTEM_CARDIO           "Cardiovascular"
#define SYSTEM_OTHER            "Other"


static char *makeTissueColorPatch(struct gtexTissue *tis)
{
char buf[256];
safef(buf, sizeof(buf), "<td style='width:10px; font-size:75%%;' bgcolor=%X></td>", tis->color);
return(cloneString(buf));
}

static char *makeTissueLabel(struct gtexTissue *tis)
{
char buf[256];
safef(buf, sizeof(buf), "<td style='width:10px; font-size:75%%;' bgcolor=%X></td>"
                        "<td style='font-size:75%%'>&nbsp;%s</td>", 
                                tis->color, tis->description);
return(cloneString(buf));
}

static char *getSystem(struct gtexTissue *tis)
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
    printf("<tr><td colspan=10 style='font-size:75%%'><i><b>%s</b></i></td></tr><tr>\n", title);
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
    printf("<td><input type=checkbox name=\"%s\" value=\"%s\" %s></td>" "<td>%s</td>\n",
               name, tisArray[j]->name, tisArray[j]->checked ? "checked" : "", tisArray[j]->label);
    col++;
    }
if ((i % tableColumns) != 0)
    while ((i++ % tableColumns) != 0)
        printf("<td></td>");
printf("</tr><tr><td></td></tr>\n");
}

static void makeAllTissueCheckboxes(char *name, struct gtexTissue *tissues, struct slName *checked)
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

static void makeTableTissueCheckboxes(char *name, struct gtexTissue *tissues, 
                                        struct slName *checked, boolean isPopup)
{
char *onClick = "";
// Sortable table can't be activated when in activated from right-click (popup mode)
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
puts("\n<table cellspacing='2' cellpadding='0' border='0' class='sortable'>");

/* table header */
puts("\n<thead class='sortable'>");
puts("\n<tr ID='tissueTableHeader' class='sortable'>");
printf("\n<th>&nbsp;<input type=hidden name='%s' class='sortOrder' value='%s'></th>\n",
        "gtexGene.sortOrder", "tissue=+ samples=+ organ=+ system=+");
puts("<th>&nbsp;&nbsp;&nbsp;&nbsp;</th>");

printf("<th id='tissue' class='sortable sort1' style='font-size:75%%' %s "
        "align='left' title='Sort on tissue'>&nbsp;Tissue</th>", onClick);

printf("<th id='samples' abbr='use' class='sortable sort2' style='font-size:75%%' %s "
        "align='left' title='Sort on sample count'>&nbsp;Samples</th>", onClick);

printf("<th id='organ' class='sortable sort3' style='font-size:75%%' %s "
        "align='left' title='Sort on organ'>&nbsp;Organ</th>", onClick);

printf("<th id='system' class='sortable sort4' style='font-size:75%%' %s "
        "align='left' title='Sort on system'>&nbsp;System</th>", onClick);
puts("\n</tr>");
puts("</thead>");

/* table body */
puts("<tbody class='sortable noAltColors' style='display: table-row-group;' id='tbodySort'>");
struct hash *tscHash = gtexGetTissueSampleCount();
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
    printf("<td><input type=checkbox name=\"%s\" value=\"%s\" %s></td>",
               name, tis->name, isChecked ? "checked" : "");
    // color patch
    printf("\n%s", makeTissueColorPatch(tis));
    // tissue name
    printf("\n<td style='font-size:75%%'>&nbsp;%s</td>", tis->description);
    // sample count
    int samples = hashIntValDefault(tscHash, tis->name, 0);
    printf("\n<td abbr='%05d' style='font-size:75%%; text-align: right; padding-right: 10px''>&nbsp;%d</td>", samples, samples);
    // organ
    printf("\n<td style='font-size:75%%; padding-right: 10px'>&nbsp;%s</td>", tis->organ);
    // system
    printf("\n<td style='font-size:75%%'>&nbsp;%s</td>", getSystem(tis));
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

void gtexGeneUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* GTEx (Genotype Tissue Expression) per gene data */
{
boolean isPopup = FALSE;
if (cartVarExists(cart, "ajax"))
    isPopup = TRUE;
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
printf("<table%s><tr><td>",boxed?" width='100%'":"");

char cartVar[1024];
char *selected = NULL;

/* Sample selection */
printf("<b>Samples:</b>&nbsp;");
safef(cartVar, sizeof(cartVar), "%s.%s", name, GTEX_SAMPLES);
selected = cartCgiUsualString(cart, cartVar, GTEX_SAMPLES_DEFAULT); 
boolean isAllSamples = sameString(selected, GTEX_SAMPLES_ALL);
cgiMakeRadioButton(cartVar, GTEX_SAMPLES_ALL, isAllSamples);
printf("All\n");
cgiMakeRadioButton(cartVar, GTEX_SAMPLES_COMPARE_SEX, !isAllSamples);
printf("Compare by gender\n");
printf("</p>");

/* Comparison type */
printf("<p><b>Comparison display:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", name, GTEX_COMPARISON_DISPLAY);
selected = cartCgiUsualString(cart, cartVar, GTEX_COMPARISON_DEFAULT); 
boolean isMirror = sameString(selected, GTEX_COMPARISON_MIRROR);
cgiMakeRadioButton(cartVar, GTEX_COMPARISON_DIFF, !isMirror);
printf("Difference graph\n");
cgiMakeRadioButton(cartVar, GTEX_COMPARISON_MIRROR, isMirror);
printf("Two graphs\n");
printf("</p>");

/* Data transform */
printf("<p><b>Log10 transform:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", name, GTEX_LOG_TRANSFORM);
boolean isLogTransform = cartCgiUsualBoolean(cart, cartVar, GTEX_LOG_TRANSFORM_DEFAULT);
cgiMakeCheckBox(cartVar, isLogTransform);

/* Viewing limits max */
printf("&nbsp;&nbsp;<b>View limits maximum:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", name, GTEX_MAX_LIMIT);
// TODO: set max and initial limits from gtexInfo table
int viewMax = cartCgiUsualInt(cart, cartVar, GTEX_MAX_LIMIT_DEFAULT);
cgiMakeIntVar(cartVar, viewMax, 4);
printf(" RPKM (range 10-180000)<br>\n");
printf("</p>");

/* Color scheme */
// Not sure if we still want this option
printf("<p><b>Tissue colors:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", name, GTEX_COLORS);
selected = cartCgiUsualString(cart, cartVar, GTEX_COLORS_DEFAULT); 
boolean isGtexColors = sameString(selected, GTEX_COLORS_GTEX);
cgiMakeRadioButton(cartVar, GTEX_COLORS_GTEX, isGtexColors);
printf("GTEx\n");
cgiMakeRadioButton(cartVar, GTEX_COLORS_RAINBOW, !isGtexColors);
printf("Rainbow\n");
printf("</p>");

/* Tissue filter */
printf("<p><b>Tissue selection:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", name, GTEX_TISSUE_SELECT);
jsMakeCheckboxGroupSetClearButton(cartVar, TRUE);
puts("&nbsp;");
jsMakeCheckboxGroupSetClearButton(cartVar, FALSE);
struct gtexTissue *tissues = gtexGetTissues();
struct slName *selectedValues = NULL;
if (cartListVarExistsAnyLevel(cart, tdb, FALSE, GTEX_TISSUE_SELECT))
    selectedValues = cartOptionalSlNameListClosestToHome(cart, tdb, FALSE, GTEX_TISSUE_SELECT);
char *selectType = cgiUsualString("tis", "table");
if (sameString(selectType, "group"))
    makeGroupedTissueCheckboxes(cartVar, tissues, selectedValues);
else if (sameString(selectType, "table"))
    makeTableTissueCheckboxes(cartVar, tissues, selectedValues, isPopup);
else
    makeAllTissueCheckboxes(cartVar, tissues, selectedValues);

cfgEndBox(boxed);
}

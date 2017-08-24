/* barChart track UI */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef BAR_CHARTUI_H
#define BAR_CHARTUI_H

/* Color scheme */
#define BAR_CHART_COLORS                     "colorScheme"
#define BAR_CHART_COLORS_RAINBOW             "rainbow"

/* Color scheme from user */
#define BAR_CHART_COLORS_USER                "user"
#define BAR_CHART_COLORS_DEFAULT             BAR_CHART_COLORS_USER

/* Data transform */
    // WARNING: this also appears in JS
#define BAR_CHART_LOG_TRANSFORM              "logTransform"
#define BAR_CHART_LOG_TRANSFORM_DEFAULT      TRUE

/* Data range and viewing limits */
#define BAR_CHART_MAX_LIMIT                  "maxLimit"
#define BAR_CHART_MAX_LIMIT_DEFAULT          10000
    // WARNING: this also appears in JS
#define BAR_CHART_MAX_VIEW_LIMIT             "maxViewLimit"
#define BAR_CHART_MAX_VIEW_LIMIT_DEFAULT     50

/* Category (bar) info */
#define BAR_CHART_MAX_CATEGORIES        100

/* Category filter */
#define BAR_CHART_CATEGORY_SELECT      "categories"

/* Labels for categories */
#define BAR_CHART_CATEGORY_LABELS        "barChartBars"
#define BAR_CHART_CATEGORY_LABEL         "barChartLabel"
#define BAR_CHART_CATEGORY_LABEL_DEFAULT "Categories"

/* Colors for categories */
#define BAR_CHART_CATEGORY_COLORS        "barChartColors"

/* Units to display on bar mouseover */
#define BAR_CHART_UNIT                   "barChartUnit"

/* Measurement type -- e.g. 'median'. Shown as part of label on details page. Default is "" */
#define BAR_CHART_METRIC                 "barChartMetric"

/* Suppress whiteout behind graph (to show highlight and blue lines) */
#define BAR_CHART_NO_WHITEOUT         "noWhiteout"
#define BAR_CHART_NO_WHITEOUT_DEFAULT        FALSE

void barChartCfgUi(char *database, struct cart *cart, struct trackDb *tdb, char *track, 
                        char *title, boolean boxed);
/* Bar chart track type */

#define barChartAutoSqlString \
"table barChartBed\n" \
"\"BED6+ with additional fields for category count and values\"\n" \
    "(\n" \
    "string chrom;       \"Reference sequence chromosome or scaffold\"\n" \
    "uint   chromStart;  \"Start position in chromosome\"\n" \
    "uint   chromEnd;    \"End position in chromosome\"\n" \
    "string name;        \"Item identifier\"\n" \
    "uint   score;       \"Score from 0-1000; derived from total median all categories (log-transformed and scaled)\"\n" \
    "char[1] strand;     \"+ or - for strand\"\n" \
    "uint expCount;      \"Number of categories\"\n" \
    "float[expCount] expScores; \"Comma separated list of category values\"\n" \
    ")\n"

double barChartUiMaxMedianScore();
/* Max median score, for scaling */

struct barChartCategory *barChartUiGetCategories(char *database, struct trackDb *tdb);
/* Get category colors and descriptions.  
 * If barChartLabel setting contains label list, assign rainbow colors.
 * O/w look for a table naed track+Category, and use labels and colors there */

struct barChartCategory *barChartUiGetCategoryById(int id, char *database, 
                                                        struct trackDb *tdb);
/* Get category info by id */

char *barChartUiGetCategoryLabelById(int id, char *database, struct trackDb *tdb);
/* Get label for a category id */

char *barChartUiGetLabel(char *database, struct trackDb *tdb);
/* Get label for category list */

#endif /* BAR_CHARTUI_H */

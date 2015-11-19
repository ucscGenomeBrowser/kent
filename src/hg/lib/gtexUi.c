/* GTEx (Genotype Tissue Expression) track controls */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "trackDb.h"
#include "gtexUi.h"

void gtexGeneUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
/* GTEx (Genotype Tissue Expression) per gene data */
{
boxed = cfgBeginBoxAndTitle(tdb, boxed, title);
printf("<table%s><tr><td>",boxed?" width='100%'":"");

char cartVarName[1024];
char *selected = NULL;

/* Sample selection */
printf("<b>Samples:</b>&nbsp;");
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, GTEX_SAMPLES);
selected = cartCgiUsualString(cart, cartVarName, GTEX_SAMPLES_DEFAULT); 
boolean isAllSamples = sameString(selected, GTEX_SAMPLES_ALL);
cgiMakeRadioButton(cartVarName, GTEX_SAMPLES_ALL, isAllSamples);
printf("All\n");
cgiMakeRadioButton(cartVarName, GTEX_SAMPLES_COMPARE_SEX, !isAllSamples);
printf("Compare by gender\n");
printf("</p>");

/* Comparison type */
printf("<p><b>Comparison display:</b>\n");
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, GTEX_COMPARISON_DISPLAY);
selected = cartCgiUsualString(cart, cartVarName, GTEX_COMPARISON_DEFAULT); 
boolean isMirror = sameString(selected, GTEX_COMPARISON_MIRROR);
cgiMakeRadioButton(cartVarName, GTEX_COMPARISON_MIRROR, isMirror);
printf("Two graphs\n");
cgiMakeRadioButton(cartVarName, GTEX_COMPARISON_DIFF, !isMirror);
printf("Difference graph\n");
printf("</p>");

/* Data transform */
printf("<p><p>Log10 transform:</B>\n");
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, GTEX_LOG_TRANSFORM);
boolean isLogTransform = cartCgiUsualBoolean(cart, cartVarName, GTEX_LOG_TRANSFORM_DEFAULT);
cgiMakeCheckBox(cartVarName, isLogTransform);

/* Viewing limits max */
printf("&nbsp;&nbsp;<b>View limits maximum:</b>\n");
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, GTEX_MAX_LIMIT);
// TODO: set max and initial limits from gtexInfo table
int viewMax = cartCgiUsualInt(cart, cartVarName, GTEX_MAX_LIMIT_DEFAULT);
cgiMakeIntVar(cartVarName, viewMax, 4);
printf(" RPKM (range 10-180000)<br>\n");
printf("</p>");

/* Color scheme */
printf("<p><b>Tissue colors:</b>\n");
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, GTEX_COLORS);
selected = cartCgiUsualString(cart, cartVarName, GTEX_COLORS_DEFAULT); 
boolean isGtexColors = sameString(selected, GTEX_COLORS_GTEX);
cgiMakeRadioButton(cartVarName, GTEX_COLORS_GTEX, isGtexColors);
printf("GTEx\n");
cgiMakeRadioButton(cartVarName, GTEX_COLORS_RAINBOW, !isGtexColors);
printf("Rainbow\n");
printf("</p>");

cfgEndBox(boxed);
}

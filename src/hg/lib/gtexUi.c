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
printf("<TABLE%s><TR><TD>",boxed?" width='100%'":"");

char cartVarName[1024];
char *selected = NULL;

/* Sample selection */
printf("<p><b>Samples:</b>&nbsp;");
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, GTEX_SAMPLES);
selected = cartCgiUsualString(cart, cartVarName, GTEX_SAMPLES_DEFAULT); 
boolean isAllSamples = sameString(selected, GTEX_SAMPLES_ALL);
cgiMakeRadioButton(cartVarName, GTEX_SAMPLES_ALL, isAllSamples);
printf("All\n");
cgiMakeRadioButton(cartVarName, GTEX_SAMPLES_COMPARE_SEX, !isAllSamples);
printf("Compare by gender\n");

/* Data transform */
printf("<p><B>Log10 transform:</B>\n");
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, GTEX_LOG_TRANSFORM);
boolean isLogTransform = cartCgiUsualBoolean(cart, cartVarName, GTEX_LOG_TRANSFORM_DEFAULT);
cgiMakeCheckBox(cartVarName, isLogTransform);
printf("</p>\n");

/* Viewing limits max */
printf("<p><B>View limits maximum:</B>\n");
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, GTEX_MAX_LIMIT);
// TODO: set max and initial limits from gtexInfo table
cgiMakeDoubleVarWithLimits(cartVarName, 100, NULL, 5, 10, 178200);
printf("RKPM (range 10-180000)<br>\n");

/* Color scheme */
printf("<p><B>Tissue colors:</B>\n");
safef(cartVarName, sizeof(cartVarName), "%s.%s", name, GTEX_COLORS);
selected = cartCgiUsualString(cart, cartVarName, GTEX_COLORS_DEFAULT); 
boolean isGtexColors = sameString(selected, GTEX_COLORS_GTEX);
cgiMakeRadioButton(cartVarName, GTEX_COLORS_GTEX, isGtexColors);
printf("GTEx\n");
cgiMakeRadioButton(cartVarName, GTEX_COLORS_RAINBOW, !isGtexColors);
printf("rainbow\n");

cfgEndBox(boxed);
}

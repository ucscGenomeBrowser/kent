/* snakeUi - Snake Format user interface controls that are shared
 * between more than one CGI. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "hCommon.h"
#include "hui.h"
#include "jsHelper.h"
#include "snakeUi.h"


static void showSnpWidthCfg(struct cart *cart, struct trackDb *tdb,
				char *name, boolean parentLevel)
{
unsigned showSnpWidth = cartOrTdbInt(cart, tdb, 
    SNAKE_SHOW_SNP_WIDTH, SNAKE_DEFAULT_SHOW_SNP_WIDTH);

printf("<B>Maximum window size in which to show mismatches: </B>\n");

char varName[1024];
safef(varName, sizeof(varName), "%s." SNAKE_SHOW_SNP_WIDTH, name);
cgiMakeIntVar(varName, showSnpWidth, 10);
puts("<BR>");
}

static char *colorByOptionLabels[] =
    {
    SNAKE_COLOR_BY_STRAND_LABEL,
    SNAKE_COLOR_BY_CHROM_LABEL,
    SNAKE_COLOR_BY_NONE_LABEL,
    };

static char *colorByOptionValues[] =
    {
    SNAKE_COLOR_BY_STRAND_VALUE,
    SNAKE_COLOR_BY_CHROM_VALUE,
    SNAKE_COLOR_BY_NONE_VALUE,
    };

static void colorByCfg(struct cart *cart, struct trackDb *tdb,
				char *name, boolean parentLevel)
{
char *colorBy = cartOrTdbString(cart, tdb, 
    SNAKE_COLOR_BY, SNAKE_DEFAULT_COLOR_BY);

printf("<B>Block coloring method:</B>\n");

char varName[1024];
safef(varName, sizeof(varName), "%s." SNAKE_COLOR_BY, name);
cgiMakeDropListFull(varName, colorByOptionLabels,
			colorByOptionValues,
			ArraySize(colorByOptionLabels),
			colorBy, NULL, NULL);
puts("<BR>");
}

void snakeCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed)
{
boolean parentLevel = isNameAtParentLevel(tdb, name);

showSnpWidthCfg(cart, tdb, name, parentLevel);
colorByCfg(cart, tdb, name, parentLevel);
}


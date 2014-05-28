/* snakeUi - Snake Format user interface controls that are shared
 * between more than one CGI. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef SNAKEUI_H
#define SNAKEUI_H

#include "cart.h"
#include "trackDb.h"

#define SNAKE_SHOW_SNP_WIDTH "showSnpWidth"
#define SNAKE_DEFAULT_SHOW_SNP_WIDTH 50000


#define   SNAKE_COLOR_BY_STRAND_LABEL     "by strand"
#define   SNAKE_COLOR_BY_CHROM_LABEL      "by chromosome"
#define   SNAKE_COLOR_BY_NONE_LABEL       "always blue"

#define   SNAKE_COLOR_BY_STRAND_VALUE     "byStrand"
#define   SNAKE_COLOR_BY_CHROM_VALUE      "byChromosome"
#define   SNAKE_COLOR_BY_NONE_VALUE       "none"

#define SNAKE_COLOR_BY "colorBy"
#define SNAKE_DEFAULT_COLOR_BY SNAKE_COLOR_BY_NONE_VALUE

void snakeCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
/* Complete track controls for snakes. */

#endif//def SNAKEUI_H

/* gtexUi - GTEx (Genotype Tissue Expression) tracks */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef GTEXUI_H
#define GTEXUI_H

#include "cart.h"
#include "trackDb.h"

// Color scheme
#define GTEX_COLORS             "colorScheme"
#define GTEX_COLORS_RAINBOW     "rainbow"
// color scheme from GTEX papers and portal
#define GTEX_COLORS_GTEX        "gtex"
#define GTEX_COLORS_DEFAULT     GTEX_COLORS_GTEX

// Graph type
#define GTEX_GRAPH              "graphType"
#define GTEX_GRAPH_RAW          "raw"
#define GTEX_GRAPH_NORMAL       "normalized"
// comparison graphs
#define GTEX_GRAPH_SEX          "sex"
#define GTEX_GRAPH_AGE          "age"
#define GTEX_GRAPH_AGE_YEARS    "years"
#define GTEX_GRAPH_AGE_DEFAULT   50

#define GTEX_GRAPH_DEFAULT      GTEX_GRAPH_RAW

#endif /* GTEXUI_H */

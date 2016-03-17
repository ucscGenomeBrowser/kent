/* gtexUi - GTEx (Genotype Tissue Expression) tracks */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef GTEXUI_H
#define GTEXUI_H

/* Color scheme */
#define GTEX_COLORS                     "colorScheme"
#define GTEX_COLORS_RAINBOW             "rainbow"

/* Color scheme from GTEX papers and portal */
#define GTEX_COLORS_GTEX                "gtex"
#define GTEX_COLORS_DEFAULT             GTEX_COLORS_GTEX

/* Data transform */
    // WARNING: this also appears in JS
#define GTEX_LOG_TRANSFORM              "logTransform"
#define GTEX_LOG_TRANSFORM_DEFAULT      TRUE

/* Viewing limits */
    // WARNING: this also appears in JS
#define GTEX_MAX_LIMIT                  "maxLimit"
#define GTEX_MAX_LIMIT_DEFAULT          300
/* TODO: Get default from gtexInfo table */

/* Sample selection and comparison */
    // WARNING: this also appears in JS
#define GTEX_SAMPLES                    "samples"
#define GTEX_SAMPLES_ALL                "all"
#define GTEX_SAMPLES_COMPARE_SEX        "sex"
#define GTEX_SAMPLES_DEFAULT            GTEX_SAMPLES_ALL

#define GTEX_SAMPLES_COMPARE_AGE        "age"
#define GTEX_COMPARE_AGE_YEARS          "years"
#define GTEX_COMPARE_AGE_DEFAULT        50

    // WARNING: this also appears in JS
#define GTEX_COMPARISON_DISPLAY         "comparison"
#define GTEX_COMPARISON_MIRROR          "mirror"
#define GTEX_COMPARISON_DIFF            "difference"
#define GTEX_COMPARISON_DEFAULT         GTEX_COMPARISON_DIFF

/* Graph type */
#define GTEX_GRAPH              "graphType"
#define GTEX_GRAPH_RAW          "raw"
#define GTEX_GRAPH_NORMAL       "normalized"
#define GTEX_GRAPH_DEFAULT      GTEX_GRAPH_RAW

/* Tissue filter */
#define GTEX_TISSUE_SELECT      "tissues"

#endif /* GTEXUI_H */

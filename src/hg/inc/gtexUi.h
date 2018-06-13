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
#define GTEX_MAX_VIEW_LIMIT                  "maxViewLimit"
#define GTEX_MAX_VIEW_LIMIT_DEFAULT          300
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

/* Gene filter */
#define GTEX_CODING_GENE_FILTER                 "codingOnly"
#define GTEX_CODING_GENE_FILTER_DEFAULT         FALSE

/* Hide exons */
#define GTEX_SHOW_EXONS         "showExons"
#define GTEX_SHOW_EXONS_DEFAULT FALSE

/* Suppress whiteout behind graph (to show highlight and blue lines) */
#define GTEX_NO_WHITEOUT         "noWhiteout"
#define GTEX_NO_WHITEOUT_DEFAULT        FALSE

/* Item name is gene symbol, accession, or both */
#define GTEX_LABEL                "label"
#define GTEX_LABEL_SYMBOL         "name"
#define GTEX_LABEL_ACCESSION      "accession"
#define GTEX_LABEL_BOTH           "both"
#define GTEX_LABEL_DEFAULT  GTEX_LABEL_SYMBOL

/* GTEx eQTL track controls */

#define GTEX_EQTL_EFFECT                "effect"
#define GTEX_EQTL_EFFECT_DEFAULT        0.0
#define GTEX_EQTL_PROBABILITY           "prob"
#define GTEX_EQTL_PROBABILITY_DEFAULT   0.0
#define GTEX_EQTL_GENE                  "gene"
#define GTEX_EQTL_TISSUE_COLOR          "tissueColor"
#define GTEX_EQTL_TISSUE_COLOR_DEFAULT  TRUE

/* Identify GTEx tracks that use special trackUI. 
 * NOTE: trackDb must follow this naming convention unless/until there is
 * a new trackType.
 */ 
#define GTEX_GENE_TRACK_BASENAME        "gtexGene"
#define GTEX_EQTL_TRACK_BASENAME        "gtexEqtlCluster"

boolean gtexIsGeneTrack(char *trackName);
/* Identify GTEx gene track so custom trackUi CGI can be launched */

boolean gtexIsEqtlTrack(char *trackName);
/* Identify GTEx eqtl track so custom trackUi CGI can be launched */

char *gtexTrackUiName();
/* Refer to Body Map CGI if suitable */

void gtexPortalLink(char *geneId);
/* print URL to GTEX portal gene expression page using Ensembl Gene Id*/

void gtexBodyMapLink();
/* print URL to GTEX body map HTML page */

boolean gtexGeneBoxplot(char *geneId, char *geneName, char *version, 
                                boolean doLogTransform, struct tempName *pngTn);
/* Create a png temp file with boxplot of GTEx expression values for this gene. 
 * GeneId is the Ensembl gene ID.  GeneName is the HUGO name, used for graph title;
 * If NULL, label with the Ensembl gene ID */

/* UI controls */

void gtexGeneUiGeneLabel(struct cart *cart, char *track, struct trackDb *tdb);
/* Radio buttons to select format of gene label */

void gtexGeneUiCodingFilter(struct cart *cart, char *track, struct trackDb *tdb);
/* Checkbox to restrict display to protein coding genes */

void gtexGeneUiGeneModel(struct cart *cart, char *track, struct trackDb *tdb);
/* Checkbox to enable display of GTEx gene model */

void gtexGeneUiLogTransform(struct cart *cart, char *track, struct trackDb *tdb);
/* Checkbox to select log-transformed RPKM values */

void gtexGeneUiViewLimits(struct cart *cart, char *track, struct trackDb *tdb);
/* Set viewing limits if log transform not checked */

void gtexGeneUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
/* GTEx (Genotype Tissue Expression) per gene data */

void gtexEqtlUiGene(struct cart *cart, char *track, struct trackDb *tdb);
/* Limit to selected gene */

void gtexEqtlUiEffectSize(struct cart *cart, char *track, struct trackDb *tdb);
/* Limit to items with absolute value of effect size >= threshold.  Use largest
 * effect size in tissue list */

void gtexEqtlUiProbability(struct cart *cart, char *track, struct trackDb *tdb);
/* Limit to items with specified probability.  Use largest probability in tissue list,
 * which is score/1000, so use that */

void gtexEqtlUiTissueColor(struct cart *cart, char *track, struct trackDb *tdb);
/* Control visibility color patch to indicate tissue (can be distracting in large regions) */

void gtexEqtlClusterUi(struct cart *cart, struct trackDb *tdb, char *track, char *title, 
                        boolean boxed);
/* GTEx (Genotype Tissue Expression) eQTL clusters. Use this on right-click,
 * (when hgGtexTrackSettings can't be) */


#endif /* GTEXUI_H */

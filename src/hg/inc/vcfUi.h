/* vcfUi - Variant Call Format user interface controls that are shared
 * between more than one CGI. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef VCFUI_H
#define VCFUI_H

#include "cart.h"
#include "trackDb.h"
#include "vcf.h"

#define VCF_HAP_HEIGHT_VAR "hapClusterHeight"
#define VCF_DEFAULT_HAP_HEIGHT 128

#define VCF_HAP_ENABLED_VAR "hapClusterEnabled"
#define VCF_HAP_METHOD_VAR "hapClusterMethod"
#define VCF_HAP_METHOD_CENTER_WEIGHTED "centerWeighted"
#define VCF_HAP_METHOD_FILE_ORDER "fileOrder"
#define VCF_HAP_METHOD_TREE_FILE "treeFile"
#define VCF_DEFAULT_HAP_METHOD VCF_HAP_METHOD_CENTER_WEIGHTED

#define VCF_HAP_COLORBY_VAR "hapClusterColorBy"
#define VCF_HAP_COLORBY_ALTONLY "altOnly"
#define VCF_HAP_COLORBY_FUNCTION "function"
#define VCF_HAP_COLORBY_REFALT "refAlt"
#define VCF_HAP_COLORBY_BASE "base"
#define VCF_DEFAULT_HAP_COLORBY VCF_HAP_COLORBY_ALTONLY

#define VCF_HAP_TREEANGLE_VAR "hapClusterTreeAngle"
#define VCF_HAP_TREEANGLE_TRIANGLE "triangle"
#define VCF_HAP_TREEANGLE_RECTANGLE "rectangle"
#define VCF_DEFAULT_HAP_TREEANGLE VCF_HAP_TREEANGLE_TRIANGLE

#define VCF_SAMPLE_COLOR_FILE "sampleColorFile"

#define VCF_SHOW_HW_VAR "showHardyWeinberg"

#define VCF_APPLY_MIN_QUAL_VAR "applyMinQual"
#define VCF_DEFAULT_APPLY_MIN_QUAL FALSE

#define VCF_DO_QUAL_UI "vcfDoQual"
#define VCF_MIN_QUAL_VAR "minQual"
#define VCF_DEFAULT_MIN_QUAL 0

#define VCF_DO_FILTER_UI "vcfDoFilter"

#define VCF_EXCLUDE_FILTER_VAR "excludeFilterValues"

#define VCF_DO_MAF_UI "vcfDoMaf"
#define VCF_MIN_ALLELE_FREQ_VAR "minFreq"
#define VCF_DEFAULT_MIN_ALLELE_FREQ 0.0

#define VCF_PHASED_CHILD_SAMPLE_SETTING "vcfChildSample"
#define VCF_PHASED_PARENTS_SAMPLE_SETTING "vcfParentSamples"
#define VCF_PHASED_SAMPLE_ORDER_VAR "vcfSampleOrder"
#define VCF_PHASED_MAX_OTHER_SAMPLES 2
#define VCF_PHASED_DEFAULT_LABEL_VAR "doDefaultLabel"
#define VCF_PHASED_ALIAS_LABEL_VAR "doAliasLabel"
#define VCF_PHASED_HIDE_OTHER_VAR "hideParents"
#define VCF_PHASED_TDB_USE_ALT_NAMES "vcfUseAltSampleNames"
#define VCF_PHASED_CHILD_BELOW_VAR "sortChildBelow"

#define VCF_PHASED_COLORBY_VAR "vcfPhasedColorBy"
#define VCF_PHASED_COLORBY_MENDEL_DIFF "mendelDiff"
#define VCF_PHASED_COLORBY_DE_NOVO "deNovo"
#define VCF_PHASED_COLORBY_FUNCTION "function"
#define VCF_PHASED_COLORBY_DEFAULT "noColor"

void vcfCfgHaplotypeCenter(struct cart *cart, struct trackDb *tdb, char *track,
			   boolean parentLevel, struct vcfFile *vcff,
			   char *thisName, char *thisChrom, int thisPos, char *formName);
/* If vcff has genotype data, show status and controls for choosing the center variant
 * for haplotype clustering/sorting in hgTracks. */

struct slPair *vcfPhasedGetSampleOrder(struct cart *cart, struct trackDb *tdb, boolean parentLevel, boolean hideOtherSamples);
/* Parse out a trio sample order from trackDb */

void vcfCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
/* Complete track controls for VCF. */

char *vcfHaplotypeOrSample(struct cart *cart);
/* Return "Sample" if the current organism is uniploid (like SARS-CoV-2), "Haplotype" otherwise. */

#endif//def VCF_UI

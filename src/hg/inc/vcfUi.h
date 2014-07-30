/* vcfUi - Variant Call Format user interface controls that are shared
 * between more than one CGI. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef VCFUI_H
#define VCFUI_H

#include "cart.h"
#include "trackDb.h"
#include "vcf.h"

#define VCF_HAP_HEIGHT_VAR "hapClusterHeight"
#define VCF_DEFAULT_HAP_HEIGHT 128

#define VCF_HAP_ENABLED_VAR "hapClusterEnabled"

#define VCF_HAP_COLORBY_VAR "hapClusterColorBy"
#define VCF_HAP_COLORBY_ALTONLY "altOnly"
#define VCF_HAP_COLORBY_REFALT "refAlt"
#define VCF_HAP_COLORBY_BASE "base"
#define VCF_DEFAULT_HAP_COLORBY VCF_HAP_COLORBY_ALTONLY

#define VCF_HAP_TREEANGLE_VAR "hapClusterTreeAngle"
#define VCF_HAP_TREEANGLE_TRIANGLE "triangle"
#define VCF_HAP_TREEANGLE_RECTANGLE "rectangle"
#define VCF_DEFAULT_HAP_TREEANGLE VCF_HAP_TREEANGLE_TRIANGLE

#define VCF_SHOW_HW_VAR "showHardyWeinberg"

#define VCF_APPLY_MIN_QUAL_VAR "applyMinQual"
#define VCF_DEFAULT_APPLY_MIN_QUAL FALSE

#define VCF_MIN_QUAL_VAR "minQual"
#define VCF_DEFAULT_MIN_QUAL 0

#define VCF_EXCLUDE_FILTER_VAR "excludeFilterValues"

#define VCF_MIN_ALLELE_FREQ_VAR "minFreq"
#define VCF_DEFAULT_MIN_ALLELE_FREQ 0.0

void vcfCfgHaplotypeCenter(struct cart *cart, struct trackDb *tdb, char *track,
			   boolean parentLevel, struct vcfFile *vcff,
			   char *thisName, char *thisChrom, int thisPos, char *formName);
/* If vcff has genotype data, show status and controls for choosing the center variant
 * for haplotype clustering/sorting in hgTracks. */

void vcfCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
/* Complete track controls for VCF. */

#endif//def VCF_UI

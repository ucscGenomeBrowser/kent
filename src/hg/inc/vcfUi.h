/* vcfUi - Variant Call Format user interface controls that are shared
 * between more than one CGI. */
#ifndef VCFUI_H
#define VCFUI_H

#include "cart.h"
#include "trackDb.h"
#include "vcf.h"

void vcfCfgHaplotypeCenter(struct cart *cart, struct trackDb *tdb, struct vcfFile *vcff,
			   char *thisName, char *thisChrom, int thisPos, char *formName);
/* If vcff has genotype data, show status and controls for choosing the center variant
 * for haplotype clustering/sorting in hgTracks. */

void vcfCfgUi(struct cart *cart, struct trackDb *tdb, char *name, char *title, boolean boxed);
/* Complete track controls for VCF. */

#endif//def VCF_UI

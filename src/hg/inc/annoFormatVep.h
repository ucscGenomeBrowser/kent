/* annoFormatVep -- write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName, interpreting input rows according to config.
 * See http://uswest.ensembl.org/info/docs/variation/vep/vep_formats.html */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef ANNOFORMATVEP_H
#define ANNOFORMATVEP_H

#include "annoFormatter.h"

struct annoFormatter *annoFormatVepNew(char *fileName, boolean doHtml,
				       struct annoStreamer *variantSource,
				       char *variantDescription,
				       struct annoStreamer *gpVarSource,
				       char *gpVarDescription,
				       struct annoStreamer *snpSource,
				       char *snpDescription,
				       struct annoAssembly *assembly);
/* Return a formatter that will write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName (can be "stdout").
 * variantSource and gpVarSource must be provided; snpSource can be NULL. */

void annoFormatVepAddExtraItem(struct annoFormatter *self, struct annoStreamer *extraSource,
			       char *tag, char *description, char *column);
/* Tell annoFormatVep that it should include the given column of extraSource
 * in the EXTRAS column with tag.  The VEP header will include tag's description.
 * For some special-cased sources e.g. dbNsfp files, column may be ignored. */

void annoFormatVepAddRegulatory(struct annoFormatter *self, struct annoStreamer *regSource,
				char *tag, char *description, char *column);
/* Tell annoFormatVep to use the regulatory_region_variant consequence if there is overlap
 * with regSource, and to include the given column of regSource in the EXTRAS column.
 * The VEP header will include tag's description. */

#endif//ndef ANNOFORMATVEP_H

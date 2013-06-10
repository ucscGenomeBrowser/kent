/* annoFormatVep -- write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName, interpreting input rows according to config.
 * See http://uswest.ensembl.org/info/docs/variation/vep/vep_formats.html */

#ifndef ANNOFORMATVEP_H
#define ANNOFORMATVEP_H

#include "annoFormatter.h"

struct annoFormatter *annoFormatVepNew(char *fileName, struct annoStreamer *variantSource,
				       char *variantDescription,
				       struct annoStreamer *gpVarSource,
				       char *gpVarDescription,
				       struct annoStreamer *snpSource,
				       char *snpDescription);
/* Return a formatter that will write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName (can be "stdout").
 * variantSource and gpVarSource must be provided; snpSource can be NULL. */

void annoFormatVepAddExtraItem(struct annoFormatter *self, struct annoStreamer *extraSource,
			       char *tag, char *description, char *column);
/* Tell annoFormatVep that it should include the given column of extraSource
 * in the EXTRAS column with tag.  The VEP header will include tag's description.
 * For some special-cased sources e.g. dbNsfp files, column may be ignored. */

#endif//ndef ANNOFORMATVEP_H

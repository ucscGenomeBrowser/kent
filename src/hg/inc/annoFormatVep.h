/* annoFormatVep -- write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName, interpreting input rows according to config.
 * See http://uswest.ensembl.org/info/docs/variation/vep/vep_formats.html */

#ifndef ANNOFORMATVEP_H
#define ANNOFORMATVEP_H

#include "annoFormatter.h"

struct annoFormatVepExtraItem
    // A single input column whose value should be placed in the Extras output column,
    // identified by tag.
    {
    struct annoFormatVepExtraItem *next;
    char *tag;					// Keyword to use in extras column (tag=value;)
    char *description;				// Text description for output header
    int rowIx;					// Offset of column in row from data source
						// (N/A for wig sources)
    };

struct annoFormatVepExtraSource
    // A streamer or grator that supplies at least one value for Extras output column.
    {
    struct annoFormatVepExtraSource *next;
    struct annoStreamer *source;		// streamer or grator: same pointers as below
    struct annoFormatVepExtraItem *items;	// one or more columns of source and their tags
    };

struct annoFormatVepConfig
    // Describe the primary source and grators (whose rows must be delivered in this order)
    // that provide data for VEP output columns.
    {
    struct annoStreamer *variantSource;		// Primary source: variants
    struct annoStreamer *gpVarSource;		// annoGratorGpVar makes the core predictions
    struct annoStreamer *snpSource;		// Latest dbSNP provides IDs of known variants
    struct annoFormatVepExtraSource *extraSources;	// Everything else that may be tacked on
    };

struct annoFormatter *annoFormatVepNew(char *fileName, struct annoFormatVepConfig *config);
/* Return a formatter that will write functional predictions in the same format as Ensembl's
 * Variant Effect Predictor to fileName, interpreting input rows according to config. */

#endif//ndef ANNOFORMATVEP_H

/* chromGraphFactory - turn user-uploaded track in chromGraph format
 * into a customTrack. */

/* Copyright (C) 2009 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CHROMGRAPHFACTORY_H
#define CHROMGRAPHFACTORY_H

#include "customFactory.h"

struct customTrack *chromGraphParser(char *genomeDb, struct customPp *cpp,
	char *formatType, char *markerType, char *columnLabels,
	char *name, char *description, struct hash *settings,
	boolean report);
/* Parse out a chromGraph file (not including any track lines) */

extern struct customFactory chromGraphFactory;

/* Symbolic defines for types of markers we support. */
#define cgfMarkerGuess "best guess"
#define cgfMarkerGenomic "chromosome base"
#define cgfMarkerSts "STS marker"
#define cgfMarkerSnp "dbSNP rsID"
#define cgfMarkerAffy100 "(Affymetrix 100K Gene Chip)"
#define cgfMarkerAffy500 "Affymetrix 500k Gene Chip"
#define cgfMarkerAffy6 "Affymetrix Genome-Wide SNP Array 6"
#define cgfMarkerAffy6SV "Affymetrix SNP Array 6 Structural-Variation"

#define cgfMarkerHumanHap300 "Illumina HumanHap300 BeadChip"
#define cgfMarkerHumanHap550 "Illumina HumanHap550 BeadChip"
#define cgfMarkerHumanHap650 "Illumina HumanHap650 BeadChip"
#define cgfMarkerHumanHap1M  "Illumina HumanHap1M BeadChip"

#define cgfMarkerAgilentCgh244A "Agilent CGH 244A"

/* Symbolic defines for type of format we support. */
#define cgfFormatGuess "best guess"
#define cgfFormatTab "tab delimited"
#define cgfFormatComma "comma delimited"
#define cgfFormatSpace "whitespace separated"

/* Symbolic defines for types of labels we support. */
#define cgfColLabelGuess	"best guess"
#define cgfColLabelNumbered	"numbered"
#define cgfColLabelFirstRow	"first row"

#endif /* CHROMGRAPHFACTORY_H */

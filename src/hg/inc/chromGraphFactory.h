/* chromGraphFactory - turn user-uploaded track in chromGraph format
 * into a customTrack. */

#ifndef CHROMGRAPHFACTORY_H
#define CHROMGRAPHFACTORY_H

#ifndef CUSTOMFACTORY_H
#include "customFactory.h"
#endif

struct customTrack *chromGraphParser(struct customPp *cpp,
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
#define cgfMarkerAffy500 "(Affymetrix 500k Gene Chip)"
#define cgfMarkerHumanHap300 "(Illumina HumanHap300 BeadChip)"

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

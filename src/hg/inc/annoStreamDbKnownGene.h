/* annoStreamDbKnownGene -- subclass of annoStreamer for adding gene symbol to knownGene table */

#ifndef ANNOSTREAMDBKNOWNGENE_H
#define ANNOSTREAMDBKNOWNGENE_H

#include "annoStreamer.h"

struct asObject *annoStreamDbKnownGeneAsObj();
/* Return an autoSql object that describs fields of a joining query on knownGene and
 * kgXref.geneSymbol. */

struct annoStreamer *annoStreamDbKnownGeneNew(char *db, struct annoAssembly *aa, int maxOutRows);
/* Create an annoStreamer (subclass) object using two database tables:
 * knownGene: the UCSC Genes main track table
 * kgXref: the related table that contains the HGNC gene symbol that everyone wants to see
 * This streamer's rows are just like a plain annoStreamDb on knownGene, but with an
 * extra column at the end, 'name2', which is recognized as a gene symbol column due to
 * its use in refGene.
 */

#endif//ndef ANNOSTREAMDBKNOWNGENE_H

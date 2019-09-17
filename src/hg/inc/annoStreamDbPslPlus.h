/* annoStreamDbPslPlus -- subclass of annoStreamer for joining PSL+CDS+seq database tables */

#ifndef ANNOSTREAMDBPSLPLUS_H
#define ANNOSTREAMDBPSLPLUS_H

#include "annoStreamer.h"
#include "jsonParse.h"

#define PSLPLUS_NUM_COLS PSL_NUM_COLS+6
#define PSLPLUS_CDS_IX PSL_NUM_COLS+0
#define PSLPLUS_PROTACC_IX PSL_NUM_COLS+1
#define PSLPLUS_NAME2_IX PSL_NUM_COLS+2
#define PSLPLUS_PATH_IX PSL_NUM_COLS+3
#define PSLPLUS_FILEOFFSET_IX PSL_NUM_COLS+4
#define PSLPLUS_FILESIZE_IX PSL_NUM_COLS+5

struct asObject *annoStreamDbPslPlusAsObj();
/* Return an autoSql object with PSL, gene name, protein acc, CDS and sequence file info fields.
 * An annoStreamDbPslPlus instance may return additional additional columns if configured, but
 * these columns will always be present. */

struct annoStreamer *annoStreamDbPslPlusNew(struct annoAssembly *aa, char *gpTable, int maxOutRows,
                                            struct jsonElement *config);
/* Create an annoStreamer (subclass) object that streams PSL, CDS and seqFile info.
 * gpTable is a genePred table that has associated PSL, CDS and sequence info
 * (i.e. refGene, ncbiRefSeq, ncbiRefSeqCurated or ncbiRefSeqPredicted). */

#endif//ndef ANNOSTREAMDBPSLPLUS_H

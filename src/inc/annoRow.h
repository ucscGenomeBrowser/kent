/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

#ifndef ANNOROW_H
#define ANNOROW_H

#include "common.h"

enum annoRowType { arUnknown, arWords, arWig, arVcf };

// stub in order to avoid problems with circular .h references:
struct annoStreamer;

struct annoRow
/* Representation of a row from a database table or file.  The chrom, start and end
 * facilitate intersection by position.  If type is arWords, then data is an array
 * of strings corresponding to columns in the autoSql definition provided by the
 * source of the annoRow.  If type is arWig, then data is an array of floats.
 * rightJoinFail is true if this row failed a filter marked as rightJoin, meaning it
 * can knock out the primary row (see annoFilter.h). */
    {
    struct annoRow *next;
    char *chrom;
    uint start;
    uint end;
    void *data;
    boolean rightJoinFail;
    };

struct annoRow *annoRowFromStringArray(char *chrom, uint start, uint end, boolean rightJoinFail,
				       char **wordsIn, int numCols);
/* Allocate & return an annoRow with data cloned from wordsIn. */

struct annoRow *annoRowWigNew(char *chrom, uint start, uint end, boolean rightJoinFail,
			      float *values);
/* Allocate & return an annoRowWig, with clone of values; length of values is (end-start). */

struct annoRow *annoRowClone(struct annoRow *rowIn, struct annoStreamer *source);
/* Allocate & return a single annoRow cloned from rowIn.
 * numCols is used only if rowIn->type is arWords. */

void annoRowFree(struct annoRow **pRow, struct annoStreamer *source);
/* Free a single annoRow. */

void annoRowFreeList(struct annoRow **pList, struct annoStreamer *source);
/* Free a list of annoRows. */

#endif//ndef ANNOROW_H

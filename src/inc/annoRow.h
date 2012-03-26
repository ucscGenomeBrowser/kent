/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

#ifndef ANNOROW_H
#define ANNOROW_H

#include "common.h"

struct annoRow
/* Representation of a row from a database table or file.  The chrom, start and end
 * facilitate intersection by position.  The words correspond to columns in the autoSql
 * definition provided by the source of the annoRow.  rightJoinFail is true if this row
 * failed a filter marked as rightJoin, meaning it can knock out the primary row (see
 * annoFilter.h). */
    {
    struct annoRow *next;
    char *chrom;
    uint start;
    uint end;
    char **words;
    boolean rightJoinFail;
    };

struct annoRow *annoRowFromStringArray(char *chrom, uint start, uint end, boolean rightJoinFail,
				       char **wordsIn, int numCols);
/* Allocate & return an annoRow with words cloned from wordsIn. */

struct annoRow *annoRowClone(struct annoRow *rowIn, int numCols);
/* Allocate & return a single annoRow cloned from rowIn. */

void annoRowFree(struct annoRow **pRow, int numCols);
/* Free a single annoRow. */

void annoRowFreeList(struct annoRow **pList, int numCols);
/* Free a list of annoRows. */

#endif//ndef ANNOROW_H

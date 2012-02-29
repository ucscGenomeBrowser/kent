/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

#ifndef ANNOROW_H
#define ANNOROW_H

#include "common.h"

struct annoRow
/* Representation of a row from a database table or file.  The chrom, chromStart and chromEnd
 * facilitate intersection by position.  The words correspond to columns in the autoSql
 * definition provided by the source of the annoRow. */
    {
    struct annoRow *next;
    char *chrom;
    uint chromStart;
    uint chromEnd;
    char **words;
    };

struct annoRow *annoRowFromStringArray(char *chrom, uint chromStart, uint chromEnd,
				       char **wordsIn, int numCols);
/* Allocate & return an annoRow with words cloned from wordsIn. */

#endif//ndef ANNOROW_H

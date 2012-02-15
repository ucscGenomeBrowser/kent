/* annoRow -- basic data interchange unit of annoGratorQuery framework. */

#ifndef ANNOROW_H
#define ANNOROW_H

#include "common.h"

struct annoRow
/* Representation of a row from a database table or file.  The chrom, chromStart and chromEnd
 * facilitate intersection by position.  The rest of the row data is purposefully left vague so
 * that different data types can use the most suitable representation, e.g. a struct bed4
 * or a char *row[] or a struct that contains a genePred and a functionPred. */
    {
    struct annoRow *next;
    char *chrom;
    uint chromStart;
    uint chromEnd;
    void *data;
    };

struct annoRow *annoRowFromStringArray(char *chrom, uint chromStart, uint chromEnd,
				       char **rowIn, int numCols);
/* Allocate & return an annoRow with data = new char ** row, with elements copied from rowIn. */

#endif//ndef ANNOROW_H

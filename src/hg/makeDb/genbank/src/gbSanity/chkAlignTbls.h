/* validation of all alignment-based tables */
#ifndef CHKALIGNTBLS_H
#define CHKALIGNTBLS_H

#include "common.h"

struct sqlConnection;
struct metaDataTbls;
struct gbSelect;

void chkAlignTables(struct gbSelect* select, struct sqlConnection* conn,
                    struct metaDataTbls* metaDataTbls, boolean noPerChrom);
/* Verify all of the alignment-related tables. */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

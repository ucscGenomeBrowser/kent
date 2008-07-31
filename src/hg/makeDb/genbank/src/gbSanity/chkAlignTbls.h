/* validation of all alignment-based tables */
#ifndef CHKALIGNTBLS_H
#define CHKALIGNTBLS_H

#include "common.h"

struct sqlConnection;
struct metaDataTbls;
struct gbSelect;
struct dbLoadOptions;

int chkAlignTables(char *db, struct gbSelect* select, struct sqlConnection* conn,
                   struct metaDataTbls* metaDataTbls, struct dbLoadOptions *options);
/* Verify all of the alignment-related tables. */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

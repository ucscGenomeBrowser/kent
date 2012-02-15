/* annoStreamPgSnp -- subclass of annoStreamer for pgSnp data */

#ifndef ANNOSTREAMPGSNP_H
#define ANNOSTREAMPGSNP_H

#include "annoStreamer.h"
#include "jksql.h"

struct annoStreamer *annoStreamPgSnpNewDb(struct sqlConnection *conn, char *table);
/* Create an annoStreamer (subclass) object from a pgSnp database table. */

#endif//ndef ANNOSTREAMPGSNP_H

/* annoStreamDb -- subclass of annoStreamer for database tables */

#ifndef ANNOSTREAMDB_H
#define ANNOSTREAMDB_H

#include "annoStreamer.h"
#include "jksql.h"

struct annoStreamer *annoStreamDbNew(struct sqlConnection *conn, char *table,
				     struct asObject *asObj);
/* Create an annoStreamer (subclass) object from a database table described by asObj.
 * conn must not be shared. */

#endif//ndef ANNOSTREAMDB_H

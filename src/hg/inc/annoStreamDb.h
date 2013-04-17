/* annoStreamDb -- subclass of annoStreamer for database tables */

#ifndef ANNOSTREAMDB_H
#define ANNOSTREAMDB_H

#include "annoStreamer.h"

struct annoStreamer *annoStreamDbNew(char *db, char *table, struct annoAssembly *aa,
				     struct asObject *asObj);
/* Create an annoStreamer (subclass) object from a database table described by asObj. */

#endif//ndef ANNOSTREAMDB_H

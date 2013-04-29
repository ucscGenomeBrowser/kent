/* annoStreamWig -- subclass of annoStreamer for wiggle database tables */

#ifndef ANNOSTREAMWIG_H
#define ANNOSTREAMWIG_H

#include "annoStreamer.h"

struct annoStreamer *annoStreamWigDbNew(char *db, char *table, struct annoAssembly *aa,
					int maxOutput);
/* Create an annoStreamer (subclass) object from a wiggle database table. */

#endif//ndef ANNOSTREAMWIG_H

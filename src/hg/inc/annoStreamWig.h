/* annoStreamWig -- subclass of annoStreamer for wiggle database tables */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef ANNOSTREAMWIG_H
#define ANNOSTREAMWIG_H

#include "annoStreamer.h"

struct annoStreamer *annoStreamWigDbNew(char *db, char *table, struct annoAssembly *aa,
					int maxOutput);
/* Create an annoStreamer (subclass) object from a wiggle database table. */

#endif//ndef ANNOSTREAMWIG_H

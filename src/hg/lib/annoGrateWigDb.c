/* annoGrateWigDb -- subclass of annoGrator for wiggle database table & file */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoGrateWigDb.h"
#include "annoStreamWig.h"
#include "wiggle.h"

struct annoGrator *annoGrateWigDbNew(char *db, char *table, struct annoAssembly *aa,
                                     enum annoGrateWigMode mode, int maxOutput)
/* Create an annoGrator subclass for wiggle data from db.table (and the file it points to).
 * See src/inc/annoGrateWig.h for a description of mode. */
{
struct annoStreamer *wigSource = annoStreamWigDbNew(db, table, aa, maxOutput);
return annoGrateWigNew(wigSource, mode);
}

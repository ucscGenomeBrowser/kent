/* annoGrateWigDb -- subclass of annoGrator for wiggle database table & file */

#ifndef ANNOGRATEWIGDB_H
#define ANNOGRATEWIGDB_H

#include "annoGrateWig.h"

struct annoGrator *annoGrateWigDbNew(char *db, char *table, int maxOutput);
/* Create an annoGrator subclass for wiggle data from db.table (and the file it points to). */

#endif//ndef ANNOGRATEWIGDB_H

/* annoGrateWig -- subclass of annoGrator for wiggle data */

#ifndef ANNOGRATEWIG_H
#define ANNOGRATEWIG_H

#include "annoGrator.h"

struct annoGrator *annoGrateWigDbNew(char *db, char *table, int maxOutput);
/* Create an annoGrator subclass for wiggle data from db.table (and the file it points to). */

#endif//ndef ANNOGRATEWIG_H

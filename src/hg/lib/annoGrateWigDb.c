/* annoGrateWigDb -- subclass of annoGrator for wiggle database table & file */

#include "annoGrateWigDb.h"
#include "annoStreamWig.h"
#include "wiggle.h"

struct annoGrator *annoGrateWigDbNew(char *db, char *table, int maxOutput)
/* Create an annoGrator subclass for wiggle data from db.table (and the file it points to). */
{
struct annoStreamer *wigSource = annoStreamWigDbNew(db, table, maxOutput);
return annoGrateWigNew(wigSource);
}


#include "common.h"
#include "trackDb.h"
#include "customComposite.h"

boolean isCustomComposite(struct trackDb *tdb)
/* Is this track part of a custom composite. */
{
return trackDbSettingClosestToHomeOn(tdb, CUSTOM_COMPOSITE_SETTING);
}


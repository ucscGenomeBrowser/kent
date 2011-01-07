// Track Search code which is shared between different CGIs

#include "searchTracks.h"
#include "hdb.h"
#include "hgConfig.h"
#include "trix.h"
#include "subText.h"
#include "jsHelper.h"

void getSearchTrixFile(char *database, char *buf, int len)
// Fill-in the name of the track search trix file
{
char *trixPath = cfgOptionDefault("browser.trixPath", "/gbdb/$db/trackDb.ix");
struct subText *subList = subTextNew("$db", database);
subTextStatic(subList, trixPath, buf, len);
}

boolean isSearchTracksSupported(char *database, struct cart *cart)
// Return TRUE if searchTracks is supported for this database and javascript is supported too
{
if (!advancedJavascriptFeaturesEnabled(cart))
    return FALSE;

char trixFile[HDB_MAX_PATH_STRING];
getSearchTrixFile(database, trixFile, sizeof(trixFile));
return fileExists(trixFile);
}

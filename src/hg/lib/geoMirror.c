/* geoMirror - support geographic based mirroring (e.g. euronode) */

#include "common.h"
#include "geoMirror.h"
#include "hgConfig.h"

boolean geoMirrorEnabled()
{
// return TRUE if this site has geographic mirroring turned on
return geoMirrorNode() != NULL;
}

char *geoMirrorNode()
{
// return which geo mirror node this is (or NULL if geo mirroring is turned off)
return cfgOption("browser.node");
}

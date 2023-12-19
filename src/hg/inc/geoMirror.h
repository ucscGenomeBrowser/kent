/* Code to support geographic mirrors (e.g. euro and asia nodes) */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef GEOMIRROR_H
#define GEOMIRROR_H

#include "hdb.h"

boolean geoMirrorEnabled();
// return TRUE if this site has geographic mirroring turned on

char *geoMirrorNode();
// return which geo mirror node this is (or NULL if geo mirroring is turned off)

char *geoMirrorCountry6(struct sqlConnection *centralConn, char *ipStr);
/* Return 2 letter country code for given IP. user has already checked table geoIpCountry6 exists.
 * Return error string otherwise. Free the response string. */

int geoMirrorDefaultNode(struct sqlConnection *centralConn, char *ipStr);
// return default node for given IP

char *geoMirrorMenu();
/* Create customized geoMirror menu string for substitution of  into 
 * <!-- OPTIONAL_MIRROR_MENU --> in htdocs/inc/globalNavBar.inc 
 * Reads hgcentral geo tables and hg.conf settings. 
 * Free the returned string when done. */

#endif /* GEOMIRROR_H */

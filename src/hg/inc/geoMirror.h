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

struct slPair *geoMirrorThisNode();
/* Return this node (browser.node) as a single pair of name=shortLabel, val=domain, or NULL when
 * geo mirroring is off or gbNode has no row for it.  slPairFreeValsAndList when done. */

struct slPair *geoMirrorOtherNodes();
/* Return the other geo mirror nodes, as pairs of name=shortLabel, val=domain, ordered by node.
 * The node this CGI is running on (browser.node) is left out.  Returns NULL when geo mirroring
 * is off or this is the only node.  slPairFreeValsAndList when done. */

void geoMirrorNotifyOtherNodes(char *cgiName, struct slPair *cgiVars);
/* Best-effort: fire cgiVars (name=value) as a GET request at cgiName on every other geo mirror
 * node (per geoMirrorOtherNodes()).  No-ops if geo mirroring is off or this is the only node.
 * Adds no authentication of its own -- callers must put their own signed proof into cgiVars,
 * since the receiving CGI runs with no session/cart tying the request to a user.  A slow or
 * unreachable peer is logged with warn() and skipped; the caller's own action must already be
 * complete locally before this is called, since a peer being down must never fail the local
 * action. */

char *geoMirrorMenu();
/* Create customized geoMirror menu string for substitution of  into 
 * <!-- OPTIONAL_MIRROR_MENU --> in htdocs/inc/globalNavBar.inc 
 * Reads hgcentral geo tables and hg.conf settings. 
 * Free the returned string when done. */

#endif /* GEOMIRROR_H */

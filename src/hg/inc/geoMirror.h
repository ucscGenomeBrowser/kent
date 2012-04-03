/* Code to support geographic mirrors (e.g. euronode) */

#ifndef GEOMIRROR_H
#define GEOMIRROR_H

boolean geoMirrorEnabled();
// return TRUE if this site has geographic mirroring turned on

char *geoMirrorNode();
// return which geo mirror node this is (or NULL if geo mirroring is turned off)

#endif /* GEOMIRROR_H */

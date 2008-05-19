/* transMapClick - transMap click handling */
#ifndef TRANSMAPCLICK_H
#define TRANSMAPCLICK_H

void transMapClickHandler(struct trackDb *tdb, char *mappedId);
/* Handle click on a transMap tracks */

void transMapShowCdnaAli(struct trackDb *tdb, char *mappedId);
/* Show alignment for accession, mostly ripped off from htcCdnaAli */

#endif

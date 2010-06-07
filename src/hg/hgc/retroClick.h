/* retroClick - retro click handling */
#ifndef RETROCLICK_H
#define RETROCLICK_H

void retroClickHandler(struct trackDb *tdb, char *mappedId);
/* Handle click on a retro tracks */

void retroShowCdnaAli(char *mappedId);
/* Show alignment for accession, mostly ripped off from transMapShowCdnaAli */

#endif

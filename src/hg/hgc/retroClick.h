/* retroClick - retro click handling */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef RETROCLICK_H
#define RETROCLICK_H

void retroClickHandler(struct trackDb *tdb, char *mappedId);
/* Handle click on a retro tracks */

void retroShowCdnaAli(char *mappedId);
/* Show alignment for accession, mostly ripped off from transMapShowCdnaAli */

#endif

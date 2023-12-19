/* bigBedFind.h - Find things in big beds . */

/* Copyright (C) 2010 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef BIGBEDFIND_H
#define BIGBEDFIND_H
boolean findBigBedPosInTdb(struct cart *cart, char *db, struct trackDb *tdb, char *term, struct hgPositions *hgp, struct hgFindSpec *hfs, boolean measureTiming);
/* Find a position in a single trackDb entry */

boolean findBigBedPosInTdbList(struct cart *cart, char *db, struct trackDb *tdbList, char *term, struct hgPositions *hgp, struct hgFindSpec *hfs, boolean measureTiming);
/* find a term in a list of tracks which may include a bigBed */ 

struct trackDb *getSearchableBigBeds(struct trackDb *tdbList);
/* Given a list of tracks, return those that are searchable */

boolean isTdbSearchable(struct trackDb *tdb);
/* Check if a single tdb is searchable */
#endif /* BIGBEDFIND_H */

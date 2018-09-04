/* bigBedFind.h - Find things in big beds . */

/* Copyright (C) 2010 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef BIGBEDFIND_H
#define BIGBEDFIND_H
boolean findBigBedPosInTdbList(struct cart *cart, char *db, struct trackDb *tdbList, char *term, struct hgPositions *hgp, struct hgFindSpec *hfs);
/* find a term in a list of tracks which may include a bigBed */ 
#endif /* BIGBEDFIND_H */

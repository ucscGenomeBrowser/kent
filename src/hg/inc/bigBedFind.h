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

struct maneLookup *maneLookupOpen(char *db, struct trackDb **tdbList);
/* Open the "mane" bigGenePred track for db, if it exists, for repeated calls to
 * maneStatusForRegion.  Returns NULL if this assembly has no mane track (e.g. non-human,
 * or hg19) -- callers should treat that as "can't tell", not an error.  tdbList is passed
 * through to tdbForTrack, so pass a pointer already shared with other tdbForTrack calls in
 * the same request to avoid a second trackDb load. */

char *maneStatusForRegion(struct maneLookup *ml, char *chrom, int start, int end,
                          struct slName *protAccList, char **retProtAcc);
/* Look for a MANE transcript overlapping chrom:start-end whose NCBI protein accession
 * (ignoring version suffix) matches one of protAccList.  Returns a cloned "MANE Select" /
 * "MANE Plus Clinical" string, or NULL if ml is NULL or there is no match.  On a match,
 * *retProtAcc is set to a cloned copy of the matching (versioned) NCBI protein accession,
 * so callers can identify the single MANE transcript out of a group of merged accessions. */

void maneLookupClose(struct maneLookup **pMl);
/* Close a maneLookup opened by maneLookupOpen. */
#endif /* BIGBEDFIND_H */

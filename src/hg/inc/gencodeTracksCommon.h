/* Copyright (C) 2022 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* Common functions for accessing All GENCODE (hgc-based) tracks */

#ifndef gencodeTracksCommon_h
#define gencodeTracksCommon_h
struct trackDb;

char *gencodeBaseAcc(char *acc, char *accBuf, int accBufSize);
/* get the accession with version number dropped. */

char* gencodeGetVersion(struct trackDb *tdb);
/* get the GENCODE version for a track. */

char *gencodeGetTableName(struct trackDb *tdb, char *tableBase);
/* Return the table name with the version attached.  This just leaks the memory
 * and lets exit() clean up.  It is tiny */

#endif

/* hReplaceGbdb - a stand-in for a needed function from hdb.h.
 *
 * The duplication from hdb.h is undesirable, but including hdb.h directly results
 * in incorporating a number of hard-coded strings that g++ then complains are
 * string constants instead of char *.
 */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

char *hReplaceGbdb(char* fileName);
/* clone and change a filename that can be located in /gbdb to somewhere else
 * according to hg.conf's "gbdbLoc1" and "gbdbLoc2". Result has to be freed. */

void freeMem(void *pt);
/* Free memory will check for null before freeing. */

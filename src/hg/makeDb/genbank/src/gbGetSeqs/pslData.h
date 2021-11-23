/* Get mRNA/EST alignment data */

/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef PSLDATA_H
#define PSLDATA_H

struct gbSelect;

void pslDataOpen(char* getWhat, boolean inclVersion, char *outFile);
/* open output file and set options. getWhat is psl or intronPsl */

void pslDataProcessUpdate(struct gbSelect* select);
/* Get PSL alignments for a partition and update.  Partition processed and
 * aligned indexes should be loaded and selected versions flaged. */

void pslDataClose();
/* close the output file */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


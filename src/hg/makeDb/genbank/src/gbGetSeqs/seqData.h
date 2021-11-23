/* Get mRNA/EST sequence data */

/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef SEQDATA_H
#define SEQDATA_H

struct gbSelect;

void seqDataOpen(boolean inclVersion, char *outFile);
/* open output file and set options */

void seqDataProcessUpdate(struct gbSelect* select);
/* Get sequences for a partition and update.  Partition processed index should
 * be loaded and selected versions flaged. */

void seqDataClose();
/* close the output file */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


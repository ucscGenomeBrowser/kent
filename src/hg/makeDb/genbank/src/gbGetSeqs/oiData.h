/* Get mRNA/EST orientation information */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef OIDATA_H
#define OIDATA_H

struct gbSelect;

void oiDataOpen(boolean inclVersion, char *outFile);
/* open output file and set options. */

void oiDataProcessUpdate(struct gbSelect* select);
/* Get orientationInfo records for a partition and update.  Partition
 * processed and aligned indexes should be loaded and selected versions
 * flaged. */

void oiDataClose();
/* close the output file */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


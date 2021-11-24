/* Get RefSeq peptides using ra and pep.fa files */

/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef PEPDATA_H
#define PEPDATA_H

struct gbSelect;

void pepDataOpen(boolean inclVersion, char *outFile);
/* open output file and set options */

void pepDataProcessUpdate(struct gbSelect* select, struct hash *idHash);
/* Get sequences for a partition and update.  Partition process and
 * aligned index should be loaded and newest versions flaged.  These
 * will be extracted if idHash NULL. Otherwise idHash has hash of
 * ids and optional versions, for either RefSeq mRNA or peptides. */

void pepDataClose();
/* close the output file */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


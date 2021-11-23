/* refPepRepair - repair refseq peptide links */

/* Copyright (C) 2006 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef REFPEPREPAIR_H
#define REFPEPREPAIR_H

void refPepList(char *db,
                FILE* outFh);
/* list of sequences needing repair */

void refPepRepair(char *db,
                  char *accFile,
                  boolean dryRun);
/* fix dangling repPep gbSeq entries. */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


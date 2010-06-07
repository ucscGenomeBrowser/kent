/* refPepRepair - repair refseq peptide links */
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


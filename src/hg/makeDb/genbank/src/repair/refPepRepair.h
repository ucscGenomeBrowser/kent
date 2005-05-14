/* refPepRepair - repair refseq peptide links */
#ifndef REFPEPREPAIR_H
#define REFPEPREPAIR_H

void refPepList(struct sqlConnection *conn,
                FILE* outFh);
/* list of sequences needing repair */

void refPepRepair(struct sqlConnection *conn,
                  boolean dryRun);
/* fix dangling repPep gbSeq entries. */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */


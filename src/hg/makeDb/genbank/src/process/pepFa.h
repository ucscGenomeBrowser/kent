/* copying the peptide fa and save info to store in ra */
#ifndef PEPFA_H
#define PEPFA_H
struct kvt;

void pepFaProcess(char* inPepFa, char* outPepFa);
/* process the peptide fa, build hash of seqs to fill in ra, copying only the
 * NP_* sequence with references to NM_*.
 */

void pepFaGetInfo(char* pepAcc, struct kvt* kvt);
/* Add peptide info to the kvt for the accession, if it exists */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

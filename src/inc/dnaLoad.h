/* dnaLoad - Load dna from a variaty of file formats. */

#ifndef DNALOAD_H
#define DNALOAD_H

struct dnaLoad;		/* Structure we keep private. */

struct dnaLoad *dnaLoadOpen(char *fileName);
/* Return new DNA loader.  Call dnaLoadNext() on this until
 * you get a NULL return, then dnaLoadClose(). */

struct dnaSeq *dnaLoadNext(struct dnaLoad *dl);
/* Return next dna sequence. */

int dnaLoadCurStart(struct dnaLoad *dl);
/* Returns the start offset of current sequence within a larger
 * sequence.  Useful for programs that want to auto-lift
 * nib and 2bit fragments.  Please call only after a
 * sucessful dnaLoadNext. */

int dnaLoadCurEnd(struct dnaLoad *dl);
/* Returns the end offset of current sequence within a larger
 * sequence.  Useful for programs that want to auto-lift
 * nib and 2bit fragments.  Please call only after a
 * sucessful dnaLoadNext. */

int dnaLoadCurSize(struct dnaLoad *dl);
/* Returns the size of the parent sequence.  Useful for
 * auto-lift programs.  Please call only after dnaLoadNext. */

void dnaLoadClose(struct dnaLoad **pDl);
/* Free up resources associated with dnaLoad. */

struct dnaSeq *dnaLoadAll(char *fileName);
/* Return list of all DNA referenced in file.  File
 * can be either a single fasta file, a single .2bit
 * file, a .nib file, or a text file containing
 * a list of the above files. DNA is mixed case. */

#endif /* DNALOAD_H */

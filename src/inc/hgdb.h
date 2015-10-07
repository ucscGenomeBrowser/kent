/* hgdb - Stuff to access human genome database. 
 *
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef HGDB_H
#define HGDB_H

char *hgdbRootDir();
/* Return root directory of human genome database. */

struct dnaSeq *hgdbRnaSeq(char *accession);
/* Return mRNA or EST sequence from an accession number. */

struct fof *hgdbRnaFof();
/* Return index for RNA sequences. */

struct dnaSeq *hgdbShortSeq(char *accession);
/* Return mRNA, EST, BACend or STS sequence based on
 * accession number. */

struct fof *hgdbShortFof();
/* Return index for short sequences. */

struct dnaSeq *hgdbFinishedSeq(char *accession);
/* Return finished BAC sequence. */

struct dnaSeq *hgdbUnfinishedSeq(char *accession);
/* Return unfinished BAC sequence. May be in 
 * several contigs (dnaSeq returned may be a list.) */

struct dnaSeq *hgdbGetSeq(char *accession);
/* Return sequence from any source. */

char *hgdbKeyText(char *accession);
/* Get key-value lines about accession number. */

struct fof *hgdbKeyFof();
/* Return index for key-values indexed by accession. */

boolean hgdbSmallKey(char *accession, char *key, char *valBuf, int valBufSize);
/* Get value of small key.  Returns FALSE if key doesn't exist. */

#endif /* HGDB_H */


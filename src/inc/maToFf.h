#ifndef MATOFF_H
#define MATOFF_H

/* Convert between mrnaAli and ffAli representations of an alignment. 
 *
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */


struct mrnaAli *ffToMa(struct ffAli *ffLeft, 
	struct dnaSeq *mrnaSeq, char *mrnaAcc,
	struct dnaSeq *genoSeq, char *genoAcc, 
	boolean isRc, boolean isEst);
/* Convert ffAli structure to mrnaAli. */

struct ffAli *maToFf(struct mrnaAli *ma, DNA *needle, DNA *haystack);
/* Convert from database to internal representation of alignment. */

#endif /* MATOFF_H */


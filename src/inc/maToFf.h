/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
#ifndef MATOFF_H
#define MATOFF_H

/* Convert between mrnaAli and ffAli representations of an alignment. */

struct mrnaAli *ffToMa(struct ffAli *ffLeft, 
	struct dnaSeq *mrnaSeq, char *mrnaAcc,
	struct dnaSeq *genoSeq, char *genoAcc, 
	boolean isRc, boolean isEst);
/* Convert ffAli structure to mrnaAli. */

struct ffAli *maToFf(struct mrnaAli *ma, DNA *needle, DNA *haystack);
/* Convert from database to internal representation of alignment. */

#endif /* MATOFF_H */


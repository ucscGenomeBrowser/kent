/* Copyright (C) 2003 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef AXTLIB_H
#define AXTLIB_H

void axtListReverse(struct axt **axtList, char *queryDb);
/* reverse complement an entire axtList */

struct axt *createAxtGap(char *nibFile, char *chrom, 	
			 int start, int end, char strand);
/* return an axt alignment with the query all deletes - null aligment */

void axtFillGap(struct axt **aList, char *nibDir, char direction);
/* fill gaps between blocks with null axts with seq on t and q seq all gaps*/
/* direction = '+' ascending on + strand, - is descending on - strand */

char *getAxtFileName(char *chrom, char *toDb, char *alignment, char *fromDb);
/* return file name for a axt alignment */

struct axt *netFillToAxt(struct cnFill *fill, struct dnaSeq *tChrom, int tSize,
	struct hash *qChromHash, char *nibDir,
	struct chain *chain, boolean swap);
/* Convert subset of chain as defined by fill to axt. swap query and target if swap is true.*/
#endif

/* chainToAxt - convert from chain to axt format. */

#ifndef CHAINTOAXT_H
#define CHAINTOAXT_H

struct axt *chainToAxt(struct chain *chain, 
	struct dnaSeq *qSeq, int qOffset,
	struct dnaSeq *tSeq, int tOffset, int maxGap, int maxChain);
/* Convert a chain to a list of axt's.  This will break
 * where there is a double-sided gap in chain, or 
 * where there is a single-sided gap greater than maxGap, or 
 * where there is a chain longer than maxChain.
 */

#endif /* CHAINTOAXT_H */

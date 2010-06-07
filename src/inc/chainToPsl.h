/* chainToPsl - convert between chains and psl.  Both of these
 * are alignment formats that can handle gaps in both strands
 * and do not include the sequence itself. */

#ifndef CHAINTOPSL_H
#define CHAINTOPSL_H

#ifndef PSL_H
#include "psl.h"
#endif

#ifndef CHAINBLOCK_H
#include "chainBlock.h"
#endif

struct psl *chainToPsl(struct chain *chain);
/* chainToPsl - convert chain to psl.  This does not fill in
 * the match, repMatch, mismatch, and N fields since it needs
 * the sequence for that.  It does fill in the rest though. */

struct psl *chainToFullPsl(struct chain *chain, 
	struct dnaSeq *query,   /* Forward query sequence. */
	struct dnaSeq *rQuery,	/* Reverse complemented query sequence. */
	struct dnaSeq *target);
/* Convert chainList to pslList, filling in matches, N's etc. */

#endif /* CHAINTOPSL_H */


/* bandExt - banded Smith-Waterman extension of alignments. 
 * An aligner might first find perfectly matching hits of
 * a small size, then extend these hits as far as possible
 * while the sequences perfectly match, then call on routines
 * in this module to do further extensions allowing small
 * gaps and mismatches. */

#ifndef BANDEXT_H
#define BANDEXT_H

#ifndef LOCALMEM_H
#include "localmem.h"
#endif

boolean bandExt(boolean global, struct axtScoreScheme *ss, int maxInsert,
	char *aStart, int aSize, char *bStart, int bSize, int dir,
	int symAlloc, int *retSymCount, char *retSymA, char *retSymB, 
	int *retRevStartA, int *retRevStartB);
/* Try to extend an alignment from aStart/bStart onwards.
 * If global is set it will always go to end (aStart+aSize-1,
 * bStart+bSize-1).  Set maxInsert to the maximum gap size allowed.  
 * 3 is often a good choice.  aStart/aSize bStart/bSize describe the
 * sequence to extend through (not including any of the existing
 * alignment. Set dir = 1 for forward extension, dir = -1 for backwards. 
 * retSymA and retSymB should point to arrays of characters of
 * symAlloc size.  symAlloc needs to be aSize*2 or so.  The
 * alignment is returned in the various ret values.  The function
 * overall returns TRUE if an extension occurred, FALSE if not. */

struct ffAli *bandExtFf(
	struct lm *lm,	/* Local memory pool, NULL to use global allocation for ff */
	struct axtScoreScheme *ss, 	/* Scoring scheme to use. */
	int maxInsert,			/* Maximum number of inserts to allow. */
	struct ffAli *origFf,		/* Alignment block to extend. */
	char *nStart, char *nEnd,	/* Bounds of region to extend through */
	char *hStart, char *hEnd,	/* Bounds of region to extend through */
	int dir,			/* +1 to extend end, -1 to extend start */
	int maxExt);			/* Maximum length of extension. */
/* Extend a gapless alignment in one direction.  Returns extending
 * ffAlis, not linked into origFf, or NULL if no extension possible. */

#endif /* BANDEXT_H */

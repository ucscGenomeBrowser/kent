#ifndef AXTLIB_H
#define AXTLIB_H

struct axt *convertFill(struct cnFill *fill, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *nibDir,
	struct chain *chain, FILE *gapFile);
/* Convert subset of chain as defined by fill to axt. */
#endif

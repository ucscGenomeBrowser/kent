#ifndef AXTLIB_H
#define AXTLIB_H

struct axt *convertFill(struct cnFill *fill, struct dnaSeq *tChrom,
	struct hash *qChromHash, char *nibDir,
	struct chain *chain, FILE *gapFile);
/* Convert subset of chain as defined by fill to axt. */

void axtListReverse(struct axt **axtList, char *queryDb);
/* reverse complement an entire axtList */

struct axt *createAxtGap(char *nibFile, char *chrom, 	
			 int start, int end, char strand);
/* return an axt alignment with the query all deletes - null aligment */

void axtFillGap(struct axt **aList, char *nib);
/* fill gaps between blocks with null axts with seq on t and q seq all gaps*/

char *getAxtFileName(char *chrom, char *toDb, char *alignment, char *fromDb);
/* return file name for a axt alignment */

#endif

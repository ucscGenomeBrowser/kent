/* hgSeq - Fetch and format DNA sequence data (to stdout) for gene records. */

#ifndef HGSEQ_H
#define HGSEQ_H

void hgSeqOptionsDb(char *db, char *table);
/* Print out HTML FORM entries for gene region and sequence display options. */

void hgSeqOptions(char *table);
/* Print out HTML FORM entries for gene region and sequence display options. */

void hgSeqItemsInRangeDb(char *db, char *table, char *chrom, int chromStart,
			 int chromEnd, char *sqlConstraints);
/* Print out dna sequence of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table.  */

void hgSeqItemsInRange(char *table, char *chrom, int chromStart, int chromEnd,
		       char *sqlConstraints);
/* Print out dna sequence of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table.  */

void hgSeqRange(char *chrom, int chromStart, int chromEnd, char strand,
		char *name);
/* Print out dna sequence of the given range. */

#endif /* HGSEQ_H */

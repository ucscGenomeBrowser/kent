/* hgSeq - Fetch and format DNA sequence data (to stdout) for gene records. */

#ifndef HGSEQ_H
#define HGSEQ_H

#include "hdb.h"
#include "cart.h"
#include "bed.h"

#ifdef NEVER
void hgSeqOptionsHti(struct hTableInfo *hti);
/* Print out HTML FORM entries for gene region and sequence display options.
 * Use defaults from CGI. */
#endif /* NEVER */

void hgSeqOptionsHtiCart(struct hTableInfo *hti, struct cart *cart);
/* Print out HTML FORM entries for gene region and sequence display options. 
 * Use defaults from CGI and cart. */

void hgSeqOptions(struct cart *cart, char *db, char *table);
/* Print out HTML FORM entries for gene region and sequence display options. */

int hgSeqItemsInRange(char *db, char *table, char *chrom, int chromStart,
			int chromEnd, char *sqlConstraints);
/* Print out dna sequence of all items (that match sqlConstraints, if nonNULL) 
   in the given range in table.  Return the number of items. */

void hgSeqRange(char *db, char *chrom, int chromStart, int chromEnd, char strand,
		char *name);
/* Print out dna sequence of the given range. */

int hgSeqBed(char *db, struct hTableInfo *hti, struct bed *bedList);
/* Print out dna sequence from the current database of all items in bedList.  
 * hti describes the bed-compatibility level of bedList items.  
 * Returns number of FASTA records printed out. */

int hgSeqBedDb(char *db, struct hTableInfo *hti, struct bed *bedList);
/* Print out dna sequence from the given database of all items in bedList.  
 * hti describes the bed-compatibility level of bedList items.  
 * Returns number of FASTA records printed out. */

#endif /* HGSEQ_H */

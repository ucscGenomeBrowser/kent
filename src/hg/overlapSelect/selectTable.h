/* selectTable - module that contains ranges use to select */
#ifndef SELECT_TABLE_H
#define SELECT_TABLE_H

/* options to select fuctions */
#define SEL_EXCLUDE_SELF 0x01
#define SEL_USE_STRAND   0x02

void selectAddPsls(char* pslFile);
/* add records from a psl file to the select table */

void selectAddGenePreds(char* genePredFile);
/* add blocks from a genePred file to the select table */

void selectAddBeds(char* bedFile);
/* add records from a bed file to the select table */

boolean selectIsOverlapped(unsigned options, char *name, char* chrom, int start, int end, char strand);
/* determine if a range is overlapped considering strand */

#endif

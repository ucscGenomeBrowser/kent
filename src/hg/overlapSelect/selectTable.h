/* selectTable - module that contains ranges use to select */
#ifndef SELECT_TABLE_H
#define SELECT_TABLE_H

void selectAddPsls(char* pslFile);
/* add records from a psl file to the select table */

void selectAddGenePreds(char* genePredFile);
/* add blocks from a genePred file to the select table */

void selectAddBeds(char* bedFile);
/* add records from a bed file to the select table */

boolean selectOverlapsGenomic(char* chrom, int start, int end, char *dbgName);
/* determine if a range is overlapped without considering strand */

boolean selectOverlapsStrand(char* chrom, int start, int end, char strand, char *dbgName);
/* determine if a range is overlapped considering strand */

#endif

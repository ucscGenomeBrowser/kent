/* read_malign.h - header file for shared procedures to handle multiple
*  alignments */

/* Return a multiple alignment contained in a file.  The first line contains
*  two integers, namely the number of rows (nrow) followed by the number of
*  columns (ncol).  The next nrow lines contain the names of the sequences.
*  The last nrow lines contain the rows of the alignment, each being a
*  sequence of ncol characters from the set {A,C,G,T,-}.
*/
char **read_malign(char *filename, int *nrow_ptr, int *ncol_ptr,
  char ***seqname_ptr);

/* Returns arrays similar to read_malign but for fourfold degenerate sites in human coordinates... */
int **read_sites(char *filename, int *nrow_ptr, int *ncol_ptr,
		 char ***seqname_ptr, int **sites);

/* Return an array giving the position in the first sequence (position numbers
*  start with 1) for each alignment column (numbered 0, 1, ..., ncol-1).
*  If there is a dash at the top of column i, then the corresponding sequence
*  position is reported as 0.
*/
int *position_in_seq1(char **align, int ncol);


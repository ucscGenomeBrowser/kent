/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef TFLOCUTIL
#define TFLOCUTIL

#include "tfbsConsLoc.h"

/* subroutines for freeing up and allocating memory */
void free_char_arr (char **arr, int size);
void free_int_arr (int **arr, int size);
void free_match_list (struct MATCH **matches);
void free_motif_list (struct MOTIF **motif_head);
void allocate_submat (struct MOTIF **new_motif);

/* subroutines for printing various data structures */
void print5_int_arr (int *arr, int nrow);
void print_col_arr (int **col_arr, int num_motifs, int which_match);
void print10_int_arr (int *arr, int nrow);
void print_matches (struct MATCH *matches);

/* other utility subroutines */
struct MOTIF* copy_motif (struct MOTIF *new_motif);
void push_motif (struct MOTIF **motifs, struct MOTIF *new_motif);

#endif

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef TFLOC
#define TFLOC

#include "matt-maf.h"
#include "util.h"

/* array indices for each nucleotide */
#define rowA 0 
#define rowC 1
#define rowG 2
#define rowT 3

/* output modes */
#define VERBOSE 0
#define GALA 1
#define HGB 2

/* strands */
#define FORWARD 0
#define REVERSE 1

/* other constants */
#define BUFSIZE sizeof (char) * 100
#define STRSIZE sizeof (char) * 70
#define ACSIZE 7
#define MAXSUBMATLEN 50
#define DEFAULT_CUTOFF 0.85
#define DEFAULT_NROW 3
#define BAR "=================================================================="
#define SYNTAX "USAGE:  tfloc [-verbose | -gala | -hgb] maf_file transfac_file id [-order] [-cut CUTOFF] [-nseq NUM_SEQS_IN_ALIGNMENT] [-nm NUM_MISSES]"



/* one match in a list of matches to one motif */
struct MATCH
{
    int col;		/* its column in this alignment */
    int lowest_score;	/* lowest score out of all seqs in the alignment */
    int pos;		/* position of motif in human seq */
    int dir;		/* FORWARD or REVERSE */
    struct MATCH *next;	/* next match in the list */
};

/* one transcription factor binding site in a list of all in the pattern */
struct MOTIF
{
    int len;		/* length of motif */
    char *name;		/* name of motif */
    char *id;		/* motif id */
    float threshold;	/* threshold score to count as a hit */
    int min;		/* minimum possible score */
    int max;		/* maximum possible score */
    int **submat;	/* substitution matrix */
    struct MOTIF *next;	/* should be NULL */
};

void add_to_matches (struct MATCH **matches, int col, 
			    int seq, int *score, int nrow, struct mafAli *ali, int len, int dir, int do_order);
void get_args (int argc, char **argv, int *output_mode, struct mafFile **file, char **id,
                      struct MOTIF **motif, int *do_order, int *nrow, float *cutoff, int *num_misses);
int get_max_score( int **submat, int length);    
int get_min_score( int **submat, int length);    
int get_pos (int start, int col, char strand, int size, char *text, int motif_len, int dir);
int get_score (int **submat, char seq_char, int col);
void get_matches (struct MATCH **matches, int dir,
			 struct mafAli *ali, int nrow, struct MOTIF *motif,
			 int do_order, int num_misses);
void init_motif (struct MOTIF **motif, char *fileName, float cutoff, 
			char *id);
int motif_hit (char *text, int col, struct MOTIF *cur_motif, int *score);
void output_matches (struct MATCH *matches, char *strand, char *id, int nrow, struct MOTIF *motif);
void print_strand (const char *strand, int dir, int nrow);

#endif

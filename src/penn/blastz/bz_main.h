#ifndef BZ_MAIN_H
#define BZ_MAIN_H

#include "bz_all.h"

void *ckrealloc(void *, size_t);

typedef struct tback {
	uchar *space;		/* array to be used. */
	unsigned int len;	/* length of that array. */
} TB, *TBack;

typedef struct align_ {
        int beg1, beg2, end1, end2;
        struct align_ *next_align;
        edit_script_t *script;
	int score;
	uchar *seq1, *seq2;
} align_t;

uchar *reverse_seq(uchar *s, int len);
align_t *bz_extend_msps(SEQ *, uchar *, SEQ *, msp_table_t *, gap_scores_t *, ss_t,
  int, int, TBack);

/* ----------------- end of header information used by blastz.c ------------- */

/* ----------- what follows is shared by bz_extend.c and bz_align.c ----------*/

/* block types: */
#define DIA 0
#define HOR 1
#define VER 2

/* Alignment segments */
typedef struct bl {
	int b1, b2, e1, e2;
	char type;
	struct bl *next_block, *prev_block;
} block, *block_ptr;

/* Expanded structure for each MSP */
typedef struct msp1 {
	int
	  pos1, pos2,	/* initially, midpoints of best 31-mer, .. */
			/* then start-points of the seeded alignment */
	  end1, end2;	/* end-points of alignments seeded by the MSP */

	/* We keep two forms of the aligment seeded by this MSP.  The first
	*  is a linked list of diagonal, horizontal and vertical segments. */
	block_ptr first_block, last_block; /* is last_block needed? */

	/* The second form of the alignment is for external consumption. */
	align_t *align;

	/* At the alignment's beginning and ending points, we keep pointers
	*  to the alignments and alignment blocks immediately above and below
	*  the terminus. */
	struct msp1 *up_align1, *down_align1, *up_align2, *down_align2;
	block_ptr up_block1, down_block1, up_block2, down_block2;

	/* Alignments are linked both by increasing start-point and by
	*  decreasing end-point. */
	struct msp1 *next_msp, *prev_msp;
} Msp_node, *Msp_ptr;

/* Structure holding input and output for Align().  */
typedef struct _gapalign_blk {
	/* The following must be supplied by caller.  */
	uchar 	*seq1,		/* the query sequence. */
		*seq2,		/* the subject sequence. */
		*seq1_r,	/* reverse of query */
		*seq2_r;  	/* reverse of subject*/
	int	len1,		/* the length of the query. */
		len2,		/* the subject length. */
		seed1,		/* query position to start the alignment. */
		seed2,		/* subject position to start the alignment.*/ 
		gap_open,	/* cost to open a gap. */
		gap_extend,	/* cost to extend a gap. */
		x_parameter;	/* values of X-dropoff parameter. */
	Msp_ptr Up_align,	/* closest alignment above (right of) seed pt */
		Down_align,	/* closest alignment below (left of) seed pt */
		Right_list,	/* alignments starting after the seed point */
		Left_list;	/* alignments ending before the seed point */
	block_ptr
		Up_block,	/* specific block of Up_align */
		Down_block;	/* specific block of Down_align */
	int	**subscore;	/* nucleotide substitution scores. */
	TBack tback;		/* block of memory for alignment trace-back */

	/* The following are returned by Align(). */
	int	score, 		/* score of alignment. */
		start1,		/* start of alignment on query. */
		start2,		/* start of alignment on subject. */
		stop1,		/* end of alignment on query. */
		stop2;		/* end of alignment on subject. */
        edit_script_t *ed_scr;	/* alignment's edit script */
} AlignIO, *AlignIOPtr;

void Align(AlignIOPtr);
#endif /* BZ_MAIN_H */

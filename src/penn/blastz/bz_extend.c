static const char rcsid[]=
"$Id: bz_extend.c,v 1.1 2002/09/25 05:44:07 kent Exp $";

/* The main duties of procedures in this file are:
*	1. Given a set of MSPs, call Align() on each one.
*	2. Maintain a data structure that accumulates the alignments (one
*		per MSP).
*/

#include "util.h"
#include "seq.h"
#include "bz_all.h"
#include "bz_main.h"

/* ----------- procedures to initialize the extended MSP structures --------  */

/* For each MSP we need to keep more information (see blastz.h).  Here we
*  allocate a larger node for each MSP and populate the most basic information.
*  The point (pos1,pos2) is set to the MSP's seed-point for a gapped alignment.
*/

/* find_best_31 - shifts (pos1,pos2) to the center of the MSP's best 31-mer */
static void find_best_31(Msp_ptr mp, uchar *seq1, uchar *seq2, int len, ss_t ss)
{
	int i, sum, best, shift;
	uchar *s, *q;

	if (len > 31) {
		q = seq1 + mp->pos1; 
		s = seq2 + mp->pos2;
		for (sum = i = 0; i < 31; ++i)
			sum += ss[*s++][*q++];
		shift = 15;
		best = sum;
		for (i = 31; i < len; i++) {
			sum -= ss[*(s-31)][*(q-31)];
			sum += ss[*s++][*q++];
			if (sum > best) {
				best = sum;
				shift = i - 15;
			}
		}
	} else
		shift = len/2;

	mp->pos1 += shift;
	mp->pos2 += shift;
}

static Msp_ptr *augment_msp_table(msp_table_t *mt, SEQ *sf1, SEQ *sf2, ss_t ss)
{
	int numMSPs, i;
	Msp_ptr *msp, mp;
	msp_t *mst;

	numMSPs = MSP_TAB_NUM(mt);
	msp = ckallocz(sizeof(Msp_ptr)*(numMSPs+1));
		/* (the extra place may be used for a trivial self-alignment) */
	msp[0] = ckallocz(sizeof(Msp_node)*(numMSPs+1));
	for (i = 1; i <= numMSPs; i++) 
		msp[i] = msp[i-1]+1;
	for (i = 0; i < numMSPs; i++) {
		mp = msp[i];
		mst = &(MSP_TAB_NTH(mt,i));
		mp->pos1 = mst->pos1;
		mp->pos2 = mst->pos2;
		find_best_31(mp, SEQ_CHARS(sf1), SEQ_CHARS(sf2), mst->len, ss);
	}
	return msp;
}

#define HUGE_INT 100000000

/* ------------- procedures to process the two lists of alignments ---------- */

/* msp_updown -- given MSP m, we search obi, the list of alignments ordered
*  by increasing beginning point, to find the alignments and their contained
*  blocks (gap-free pieces) that are the closest above and below the MSP's
*  seed-point.  0 is returned if the seed-point is in an already-computed
*  alignment.
*/
static int msp_updown(Msp_ptr obi, Msp_ptr m)
{
	int up = HUGE_INT, down = HUGE_INT, a, p1 = m->pos1, p2 = m->pos2;
	block_ptr bp, bup, bdown;
	Msp_ptr mup, mdown;

	bup = bdown = NULL;
	mup = mdown = NULL;

	for ( ; obi && obi->pos1 <= p1; obi = obi->next_msp) {
		if (obi->end1 < p1)
			continue;
		for (bp = obi->first_block; bp; bp = bp->next_block) 
			if (bp->e1 >= p1)
				break;
		if (bp == NULL)
			continue;	/* CAN HAPPEN, BUT HOW? */
		if (bp->type == HOR)
			fatal("msp_updown: cannot be horizontal.");
		if (bp->type == DIA) 
			a = p2 - (bp->b2 + p1 - bp->b1);
		else
			a = p2 - bp->b2;
		if (a == 0)
			return 0;
		else if (a > 0 && down > a) {
			down = a;
			bdown = bp;
			mdown = obi;
		} else if (a < 0 && up > -a) {
			up = -a;
			bup = bp;
			mup = obi;
		}
	}
	m->up_block1 = m->up_block2 = bup;
	m->up_align1 =  m->up_align2 = mup;
	m->down_block1 = m->down_block2 = bdown;
	m->down_align1 = m->down_align2 = mdown;
	return 1;
}

/* align_updown -- given a new alignment, determine the alignments and their
*  blocks that are the closest in either direction at both ends of the
*  alignment.
*/
static void align_updown(Msp_ptr obi, Msp_ptr mptr)
{
	int left_up = HUGE_INT, right_up = HUGE_INT, left_down = HUGE_INT,
	  right_down = HUGE_INT, a;
	block_ptr bp, up_left, up_right, down_left, down_right;
	Msp_ptr up_align_left, up_align_right, down_align_left, down_align_right;

	up_align_left = down_align_left = up_align_right = down_align_right = NULL;
	up_left = up_right = down_left = down_right = NULL;
	for ( ; obi; obi = obi->next_msp) {
		if (obi->pos1 <= mptr->end1 && obi->end1 >= mptr->pos1) {
			for (bp = obi->first_block; bp; bp = bp->next_block) 
				if (bp->type != HOR && bp->e1 >= mptr->pos1)
					break;
			if (bp && bp->b1 <= mptr->pos1) {
				if (bp->type == DIA) 
					a = mptr->pos2 -
					  (bp->b2 + mptr->pos1 - bp->b1);
				else
					a = mptr->pos2 - bp->b2;
				if (a > 0 && left_down > a) {
					left_down = a;
					down_left = bp;
					down_align_left = obi;
				} else if (a < 0 && left_up > -a) {
					left_up = -a;
					up_left = bp;
					up_align_left = obi;
				}
			}
/* DO WE NEED THESE CHECKS FOR BP->TYPE != HOR) ???? */
			for ( ; bp; bp = bp->next_block) 
				if (bp->type != HOR && bp->e1 >= mptr->end1)
					break;
			if (bp && bp->type != HOR &&  bp->e1 >= mptr->end1) {
				if (bp->type == DIA) 
					a = mptr->end2 -
					  (bp->b2 + mptr->end1 - bp->b1);
				else
					a = mptr->end2 - bp->b2;
				if (a > 0 && right_down > a) {
					right_down = a;
					down_right = bp;
					down_align_right = obi;
				} else if (a < 0 && right_up > -a) {
					right_up = -a;
					up_right = bp;
					up_align_right = obi;
				}
			}
		}
	}
	mptr->up_block1 = up_left;
	mptr->up_align1 = up_align_left;
	mptr->up_block2 = up_right;
	mptr->up_align2 = up_align_right;
	mptr->down_block1 = down_left;
	mptr->down_align1 = down_align_left;
	mptr->down_block2 = down_right;
	mptr->down_align2 = down_align_right;
}

/* insert_align - insert a new alignment in the two lists of alignments,
*  one ordered by increasing beginning point and the other ordered by
*  decreasing end point.
*/
static void insert_align(Msp_ptr mptr, Msp_ptr *obi, Msp_ptr *oed)
{
	Msp_ptr mp, mq;
	Msp_ptr order_beg_inc=*obi, order_end_dec=*oed;

	if (mptr->first_block == NULL)
		fatal("insert_align: null first block.");
	for (mq = NULL, mp = order_beg_inc; mp; mq = mp, mp = mp->next_msp)
		if (mp->pos1 >= mptr->pos1)
			break;
	if (mq) {
		mq->next_msp = mptr;
		mptr->next_msp = mp;
	} else {
		mptr->next_msp = order_beg_inc;
		order_beg_inc = mptr;
	}

	for (mq = NULL, mp = order_end_dec; mp; mq = mp, mp = mp->prev_msp)
		if (mp->end1 <= mptr->end1)
			break;
	if (mq) {
		mq->prev_msp = mptr;
		mptr->prev_msp = mp;
	} else {
		mptr->prev_msp = order_end_dec;
		order_end_dec = mptr;
	}
	*obi = order_beg_inc; *oed = order_end_dec;
}

/* get_closest - in preparation for extending the seedpoint of an MSP, find
*  the closest alignment ending before the seedpoint and the closest
*  alignment starting after the seedpoint.
*/
static void get_closest(int b1, AlignIOPtr align_IO, Msp_ptr order_beg_inc,
  Msp_ptr order_end_dec)
{
	Msp_ptr mp;

	for (mp = order_end_dec; mp; mp = mp->prev_msp)
		if (mp->end1 < b1)
			break;
	align_IO->Left_list = mp;
	for (mp = order_beg_inc; mp; mp = mp->next_msp)
		if (mp->pos1 > b1)
			break;
	align_IO->Right_list = mp;
}

/* ------------------ procedures to process a new alignment  ---------------- */

/* save the internal form of a gap-free block */
static void insert_to_tail(Msp_ptr mp, block_ptr bp)
{
	bp->prev_block = mp->first_block->prev_block;
	bp->next_block = mp->first_block;
	mp->first_block->prev_block->next_block = bp;
	mp->first_block->prev_block = bp;
}

/* save_block - add a gap-free piece of alignment at the end of the list kept
*  with the seeding MSP.  If appropriate, insert a horizontal or vertical
*  piece before it.
*/
static void save_block(int b1, int b2, int e1, int e2, Msp_ptr mptr)
{
	block_ptr bq, bp = (block_ptr) ckalloc(sizeof(block));

	bp->b1 = b1; bp->b2 = b2; bp->e1= e1; bp->e2 = e2;
	bp->type = DIA;
	if (mptr->first_block) {
		bq = (block_ptr) ckalloc(sizeof(block));
		bq->type =
		  ((b1 == mptr->first_block->prev_block->e1 + 1) ? HOR : VER);
		bq->b1 = mptr->first_block->prev_block->e1 + 1;
		bq->b2 = mptr->first_block->prev_block->e2 + 1;
		bq->e1 = b1 - 1; bq->e2 = b2 - 1;
		insert_to_tail(mptr, bq);
		insert_to_tail(mptr, bp);
	} else
		mptr->first_block = bp->prev_block = bp->next_block = bp;
}

/* format_alignment - process the edit script for a newly computed alignment
*  by (1) storing a linked-list form with the seeding MSP and (2) augmenting
*  the edit script into a form suitable for returning to the calling program.
*/
static align_t *format_alignment(AlignIOPtr align_IO, Msp_ptr mptr) 
{
        int beg1, end1, beg2, end2, M, N, i, j, opn, not_used, start_i,
	  start_j, run;
	edit_script_t *S;
	uchar *seq1, *seq2;
	align_t *a;

	beg1 = align_IO->start1 + 1;  
	end1 = align_IO->stop1 + 1;
	beg2 = align_IO->start2 + 1;
	end2 = align_IO->stop2 + 1;
	S = align_IO->ed_scr;
	seq1 = align_IO->seq1;
	seq2 = align_IO->seq2;

	M = end1 - beg1 + 1;
	N = end2 - beg2 + 1;

	for (opn = i = j = 0; i < M || j < N; ) {
		start_i = i;
		start_j = j;

                run = es_rep_len(S, &opn, seq1+beg1+i-1, seq2+beg2+j-1,
		  &not_used);
                i += run; j += run; 
		save_block(beg1+start_i-1, beg2+start_j-1,
				  beg1+i-2, beg2+j-2, mptr);
                if (i < M || j < N)
                  es_indel_len(S, &opn, &i, &j);
	
    	}
	a = ckalloc(sizeof(align_t));
	a->script = S;
	a->beg1 = beg1; a->beg2 = beg2; 
	a->end1 = end1; a->end2 = end2;
	a->seq1 = seq1; a->seq2 = seq2;
	a->score = align_IO->score;
	a->next_align = NULL;
	return a;
}

uchar *reverse_seq(uchar *s, int len)
{
	uchar *p = ckalloc(len+1), *t;

	for (t = p + len - 1; t >= p; --t)
		*t = *s++;
	p[len] = '\0';
	return p;
}


align_t *bz_extend_msps(SEQ *sf1, uchar *rf1, SEQ *sf2, msp_table_t *mt,
  gap_scores_t *ds, ss_t ss, int Y, int K, TBack tback)
{
	int i, scor, *sp[NACHARS], numMSPs = MSP_TAB_NUM(mt);
	AlignIOPtr align_IO;
	Msp_ptr mp, *msp, mptr, order_beg_inc = NULL,
	  order_end_dec = NULL;
	align_t *head, *last;
	block_ptr bp, bq;

	mt = msp_sort(mt);	/* sort by decreasing score */
	msp = augment_msp_table(mt, sf1, sf2, ss);

	align_IO = ckalloc(sizeof(*align_IO));
	for (i = 0; i < NACHARS; i++)
		sp[i] = ss[i];

	align_IO->subscore = sp;
	align_IO->len2 = SEQ_LEN(sf2);
	align_IO->len1 = SEQ_LEN(sf1);
	align_IO->gap_open = ds->O;
	align_IO->gap_extend = ds->E;
	align_IO->x_parameter = Y;
	align_IO->seq1 = SEQ_CHARS(sf1);
	align_IO->seq2 = SEQ_CHARS(sf2);

	align_IO->seq1_r = rf1;
	/* align_IO->seq1_r = reverse_seq(SEQ_CHARS(sf1), SEQ_LEN(sf1)); */
	align_IO->seq2_r = reverse_seq(SEQ_CHARS(sf2), SEQ_LEN(sf2));

	if (tback == NULL)
		fatal("bz_extend_msps got a NULL tback pointer.");
	align_IO->tback = tback;

	/* Special case: trivial alignment in a self-comparison. */
	if (SEQ_LEN(sf1) == SEQ_LEN(sf2)) {
		uchar *s = SEQ_CHARS(sf1), *t = SEQ_CHARS(sf2), S, T;
		for (scor = i = 0; i < SEQ_LEN(sf1); ++i) {
			S = toupper(s[i]);
			T = toupper(t[i]);
			scor += ss[S][T];
			if (S != T)
				break;
		}
		if (i == SEQ_LEN(sf1)) {   /* seq1 and seq2 are identical */
			align_t *a;
			mp = msp[numMSPs];
			mp->pos1 =  mp->pos2 = 0;
			mp->end1 = mp->end2 = SEQ_LEN(sf1)-1;
			mp->up_align1 = mp->up_align2 = mp->down_align1 =
			  mp->down_align2 = NULL;
			mp->up_block1 = mp->up_block2 = mp->down_block1 =
			  mp->down_block2 = NULL;
			mp->first_block = NULL;
			save_block(0, 0, mp->end1, mp->end2, msp[numMSPs]);
			insert_align(mp, &order_beg_inc, &order_end_dec);
			mp->last_block = mp->first_block;
			mp->first_block->prev_block =
			  mp->last_block->next_block = NULL;
			a = mp->align = ckalloc(sizeof(align_t));
			a->script = edit_script_new();
			(void)edit_script_rep(a->script, SEQ_LEN(sf1));
			a->beg1 = a->beg2 = 1;
			a->end1 = a->end2 = SEQ_LEN(sf1);
			a->seq1 = a->seq2 = SEQ_CHARS(sf1);
			a->score = scor;
			a->next_align = NULL;
		}
	}

	/* Loop over the (non-trivial) MSPs. */
	for (i = 0; i < numMSPs; i++) {
		mptr = msp[i];
		if (!msp_updown(order_beg_inc, mptr))
			continue;
		align_IO->seed1 = mptr->pos1;
		align_IO->seed2 = mptr->pos2;
		get_closest(mptr->pos1, align_IO, order_beg_inc, order_end_dec);
		align_IO->Up_align = mptr->up_align1;
		align_IO->Down_align = mptr->down_align1;
		align_IO->Up_block = mptr->up_block1;
		align_IO->Down_block = mptr->down_block1;
		Align(align_IO);
		mptr->align = format_alignment(align_IO, mptr);
		mptr->pos1 = align_IO->start1;
		mptr->pos2 = align_IO->start2;
		mptr->end1 = align_IO->stop1;
		mptr->end2 = align_IO->stop2;
		if (mptr->first_block) { /* HOW CAN THIS BE FALSE??? */
			mptr->last_block = mptr->first_block->prev_block;
			mptr->first_block->prev_block =
			   mptr->last_block->next_block = NULL;
			align_updown(order_beg_inc, mptr);
			insert_align(mptr, &order_beg_inc, &order_end_dec);
		} 
	}

	/* Link the alignments for return to the calling procedure. */
	head = last = NULL;
	for (mp = order_beg_inc; mp; mp = mp->next_msp)
		if (mp->align->score >= K) {
			if (head == NULL)
				head = last = mp->align;
			else
				last = last->next_align = mp->align;
		}

	/* Clean up and return. */
	for (mp = order_beg_inc; mp; mp = mp->next_msp)
		for (bp = mp->first_block; bp; bp = bq) {
			bq = bp->next_block;
			ckfree(bp); 
		}
	ckfree(msp[0]);
	ckfree(msp);
	// not ours: ckfree(align_IO->seq1_r);
	ckfree(align_IO->seq2_r);
	ckfree(align_IO);
	return head;
}

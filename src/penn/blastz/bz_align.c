static const char rcsid[]=
"$Id: bz_align.c,v 1.1 2002/09/25 05:44:06 kent Exp $";

/* bz_align.c -- The only entry point is:
*
*  void Align(AlignIOPtr align_IO)
*
*  The X-drop variant of dynamic programming is applied to create a gapped
*  alignment by extending in both directions from a "seed point".
*
*  Procedures in this file are not allowed to modify any of the data structures
*  controlled by bz_extend.c, namely the Msp structures and the alignments and
*  blocks that they point to.
*/

/* WHEN ADDING TO THE BLIST, CAN WE PRUNE ALIGNMENTS OUTSIDE OF [L,R]? */

#include "bz_all.h"

#define MININT      -9999999
#define TRUE 1
#define FALSE 0
#define Boolean int

/* Dynamic Programming structure. */
typedef struct DP {
	int DD, CC, FF;	/* FF is unused, but it adds speed under linux */
	int mask;       /* mask out previously used points */ 
} *dp_ptr, dp_node;

typedef struct dyn_prog {
	dp_node *p;
	unsigned int len;
} dyn_prog;

/* List of blocks maintained herein. */
typedef struct _list {       
	block_ptr block;
	int x, /* column position where block intersects the sweep row */
	    last_row;
	char type, filter;
	struct _list *next;
} blist_node, *blist_ptr;

/* The trace-back information, i.e., to trace backwards along an optimal path,
*  is packed into one byte per dynamic-programming grid point.  At each grid
*  point there are three nodes: C which is entered by one diagonal edge,  D
*  which is entered by two vertical edges (from a D node and a C node),
*  and I which is entered by two horizontal edges.  If this is new to you,
*  please consult Myers and Miller, Bull. Math. Biol. 51 (1989), pp. 5-37.
*
*  The right-most two bits hold FLAG_C or FLAG_I or FLAG_D, telling which
*  node gets the maximum score from an entering edge.  FLAG_DD (third bit
*  from the right) is 1 iff it is better to take a vertical edge from the D
*  node than a vertical edge from the C node.  FLAG_II does the same for
*  horizontal edges.
*/

#define FLAG_C (0)
#define FLAG_I (1<<0)
#define FLAG_D (1<<1)
#define FLAG_DD (1<<2)
#define FLAG_II (1<<3)
#define SELECT_CID (FLAG_I | FLAG_D | FLAG_C)

#define PRUNE \
{\
    c = dp->CC + wa[*(b++)];\
    if (Lcol == col) Lcol++;\
    else {i = dq->DD = dq->CC = MININT; dq++;}\
    dp++; *(tbp++) = 0;\
    continue;\
}

/* ------------- allocate the dynamic-programming "sweep row" array --------- */

/* ensure that the first n elements exist and have been initialized */
static void dp_ready(dyn_prog *dp, int n)
{
	int old_len, added;

	if (dp == NULL)
		fatal("dp_ready called with NULL pointer");

	n = MAX(n,0);

	/* make sure that there is room for n of them */
	if (dp->p) {
		old_len = dp->len;
		if (n > old_len) {
			dp->len = dp->len/16 + n + 1000;
			dp->p = ckrealloc(dp->p, dp->len * sizeof(dp_node));
		}
	} else {
		old_len = 0;
		dp->len = n + 1000;
		dp->p = (dp_node *) ckalloc(dp->len * sizeof(dp_node));
	}

	/* 0..old_len-1 are already in use, but zero the rest */
	if ((added = dp->len - old_len) > 0)
		memset(dp->p + old_len, 0, sizeof(dp->p[0])*added);
	/* {int i; for (i = old_len; i < dp->len; ++i) dp->p[i].mask = 0;} */
}



/* ---------------------------    Data Structures    ------------------------ */

/* As the dynamic-programming "sweep row" advances, we need to update the
*  following information:
*
* 1.  The alignment and the specific block that constrain the feasible region
*     below (down_align and down_block) and above (up_align and up_block), if they
*     exist.  These are initially given for the alignment's seed-point, and
*     information passed to this file allows efficient updating.  The column
*     indices just inside where the two alignments intersect the current row
*     are called L and R.
*
* 2.  The blocks (gap-free segments of earlier alignments) that intersect
*     the sweep row within the feasible region.  These are keep in a list
*     (blist) that supports insertion and deletion when the sweep row reaches
*     an end of an alignment.
*/


/* --------------------- procedures to update L and R ----------------------- */

/* next_b -- move to the next down_block or up_block in a forward sweep;
*  return its column position */
static int next_b(block_ptr *bp, Msp_ptr *mp, int is_up, int row, int seed1,
  int seed2)
{
	*bp = (*bp)->next_block;
	if (*bp) {
		if ((*bp)->type == HOR && (*bp = (*bp)->next_block) == NULL)
			fatal("Last alignment block was horizontal");
		return (*bp)->b2 - seed2;
	}

	/* we've run off the end of an alignment; move to the next one */
	if (is_up) {
		*bp = (*mp)->up_block2;
		*mp = (*mp)->up_align2;
	} else {
		*bp = (*mp)->down_block2;
		*mp = (*mp)->down_align2;
	}

	/* if indeed there was a next one .. */
	if (*bp) {
		if ((*bp)->type == DIA)
			return (*bp)->b2 + row + seed1 - (*bp)->b1 - seed2;
		return (*bp)->b2 - seed2; /* jumped to a vertical block */
	}

	return 0;	/* no constraint; there was no "next alignment" */
}

/* prev_b -- move to the previous down_block or up_block in a reverse sweep;
*  return its column position */
static int prev_b(block_ptr *bp, Msp_ptr *mp, int is_up, int row, int seed1,
  int seed2)
{
	*bp = (*bp)->prev_block;
	if (*bp) {
		if ((*bp)->type == HOR && (*bp = (*bp)->prev_block) == NULL)
			fatal("First alignment block was horizontal");
		return seed2 - (*bp)->e2;
	}

	/* we've run off the front of an alignment; move to the previous one */
	if (is_up) {
		*bp = (*mp)->up_block1;
		*mp = (*mp)->up_align1;
	} else {
		*bp = (*mp)->down_block1;
		*mp = (*mp)->down_align1;
	}

	/* if indeed there was a previous one .. */ 
	if (*bp) {
		if ((*bp)->type == DIA) 
			return seed2 - ((*bp)->e2 + seed1 - row - (*bp)->e1);
		return seed2 - (*bp)->e2; /* jumped to a vertical block */
	}

	return 0;	/* no constraint; there was no "previous alignment" */
}

/* update_LR -- also update Lc and Rc, the actual column limits for
*  dynamic programming, which may lie strictly inside L and R because of the
*  X-drop constraint. */
static void update_LR(block_ptr *Up_block, block_ptr *Down_block, int row,
  int *pL, int *pR, int *pLc, int *pRc, int seed1, int seed2,
  Boolean reversed, Msp_ptr *Up_align, Msp_ptr *Down_align)
{
	 int L = *pL, R = *pR, Lc = *pLc, Rc = *pRc;

	if (!reversed) {
		if (*Down_block) {
			if ((*Down_block)->e1 >= row + seed1) {
				if ((*Down_block)->type == DIA)
					L++;
			} else
				L = next_b(Down_block, Down_align, 0, row,
				  seed1, seed2) + 1;
		}
		if (*Down_block)
			Lc = MAX(Lc, L);
		if (*Up_block) {
			if ((*Up_block)->e1 >= row + seed1) {
				if ((*Up_block)->type == DIA)
					R++; 
			} else
				R = next_b(Up_block, Up_align, 1, row,
				  seed1, seed2) - 1;
		}
		if (*Up_block)
			Rc = MIN(Rc, R);
	} else {      		/* reversed */
		if (*Up_block) {
			if ((*Up_block)->b1 <= seed1 - row) {
				if ((*Up_block)->type == DIA)
					L++;
			} else
				L = prev_b(Up_block, Up_align, 1, row,
				  seed1, seed2) + 1;
		}
		if (*Up_block)
			Lc = MAX(Lc, L);
		if (*Down_block) {
			if ((*Down_block)->b1 <= seed1 - row) {
				if ((*Down_block)->type == DIA)
					R++; 
			} else
				R = prev_b(Down_block, Down_align, 0, row,
				  seed1, seed2) - 1;
		}
		if (*Down_block)
			Rc = MIN(Rc, R);
	}

	*pL = L; *pR = R; *pLc = Lc; *pRc = Rc;
}



/* ------------------- procedures to update the blist ----------------------- */

/* build_lp -- set the p and e fields of a blist entry and mask the
*  corresponding positions in the dynamic programming array */
static void build_lp(blist_ptr lp, int reversed, dp_ptr CD, int b, int e,
  int row, int seed1, int seed2)
{
	int i;

	lp->type = lp->block->type;
	if (reversed) {
		lp->x = seed2 - lp->block->e2;
		lp->last_row = seed1 - lp->block->b1;
	} else {
		lp->x = lp->block->b2 - seed2;
		lp->last_row = lp->block->e1 - seed1;
	}

	if (lp->type != HOR) {
		if (lp->x >= b && lp->x <= e)
			CD[lp->x].mask = row;
	} else {
		int end_hor = 
		  (reversed) ? seed2 - lp->block->b2 : lp->block->e2 - seed2;
		for (i = MAX(b,lp->x); i <= MIN(e,end_hor); i++)
			CD[i].mask = row;
	}
}

/* add_new_align -- add an alignment's terminal block to the blist */
static blist_ptr add_new_align(blist_ptr list, Msp_ptr right_list, int reversed,
  dp_ptr CD, int b, int e, int row, int seed1, int seed2)
{
	blist_ptr lp = (blist_ptr) ckalloc(sizeof(blist_node));

	lp->filter = 0;
	if (reversed)
		lp->block = right_list->last_block;
	else
		lp->block = right_list->first_block;
	lp->next = list;
	build_lp(lp, reversed, CD, b, e, row, seed1, seed2);
	return lp;
}

/* filter_blist -- remove blist nodes with filter values != i */
static void filter_blist(blist_ptr *list, int i)
{
	blist_ptr pp, lp;

	for (pp = NULL, lp = *list; lp; )
		if (lp->filter != i) {
			if (pp) {
				pp->next = lp->next;
				ckfree(lp);
				lp = pp->next;
			} else {
				*list = lp->next;
				ckfree(lp);
				lp = *list;
			}
		} else {
			pp = lp;
			lp = lp->next;
		}
}

static block_ptr next_block(block_ptr bp, Boolean reversed)
{
	return (reversed ? bp->prev_block : bp->next_block);
}

static void update_blist(Boolean reversed, blist_ptr *plist, dp_ptr dp,
  int row, Msp_ptr *p_align_list, int begin, int end, int seed1, int seed2)
{
	blist_ptr lp, list = *plist;
	Msp_ptr align_list = *p_align_list;

	for (lp = list; lp; lp = lp->next) {
		if (lp->type == HOR)
			fatal("Impossible HOR block.");
		if (lp->last_row >= row) {
			if (lp->type == DIA)
				++(lp->x);
			if (lp->x >= begin && lp->x <= end)
				dp[lp->x].mask = row;
		} else if
		   ((lp->block = next_block(lp->block, reversed)) != NULL) {
			build_lp(lp, reversed, dp, begin, end, row, seed1,
			  seed2);
			if (lp->type == HOR) {
				lp->block = next_block(lp->block, reversed);
				build_lp(lp, reversed, dp, begin, end,
				  row, seed1, seed2);
			}
		} else
			lp->filter = 1;
	}

	/* sweep row may have reached new alignments */
	if (!reversed) {
		while (align_list && align_list->pos1 - seed1 == row) {
			list = add_new_align(list, align_list, reversed,
			  dp, begin, end, row, seed1, seed2);
			align_list = align_list->next_msp;
		}
	} else {
		while (align_list && seed1 - align_list->end1 == row) {
			list = add_new_align(list, align_list, reversed,
			  dp, begin, end, row, seed1, seed2);
			align_list = align_list->prev_msp;
		}
	}

	filter_blist(&list, 0); /* delete blocks whose filter is not 0 */
	*plist = list;
	*p_align_list = align_list;
}

/* --------------------- procedure to do the real work ---------------------- */

static int ALIGN_SMALL(uchar *A, uchar *B, int M, int N, edit_script_t *S, 
  int *pend1, int *pend2, AlignIOPtr align_IO, int reversed)
{ 
	int cb, row, col = 0, L, R, Lcol, Rcol, NN, gap_extend, c, d, i, m,
	  dp_len, best_score, *wa, X, tback_len, Lcol_start, end1, end2,
	  needed, seed1, seed2;
	dp_ptr dp, dq;
	dyn_prog *dyn_prog;
	static uchar **tback_row = 0;  /* XXX - cached between calls - not reentrant!! */
	uchar *tbp, *b;
	uchar st = 0;
	uchar k, s;
	int **matrix;
	TBack tback;
	blist_ptr list;
	block_ptr Up_block, Down_block;
	Msp_ptr align_list, Up_align, Down_align;
  
	if (N <= 0 || M <= 0)
		return *pend1 = *pend2 = 0;

	matrix = align_IO->subscore;
	gap_extend = align_IO->gap_extend;
	m = align_IO->gap_open + gap_extend;
	tback = align_IO->tback;
	tback_len = tback->len;
	X = align_IO->x_parameter;

	L = 0;
	R = N;
	seed1 = align_IO->seed1;
	seed2 = align_IO->seed2;
	Up_block = align_IO->Up_block;
	if (Up_block) {
		if (Up_block->type == DIA)
			R = Up_block->b2 + seed1 - Up_block->b1 - seed2;
		else
			R = Up_block->b2 - seed2;
	}
	Down_block = align_IO->Down_block;
	if (Down_block) {
		if (Down_block->type == DIA)
			L = Down_block->b2 + seed1 - Down_block->b1 - seed2;
		else
			L = Down_block->b2 - seed2;
	}
	if (reversed) {
		int temp = 0;	/* silence compiler warning */
		if (Down_block) temp = -L;
		if (Up_block) L = -R;
		if (Down_block) R = temp;
	}
	list = NULL;
	Up_align = align_IO->Up_align;
	Down_align = align_IO->Down_align;
	align_list = (reversed ? align_IO->Left_list : align_IO->Right_list);

	// tback_row = ckalloc(sizeof(uchar *)*(M+1));
	// XXX - the following is a performance hack, to avoid expensive 
	//     - mmap/munmap activity in linux with gnu malloc.  
	//     - malloc should be smarter, in which case this would not be needed.
	// XXX - not reentrant!
	{ unsigned int n = roundup(sizeof(uchar *)*(M+1), 64*1024);
	  tback_row = ckrealloc(tback_row, n);
	}
	// This didn't actually help as much as I wanted: too many different
	// sizes.  But I think mremap is cheaper than mmap/mfree, so keep it.

	tbp = tback_row[0] = tback->space;

	needed = X/gap_extend + 6;	/* upper bound for 0-th row */
	if (needed > tback_len)
		fatal("Too little space in trace_back array");
	dyn_prog = ckallocz(sizeof(dp_node));
	dp_ready(dyn_prog, col);
	dp_len = dyn_prog->len;
	dyn_prog->p[0].CC = 0;
	*(tbp++) = 0;
	c = dyn_prog->p[0].DD = -m;
	for (col = 1; col <= N && c >= -X; col++) {
		dyn_prog->p[col].CC = c;
		dyn_prog->p[col].DD = c - m;
		*(tbp++) = FLAG_I;
		c -= gap_extend;
	}
	Lcol = best_score = end1 = end2 = 0;
	Rcol = col; /* 1 column beyond the feasible region */
	for (row = 1; row <= M; row++) {
		Lcol_start = Lcol;
		update_LR(&Up_block, &Down_block, row, &L, &R, &Lcol,
		  &Rcol, seed1, seed2, reversed, &Up_align, &Down_align);
		update_blist(reversed, &list, dyn_prog->p - Lcol_start, row,
		  &align_list, Lcol, Rcol, seed1, seed2);
		needed = Rcol - Lcol + X/gap_extend + 6;
		if (tbp - tback_row[0] + needed >= tback_len) {
			if (reversed)
			  fprintf(stderr,
			    "truncating alignment starting at (%d,%d);",
				seed1 + 2 - end1, seed2 + 2 - end2);
			else
			  fprintf(stderr,
			    "truncating alignment ending at (%d,%d);",
			  	end1 + seed1 + 1, end2 + seed2 + 1);
			fprintf(stderr, "  seeded at (%d,%d)\n", seed1, seed2);
			goto back;
		}
		tback_row[row] = tbp - Lcol;
		wa = matrix[A[row]]; 
		i = c = MININT;
		b = B + Lcol + 1;
		dp_ready(dyn_prog, needed);
		dp_len = dyn_prog->len;
		dq = dyn_prog->p;
		dp = dq + Lcol - Lcol_start;
		/* We will be taking old values from *dp and putting new ones
		* at *dq.  If dq lags behind dp (because update_LR has raised
		* Lc) that only makes it easier to avoid overwriting CC
		* entries before they are used. */
		for (col = cb = Lcol; col < Rcol && b < B + N; col++) {
			d = dp->DD;
			/* At this point, c, d and i hold the values reaching
			*  nodes C, D and I along edges from previous grid
			*  points.  C also has edges entering it from the
			*  current D and I nodes, so c may be improved.  Also,
			*  b, dq, dp and tbp point to values for the current
			*  position; rather than incrementing them in the "for"
			*  statement it is more efficient to ++ their last use.
			*/
			if (list && dp->mask == row) {
				PRUNE
			}
			if (c < d || c < i) {
				if (d < i) {
					c = i;
					st = FLAG_DD | FLAG_II | FLAG_I;
				} else {
					c = d;
					st =  FLAG_DD | FLAG_II | FLAG_D;
				}
				if (c < best_score - X) {
					PRUNE
				}
				i -= gap_extend; /* for next grid pt in row */
				dq->DD = d - gap_extend; /* d for next row */
			} else {
				if (c < best_score - X) {
					PRUNE
				}
				if (c > best_score) {
					best_score = c;
					end1 = row;
					end2 = col;
				}
				if ((c -= m) > (d -= gap_extend)) {
					dq->DD = c;
					st = FLAG_C;
				} else {
					dq->DD = d;
					st = FLAG_DD | FLAG_C;
				} 
				if (c > (i -= gap_extend))
					i = c; 
				else
					st |= FLAG_II;
				c += m;
			}
			cb = col;	/* last non-pruned position */
			/* don't overwrite CC too soon */
			d = (dp++)->CC + wa[*(b++)];
			(dq++)->CC = c;	/* c for this position */
			c = d;		/* c for next position in this row */
			*(tbp++) = st;
		}
		if (Lcol >= Rcol)
			break;
		NN = (Up_block ? R : N);
		if (cb < Rcol-1)
			Rcol = cb+1;
		else
			while (i >= best_score - X && Rcol <= NN) {
				assert(dq - dyn_prog->p < dp_len);
				dq->CC = i;
				(dq++)->DD = i - m;
				i -= gap_extend;
				*(tbp++) = FLAG_I;
				Rcol++;
			}
		if (Rcol <= NN) {
			assert(dq - dyn_prog->p < dp_len);
			dq->DD = dq->CC = MININT;
			Rcol++; 
		}
	}
    back:
	*pend1 = row = end1;
	*pend2 = col = end2;
	for (s = 0; row >= 1 || col > 0 ; s = k) {
		st = tback_row[row][col];
		k = st & SELECT_CID;
		if (s == FLAG_I && (st & FLAG_II))
			k = FLAG_I;
		if (s == FLAG_D && (st & FLAG_DD))
			k = FLAG_D;
		if (k == FLAG_I) {
			col--;
			edit_script_ins(S, 1);
		} else if (k == FLAG_D) {
			row--; 
			edit_script_del(S, 1); 
		} else {
			row--;
			col--;
			edit_script_rep(S, 1);
		}
	}
	filter_blist(&list, 2);		/* removes everything */
	// ckfree(tback_row);  // XXX - static var
	ckfree(dyn_prog->p);
	ckfree(dyn_prog);
	
	return best_score;
}

void Align(AlignIOPtr align_IO)
{
	int seed1, seed2, score_right, score_left, end1, end2;
	edit_script_t *ed_scr, *ed_scr1;

	if (align_IO == NULL)
		fatal("Align() called with NULL pointer.");

	seed1 = align_IO->seed1;
	seed2 = align_IO->seed2;

	ed_scr = edit_script_new();
	score_left = ALIGN_SMALL(
				align_IO->seq1_r + align_IO->len1 - seed1 - 2, 
				align_IO->seq2_r + align_IO->len2 - seed2 - 2, 
				seed1 + 1, seed2 + 1, 
				ed_scr, &end1, &end2,
				align_IO, TRUE);
	align_IO->start1 = seed1 + 1 - end1;
	align_IO->start2 = seed2 + 1 - end2;

	ed_scr1 = edit_script_new();
	score_right = ALIGN_SMALL(
				align_IO->seq1 + seed1,
				align_IO->seq2 + seed2,
				align_IO->len1 - seed1 - 1,
				align_IO->len2 - seed2 - 1,
				ed_scr1, &end1, &end2,
				align_IO, FALSE); 
	align_IO->stop1 = seed1 + end1;
	align_IO->stop2 = seed2 + end2;
	edit_script_reverse_inplace(ed_scr1);
	edit_script_append(ed_scr, ed_scr1);
	edit_script_free(ed_scr1);

	align_IO->score = score_right + score_left;
	align_IO->ed_scr = ed_scr;
}

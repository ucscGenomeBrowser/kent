/* RCSID("$Id: yama.c,v 1.1 2002/09/24 18:04:18 kent Exp $"); */ 

/* Entry point:
*
* int row_yama(uchar **AL, uchar **GC, int K, int M, uchar *Seq, int N,
*   int left_type, int right_type, int *LB, int *RB, ss_t ss, int *gop,
*   int *gtype, int target_row, TBack tback, uchar ***AL_new, int *M_new)
*
* A is a K-by-M matrix representing a multiple alignment, with rows
* indexed by 0 <= k < K, and columns indexed by 1 <= i <= M.  Entries are in
* {A,C,G,T,DASH,FREE}.  AL is stored by columns, so the entry in row k and
* column i is AL[i][k].
*
* GC is a K-by-(M+1) array with entries in {DASH,FREE} (indexed with 0 <= k < K
* and 0 <= i <= M), where GC[i] is the column to be inserted between columns
* i and i+1 of AL.
*
* Seq is a sequence with entries in {A,C,G,T} indexed 1 <= j <= N.
*
* left_type and right_type are the types of the first and last columns of the
* new alignment, as encoded by: 0 = substitution; 1 = deletion (penalized gap in
* Seq); 2 = insertion (gap in AL); 3 = either substition or deletion but with
* no penalties for end-gaps in Seq.
*
* target_row (0 <= target_row <= K) tells where Seq should go in the resulting
* alignment.  gop and gtype are used by the macro GAP (defined in refine.h)
* for evaluating gap penalties.
*
* AL_new is the resulting (K+1)-by-M_new matrix.  It can be freed by:
* "free(AL_new[1]); free(AL_new+1);".
*/

#include "refine.h"

#define FLAG_C (0)
#define FLAG_I (1<<0)
#define FLAG_D (1<<1)
#define SELECT_CID (FLAG_I | FLAG_D | FLAG_C)
#define ALL_FLAG_I (FLAG_I<<4) | (FLAG_I<<2) | FLAG_I

/* Dynamic Programming structure. */
/* later, consider precomputing DD and the corresponding flag */
typedef struct DP {
	int D, C, I;
	int mask;       /* padding for linux/GCC */ 
} *dp_ptr, dp_node;

static void new_col(uchar *from_col, uchar c, uchar *to_col, int row, int K)
{
	int k;


	for (k = 0; k < row; ++k)
		to_col[k] = from_col[k];
	to_col[row] = c;
	for (k = row; k < K; ++k)
		to_col[k+1] = from_col[k];
}

static void new_align(edit_script_t *ed_scr, uchar **AL, uchar **GC, uchar *Seq,
  int M, int N, int K, int row, int left_type, int right_type,
  uchar ***AL_new, int *M_new)
{
	int i, j, k, m, m_new, opc;
	edit_op_t *o;
	uchar **al_new;

	m_new = 0;
	for (o = edit_script_first(ed_scr); o; o = edit_script_next(ed_scr, o))
		m_new += edit_val_get(*o);


	al_new = (uchar **)ckalloc((m_new+1)*sizeof(uchar *)) - 1;
	al_new[1] = (uchar *)ckalloc(m_new*(K+1)*sizeof(uchar));
	for (i = 2; i <= m_new; ++i)
		al_new[i] = al_new[i-1] + (K+1);

	i = j = m = 0;
	for (o = edit_script_first(ed_scr); o; o = edit_script_next(ed_scr, o)){
		opc = edit_opc_get(*o);
		k = edit_val_get(*o);
		if (opc == EDIT_OP_REP) {
		     while (k-- > 0) {
			if (++i > M || ++j > N || ++m > m_new) {
				fprintf(stderr, "%d>%d || %d>%d || %d>%d\n",
					i, M, j, N, m, m_new);
				fatal("Impossible 1.");
			}
			new_col(AL[i], Seq[j], al_new[m], row, K);
		    }
		} else if (opc == EDIT_OP_INS) {
		    while (k-- > 0) {
			if (++j > N || ++m > m_new) {
				fprintf(stderr, "%d>%d || %d>%d || %d>%d\n",
					i, M, j, N, m, m_new);
				fatal("Impossible 3.");
			}
			new_col(GC[i], Seq[j], al_new[m], row, K);
		    }
		} else if (opc == EDIT_OP_DEL) {
		    while (k-- > 0) {
			if (++i > M || ++m > m_new) {
				fprintf(stderr, "%d>%d || %d>%d || %d>%d\n",
					i, M, j, N, m, m_new);
				fatal("Impossible 2.");
			}
			new_col(AL[i],
			  ((0<j || left_type != 3) && (j<N || right_type != 3)
				? DASH : FREE),
			  al_new[m], row, K);
		    }
		} else
			fatalf("Illegal opc type: %d", opc);
	}
	if (i != M || j != N || m  != m_new) {
		fprintf(stderr, "i=%d, j=%d, m=%d, M=%d, N=%d, M_new=%d\n",
		  i, j, j, M, N, m_new);
		fatal("Error in new_align() of yama.c.");
	}
	*M_new = m_new;
	*AL_new = al_new;
}


int row_yama(uchar **AL, uchar **GC, int K, int M, uchar *Seq, int N,
  int left_type, int right_type, int *LB, int *RB, ss_t ss, int *gop,
  int *gtype, int target_row, TBack tback, uchar ***OAL, int *OM)
{
	int score, c, d, i, k, row, col, cc[128], lb, lb1;
	int x, y, z, gap_cc, gap_cd, gap_ci, gap_dc, gap_dd, gap_di,
	  gap_ic, gap_id, gap_ii, cc_vert, cc_hori, diag_c, diag_d, diag_i;
	uchar **tback_row, *tbp, st, flag_d, flag_i, flag_c, type,
	  *pal, *cal, *gal, *nal,
	   X = 'x';	/* X denotes any non-dash character */
	dp_ptr dp;
	edit_script_t *ed_scr;

	if (target_row < 0 || target_row > K)
		fatal("row_yama: Improper value of target_row.");

	if (LB[0] != 0 || RB[M] != N)
		fatal("LB and RB not terminated properly");
	for (row = 1; row <= M; row++) {
		if (LB[row] < LB[row-1])
			fatal("LB not monotonic");
		if (RB[row] < RB[row-1])
			fatal("RB not monotonic");
	}

	for (i = 0; i < 128; ++i)
		cc[i] = 0;
	dp = ckalloc((N+1)*sizeof(dp_node));
	dp[0].C = dp[0].D = dp[0].I = 0;
	for (col = 1; col <= N; ++col)
		dp[col].C = dp[col].D = dp[col].I = MININT;

	tback_row = ckalloc((M+1)*sizeof(uchar *));
	tbp = tback_row[0] = tback->space;
	*(tbp++) = 0;	/* unused */

	if (left_type == 2) {
		gal = GC[0];
		for (cc_hori = k = 0; k < K; ++k)
			cc_hori += SS(gal[k],X);
		for (i = 0, col = 1; col <= RB[0]; ++col) {
			dp[col].I = (i += cc_hori);
			*(tbp++) = ALL_FLAG_I;
		}
	} /* other left_types cannot start with insertion; as if RB[0] = 0 */

	i = c = d = 0;
	for (row = 1; row <= M; ++row) {
		pal = (row > 1) ? AL[row-1] : GC[0];
			/* (since any C or D is OK in row 0) */
		cal = AL[row];
		gal = GC[row-1];
		nal = GC[row];
		/* For the current column, cal, of AL, precompute the score
		*  for aligning with different nucleotides and dash.  Compute
		*  all combinations of gap costs for the preceding column, pal,
		*  and cal.  */
		cc['A'] = cc['C'] = cc['G'] = cc['T'] = cc_vert = cc_hori = 0;
		gap_ii = gap_ic = gap_id = gap_ci = gap_cc = gap_cd = gap_di
		  = gap_dc = gap_dd = 0;
		for (k = 0; k < K; ++k) {
			cc['A'] += SS(cal[k],'A');
			cc['C'] += SS(cal[k],'C');
			cc['G'] += SS(cal[k],'G');
			cc['T'] += SS(cal[k],'T');
			cc_vert += SS(cal[k],DASH);
			cc_hori += SS(nal[k],X);
			gap_ii -= GAP(nal[k],X,nal[k],X);
			gap_ic -= GAP(gal[k],X,cal[k],X);
			gap_id -= GAP(gal[k],X,cal[k],DASH);
			gap_ci -= GAP(cal[k],X,nal[k],X);
			gap_cc -= GAP(pal[k],X,cal[k],X);
			gap_cd -= GAP(pal[k],X,cal[k],DASH);
			gap_di -= GAP(cal[k],DASH,nal[k],X);
			gap_dc -= GAP(pal[k],DASH,cal[k],X);
			gap_dd -= GAP(pal[k],DASH,cal[k],DASH);
		}
		if (row == 1)
			gap_cc = gap_cd = gap_dc = gap_dd = 0;

		/* extra care with LB to avoid unwanted initial deletions */
		lb = LB[row];
		if (left_type == 0 || left_type == 2) /* no initial deletion */
			lb = MAX(lb,1);
		lb1 = LB[row-1];
		if (row > 1 && (left_type == 0 || left_type == 2))
			lb1 = MAX(lb1,1);

		tback_row[row] = tbp - lb;

		col = lb - 1;
		if (lb1 <= col) {
			diag_c = dp[col].C;
			diag_d = dp[col].D;
			diag_i = dp[col].I;
		} else
			diag_c = diag_d = diag_i = MININT;
			
		c = d = i = MININT;
		if (tbp - tback_row[0] + RB[row] - LB[row] + 3 >=
		    (int)(tback->len))
			fatal("ran out of trace-back space");	
		while (++col <= RB[row]) {
			/* c, d, and i values are for the previous position */
			/* diag_c, diag_d and diag_i are for (row-1,col-1) */

		    /* compute i and flag_i */
			x = c + gap_ci;
			y = d + gap_di;
			z = i + gap_ii;
			if (x >= y && x >= z) {
				i = x;
				flag_i = FLAG_C;
			} else if (y > z) {
				i = y;
				flag_i = FLAG_D;
			} else {
				i = z;
				flag_i = FLAG_I;
			}
			i += cc_hori;

		    /* compute c and flag_c */
			x = diag_c + gap_cc;
			y = diag_d + gap_dc;
			z = diag_i + gap_ic;
			if (x >= y && x >= z) {
				c = x;
				flag_c = FLAG_C;
			} else if (y > z) {
				c = y;
				flag_c = FLAG_D;
			} else {
				c = z;
				flag_c = FLAG_I;
			}
			if (col > 0) {
				if (strchr("ACGT", Seq[col]))
					c += cc[Seq[col]];
				else
					for (k = 0; k < K; ++k)
						c += SS(cal[k],Seq[col]);
			} else
				c = MININT;
			/* no initial substitution if DEL or INS required */
			if (row == 1 && col == 1 &&
			    (left_type == 1 || left_type == 2))
				c = MININT;

			/* compute d and flag_d */
			if ((col > 0 || left_type != 3) &&
			    (col < N || right_type != 3)) {
				x = dp[col].C + gap_cd + cc_vert;
				y = dp[col].D + gap_dd + cc_vert;
				z = dp[col].I + gap_id + cc_vert;
			} else {
				x = dp[col].C;
				y = dp[col].D;
				z = dp[col].I;
			}
			if (x >= y && x >= z) {
				d = x;
				flag_d = FLAG_C;
			} else if (y > z) {
				d = y;
				flag_d = FLAG_D;
			} else {
				d = z;
				flag_d = FLAG_I;
			}

			diag_c = dp[col].C;
			diag_d = dp[col].D;
			diag_i = dp[col].I;
			dp[col].C = c;
			dp[col].D = d;
			dp[col].I = i;
/*
fprintf(stderr, "row = %d, col = %d, c = %d, d = %d, i = %d\n",row,col,c,d,i);
*/
			*(tbp++) = flag_c | (flag_d<<2) | (flag_i<<4);
		}
	}

	ed_scr = edit_script_new();
	switch (right_type) {
		case 0 :
			type = FLAG_C;
			score = c;
			break;
		case 1 :
			type = FLAG_D;
			score = d;
			break;
		case 2 :
			type = FLAG_I;
			score = i;
			break;
		case 3 :
			type = (c > d ? FLAG_C : FLAG_D);
			score = MAX(c,d);
			break;
		default :
			fatal("Impossible type in row_yama.");
	}
	for (row = M, col = N; row > 0 || col > 0; type = k) {
		if (row < 0 || col < 0)
			fatal("Error generating edit script.");
		st = tback_row[row][col];
		if (type == FLAG_I) {
			edit_script_ins(ed_scr, 1);
			col--;
			k = (st>>4);
		} else if (type == FLAG_D) {
			edit_script_del(ed_scr, 1);
			row--;
			k = (st>>2) & SELECT_CID;
		} else if (type == FLAG_C) {
			edit_script_rep(ed_scr, 1);
			row--;
			col--;
			k = st & SELECT_CID;
		} else
			fatal("illegal type in traceback");
	}

	edit_script_reverse_inplace(ed_scr);
	new_align(ed_scr, AL, GC, Seq, M, N, K, target_row, left_type,
	  right_type, OAL, OM);

	edit_script_free(ed_scr);
	free(tback_row);
	free(dp);
	return score;
}

#include "util.h"
#include "seq.h"
#include "bz_all.h"

#ifndef __lint
static const char rcsid[] = 
"$Id: bz_chain.c,v 1.1 2002/09/25 05:44:07 kent Exp $";
#endif

/* The entry point is
*
*	msp_make_chain(msp_table_t *mt, int Gap1, int Gap2, int Scale, connect_fn)
*
* A highest_scoring chain of MSPs is determined, where each MSP in the chain
* (other than the last), begins strictly before the start of the next MSP in
* the chain, and a chain's score is Scale times the sum of MSP scores minus
* the sum of
*		connect(MSP_i, MSP_(i+1), Scale)
* (the last sum is taken over all MSPs in the chain except the last).  An MSP's
* cum_score value is set to 1 if the MSP is in the chain, and to 0 otherwise.
*	 Parameters Gap1 and Gap2 permit msp_make_chain to deduce useful
* inequalities about chain scores.  Namely, let MSP_i and MSP_j be MSPs on
* diagonals diag_i and diag_j, and set diff = diag_j - diag_i.
* Then Gap1 and Gap2 are required to satisfy:
*	if diff >= 0, then connect(MSP_i,MSP_j) >= diff*Gap1,
* while
*	if diff < 0, then connect(MSP_i,MSP_j) >= -diff*Gap2.
* In effect, Scale permits integer arithmetic to be used with very small gap
* penalties, since the computed chain also maximizes the sum of the MSP costs
* minus the sum of the connect(MSP_i, MSP_(i+1), Scale)/Scale.
*/

typedef double big_score_t;

#define px(i,j,G) \
	((j == 0) ? \
	 (G->start[G->perm[i]].pos1 - G->start[G->perm[i]].pos2): \
	 (G->start[G->perm[i]].pos2))

enum { BUCKET_SIZE=3 };

typedef struct {
	int Gap1, Gap2, Scale;  /* chain penalties */
	big_score_t *total_score;
	int *perm;
	int *rev_perm;
	msp_t *start, *query;
	int X, Y, Diag;		/* target point */
	connect_t connect;
} gp_t;

typedef struct best_predecessor {
	int num;
	big_score_t contrib;
} bp_t;

typedef struct kdnode {
	int	bucket;
	int	cutval;
        big_score_t max_score;
	struct	kdnode *loson, *hison;
	int	lopt, hipt;
} Kdnode, *Kdptr;

static bp_t bp_cons(int n, big_score_t c)
{
	bp_t bp;
	bp.num = n;
	bp.contrib = c;
	return bp;
}

/* --------------------------- build the K-d tree --------------------------- */

#define SWAP(p,q) \
  do {int t; t = G->perm[p]; G->perm[p] = G->perm[q]; G->perm[q] = t;} \
  /*CONSTCOND*/ while(0)

static int partition(int L, int U, int cutdim, const gp_t *const G)
{
	int m, a, b, c, v, i, j;

	if (U - L < 2)
		fatal("partition: cannot happen");

	m = (L+U)/2;
	v = a = px(L, cutdim, G);
	b = px(m, cutdim, G);
	c = px(U, cutdim, G);
	/* find the median of three entries; move to the front */
	if ((a <= b && b <= c) || (c <= b && b <= a)) {
		SWAP(L,m);
		v = b;
	} else if ((a <= c && c <= b) || (b <= c && c <= a)) {
		SWAP(L,U);
		v = c;
	}

	/* move smaller entries to front, larger to rear */
	i = L;
	j = U+1;
	while (i < j) {
		/* search forward for a large entry */
		for (++i; i <= U && px(i,cutdim,G) <= v; ++i)
			;
		/* search backward for a small entry */
		for (--j; j >= L && px(j,cutdim,G) > v; --j)
			;
		SWAP(i,j);
	}
	SWAP(i,j);	/* reverse the last swap */
	SWAP(L,j);	/* move the "pivot" value to the proper location */

	/* warning: if we return j = U, build() will recurse forever */
	if (j < U)
		return j;
	if (U-L == 2) return U-1;
	return partition(L, U-1, cutdim, G);
}

static Kdptr build(int L, int U, int toggle, const gp_t *const G)
{
	Kdptr p;
	int m;

	p = ckallocz(sizeof(Kdnode));
	p->max_score = 0;
	if (U-L+1 <= BUCKET_SIZE) {
		p->bucket = 1;
		p->lopt = L;
		p->hipt = U;
	} else {
		p->bucket = 0;
		m = partition(L, U, toggle, G);
		p->cutval = px(m, toggle, G);
		p->hipt = m;
		p->loson = build(L, m, 1-toggle, G);
		p->hison = build(m+1, U, 1-toggle, G);
	}
	assert(p);
	assert(p->bucket || (p->loson && p->hison));
	return p;
}

/* -------------------- find a best predecessor of an MSP ------------------- */
static bp_t best_pred(Kdptr Q, int lowbd_dist, int toggle,
			bp_t bp, const gp_t *const G)
{
	int i;
	big_score_t contrib;
	
	assert(Q);

	if (Q->max_score - lowbd_dist <= bp.contrib)
		return bp;

	assert(Q->bucket || (Q->loson && Q->hison));

	if (Q->bucket)
		for (i = Q->lopt; i <= Q->hipt; ++i) {
			int j = G->perm[i];
			msp_t *s = G->start+j;
			if (s->pos1 >= G->X || s->pos2 >= G->Y)
				continue;
			contrib = G->total_score[j] - G->connect(s, G->query, G->Scale);
			if (contrib > bp.contrib) {
				bp.contrib = contrib;
				bp.num = j;
			}
		}
	else if (toggle == 0) {
		big_score_t diff = G->Diag - Q->cutval;
		if (diff >= 0) {
	      		bp = best_pred(Q->hison, lowbd_dist, 1-toggle, bp, G);
			bp = best_pred(Q->loson, diff*G->Gap1, 1-toggle, bp, G);
		} else {
			bp = best_pred(Q->loson, lowbd_dist, 1-toggle, bp, G);
			bp = best_pred(Q->hison, -diff*G->Gap2, 1-toggle, bp, G);
		}
	} else {
		if (G->Y >= Q->cutval)
			bp = best_pred(Q->hison, lowbd_dist, 1-toggle, bp, G);
		bp = best_pred(Q->loson, lowbd_dist, 1-toggle, bp, G);
	}
	return bp;
}

static void update_max_score(Kdptr Q, big_score_t score, int pos)
{
	if (Q != NULL) {
		Q->max_score = MAX(Q->max_score, score);
		if (Q->hipt >= pos)
			update_max_score(Q->loson, score, pos);
		else
			update_max_score(Q->hison, score, pos);
	}
}

/* ------------------------------ Entry Point ------------------------------ */
int
msp_make_chain(msp_table_t *mt, int gap1, int gap2, int scale, connect_t con)
{
        msp_t *p;
	big_score_t best;
	int best_end;
	int i, n, *chain;
	Kdptr root;
	gp_t G;
	bp_t bp;

	if (mt == 0)
		return 0;

        msp_sort_pos1(mt);
	n = MSP_TAB_NUM(mt);

	if (n == 0)
		return 0;

	G.connect = con;
	G.start = MSP_TAB_FIRST(mt);
	G.perm = ckalloc(sizeof(int)*n);
	G.rev_perm = ckalloc(sizeof(int)*n);
	G.total_score = ckalloc(sizeof(big_score_t)*n);
	G.Gap1 = gap1;
	G.Gap2 = gap2;
	G.Scale = scale;

	for (i = 0; i < n; ++i)
		G.perm[i] = i;
	root = build(0, n-1, 1, &G);
	for (i = 0; i < n; ++i) 
	     G.rev_perm[G.perm[i]] = i;
	for (i = 0; i < n; ++i) 
		G.total_score[i] = 0;
	chain = ckalloc(n*sizeof(int));

	best_end = -1;
	best = 0;
	for (i = 0; i < n; i++) {
		G.query = G.start+i;
		G.X = G.query->pos1;
		G.Y = G.query->pos2;
		G.Diag = G.X - G.Y;
		bp = best_pred(root, 0, 1, bp_cons(-1, (big_score_t) 0), &G);
		G.total_score[i] = G.query->score*G.Scale + bp.contrib;
		if (G.total_score[i] > best) {
			best = G.total_score[i];
			best_end = i;
		}
		chain[i] = bp.num;
		update_max_score(root, G.total_score[i], G.rev_perm[i]);
	}
	for (p = MSP_TAB_FIRST(mt); MSP_TAB_MORE(mt,p); p = MSP_TAB_NEXT(p))
		p->cum_score = 0;
	for (i = best_end; i != -1; i = chain[i])
		(G.start+i)->cum_score = 1;
	/* some day we need to clean up this cum_score business */

	ZFREE(G.perm);
	ZFREE(G.rev_perm);
	ZFREE(G.total_score);
	ZFREE(chain);

	return best;
}


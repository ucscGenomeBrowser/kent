#ifndef __lint
static const char rcsid[] =
"$Id: bz_table.c,v 1.1 2002/09/25 05:44:07 kent Exp $";
#endif

#ifdef DEBUG
#include "valgrind.h"
#endif

#include "util.h"
#include "seq.h"
#include "dna.h"
#include "bz_table.h"

#include "astack.c"

typedef struct blast_table_w bt_t;

typedef struct blast_epos_node {
	struct blast_epos_node *link;	/* next occurrence of the word */
	blast_ecode_t ecode;
	int pos;		/* position where word occurs in seq #1 */
} blast_epos_node_t;

static void blast_free_chain(bt_t *bt, blast_ecode_t ecode);

/* -------------   build table of W-tuples in the first sequence ------------*/

#ifdef USE_OBSTACK  /* ifdef considered harmful !!! */
#define obstack_chunk_alloc ckalloc
#define obstack_chunk_free ckfree
#include <obstack.h> /* GNU-ism */
static struct obstack bt_obstk;
#endif

msp_table_t *msp_new_table(void)
{
	msp_table_t *mt = ckallocz(sizeof(*mt));
	return mt;
}

static blast_epos_node_t *cons_epos_node(int pos, blast_ecode_t ecode, blast_epos_node_t *link)
{
#ifdef USE_OBSTACK
	blast_epos_node_t *n = obstack_alloc(&bt_obstk, sizeof(blast_epos_node_t));
#else
	blast_epos_node_t *n = ckalloc(sizeof(struct blast_epos_node));
#endif
	n->link = link;
	n->pos = pos;
	n->ecode = ecode;
	return n;
}

static unsigned int pos_add(bt_t *bt, blast_ecode_t ecode, int pos)
{
	unsigned int e = ecode % BLAST_POS_TAB_SIZE;
	bt->epos_tab[e] = cons_epos_node(pos, ecode, bt->epos_tab[e]);
	bt->num++;
	return e;
}

static blast_epos_node_t **pos_getp(const bt_t *bt, blast_ecode_t ecode)
{
	unsigned int e = ecode % BLAST_POS_TAB_SIZE;
	return &bt->epos_tab[e];
}

static blast_epos_node_t *pos_get(const bt_t *bt, blast_ecode_t ecode)
{
	return *pos_getp(bt, ecode);
}

static blast_table_t *cons_blast_table(int W, const signed char *encoding)
{
	blast_table_t *bt = ckallocz(sizeof(*bt));
	bt->w.encoding = encoding;
	if ((unsigned int)W > sizeof(blast_ecode_t)*8/2)
		W = sizeof(blast_ecode_t)*8/2;
	bt->w.W = W;
	bt->w.epos_tab=ckallocz(BLAST_POS_TAB_SIZE*sizeof(blast_epos_node_t));
#ifdef USE_OBSTACK
	obstack_begin(&bt_obstk, 10*1024*1024);
#endif
	return bt;
}

static blast_table_t *blast_table_enc_new(SEQ *seq, int W,
  const signed char *enc)
{
	blast_ecode_t ecode;
	int i;
	uchar *s;

	uchar *const seq_beg = SEQ_CHARS(seq);
	uchar *const seq_end = seq_beg+SEQ_LEN(seq);
	blast_table_t *const bt = cons_blast_table(W, enc);
	const signed char *const encoding = bt->w.encoding;
	const blast_ecode_t mask = (1UL << ((W-1)*2)) - 1;

        if (W<=1) return 0;
	for (s = seq_beg; s < seq_end; ) {
	restart:
		/* init */
		ecode = 0L;
		for (i = 1; i < W && s < seq_end; ++i) {
			const int e = encoding[*s++];
			if (e < 0) goto restart; /* hit a run of 'X's */
			ecode = (ecode << 2) + e;
		}
		/* compute */
		for (; s < seq_end; ++s) {
			const int e = encoding[*s];
			if (e < 0) goto restart; /* hit a run of 'X's */
			ecode = ((ecode & mask) << 2) + e;
			pos_add(&bt->w, ecode, s-seq_beg+1);
		} 
	}
	return bt;
}

static blast_table_t *blast_table_masked_new(SEQ *seq, int W)
{
	return blast_table_enc_new(seq, W, fasta_encoding);
}

blast_table_t *blast_table_new(SEQ *seq, int W)
{
	return blast_table_masked_new(seq, W);
}

#if 0
     
static int chain_len(bt_t *bt, blast_ecode_t ecode)
{
        blast_epos_node_t *p;
	int count = 0;
	for (p = pos_get(bt, ecode); p; p = p->link)
		++count;
	return count;
}

static int pos_density(bt_t *bt, blast_ecode_t ecode)
{
        blast_epos_node_t *p;
	int count = 0;
	for (p = pos_get(bt, ecode); p; p = p->link)
		++count;
	return count;
}

bt_t *blast_table_pare(bt_t *bt, int surfeit)
{
        blast_ecode_t ecode;

        if (bt == 0) return 0;
 
        for (ecode = 0; ecode < BLAST_POS_TAB_SIZE; ++ecode) {
		/* Delete ecodes whose chains are too long. */
		if (chain_len(bt, ecode) > surfeit)
			blast_free_chain(bt, ecode);
		/* Delete ecodes whose positions are too dense. */
		if (pos_density(bt, ecode) > density)
			blast_free_chain(bt, ecode);
	}
	return bt;
}
#endif

/* -----------------------   search the second sequence   --------------------*/

static double entropy(const uchar *s, int n)
{
	double pA, pC, pG, pT, qA, qC, qG, qT, e;
	int count[128];
	int i, cA, cC, cG, cT;

	for (i = 0; i < 128; ++i)
		count[i] = 0;
	for (i = 0; i < n; ++i)
		++count[s[i]];
	cA = count['A'];
	cC = count['C'];
	cG = count['G'];
	cT = count['T'];
	pA = ((double)count['A']) / ((double)n);
	pC = ((double)count['C']) / ((double)n);
	pG = ((double)count['G']) / ((double)n);
	pT = ((double)count['T']) / ((double)n);
	qA = (cA ? log(pA) : 0.0);
	qC = (cC ? log(pC) : 0.0);
	qG = (cG ? log(pG) : 0.0);
	qT = (cT ? log(pT) : 0.0);
	e = -(pA*qA + pC*qC + pG*qG + pT*qT)/log(4.0);

	return e;
}

int msp_add(msp_table_t *mt, 
		int len, int pos1, int pos2, score_t score, int filter)
{
	/* XXX - use vec */
	/*printf("add %d %d %d %d %d\n", len, pos1, pos2, score, filter);*/
	if (mt->num >= mt->size) {
		/*fprintf(stderr, "grow(%d/%d) ", mt->num, mt->size);*/
		mt->size += 15;
		mt->size += mt->size/2;
		mt->msp = ckrealloc(mt->msp, mt->size*sizeof(*mt->msp));
	}
	{ msp_t *const msp = mt->msp + mt->num;
	  msp->len = len;
	  msp->pos1 = pos1;
	  msp->pos2 = pos2;
	  msp->score = score;
	  msp->filter = filter;
	}
	mt->num++;
	/*fprintf(stderr, "(%d/%d)\n", mt->num, mt->size);*/
	return mt->num;
}

/* extend_hit - extend a word-sized hit to a longer match */
int msp_extend_hit(msp_table_t *mt,
			SEQ *s1, SEQ *s2, ss_t ss, int X, int K, int W, int P,
			int pos1, int pos2,
			comb_t *diag_lev
			)
{
	const uchar *beg1, *end1;
	int left_sum, right_sum;
	const uchar *const seq1 = SEQ_CHARS(s1);
	const uchar *const seq2 = SEQ_CHARS(s2);
	const int diag = pos2 - pos1;

	if (seq1 == seq2 && pos1 >= pos2)
		return 0;
	
	if (comb_get(diag_lev,diag) > pos1)
		return 0;
        
	{ /* extend to the right */
	    const uchar *q = end1 = seq1 + pos1;
	    const uchar *s = seq2 + pos2;
            const int end = SEQ_LEN(s2)-diag;
	    const int end_ = (end < SEQ_LEN(s1)) ? end : SEQ_LEN(s1);
	    const uchar *const seq1_end = seq1 + end_;
	    int sum;

	    right_sum = sum = 0;
	    while (q < seq1_end && sum >= right_sum - X) {
		if ((sum += ss[*s++][*q++]) > right_sum) {
			right_sum = sum;
			end1 = q;
		}
	    }
	}

	{ /* extend to the left */
	    const uchar *q = beg1 = (seq1 + pos1) - W;
	    const uchar *s = (seq2+pos2)-W;
	    const int end = comb_get(diag_lev,diag)-W;
	    const int end_ = (end < 0) ? 0 : end;
	    const int end__ = (end_ < -diag) ? -diag : end_;
	    const uchar *const seq1_end = seq1+end__;
	    int sum;

	    left_sum = sum = 0;
	    while (q > seq1_end  && sum >= left_sum - X) {
		if ((sum += ss[*--s][*--q]) > left_sum) {
			left_sum = sum;
			beg1 = q;
		}
	    }
	}

	{   score_t score = right_sum + left_sum;
	    const uchar *q = (seq1 + pos1) - W;
	    const uchar *s = (seq2 + pos2) - W;
	    const uchar *qend = seq1 + pos1;

#ifdef DEBUG
fprintf(stderr, "seq1=%p\n", seq1);
fprintf(stderr, "end1=%p\n", seq1+SEQ_LEN(s1));
fprintf(stderr, "q=   %p\n", q);

fprintf(stderr, "seq2=%p\n", seq2);
fprintf(stderr, "end2=%p\n", seq1+SEQ_LEN(s2));
fprintf(stderr, "pos2=%d W=%d\n", pos2, W);
fprintf(stderr, "s=   %p\n", s);

assert(q >= seq1);
assert(s >= seq2);
#endif


	    while (q < qend) {
#ifdef DEBUG
if (VALGRIND_CHECK_READABLE(q, sizeof(*q))) fatalf("\nq=%p %d",q, *q);
if (VALGRIND_CHECK_READABLE(s, sizeof(*s))) fatalf("\ns=%p %d",s, *s);
#endif
		score += ss[*q++][*s++];
	    }
            if (P && score >= K && score <= 10*K) {
                double ent = entropy(beg1, end1-beg1);
		int old_score = score;
                score = (int) ((double)score * ent);
		if (score <= K && P > 1)
			fprintf(stderr, "pruned hit of score %d at (%d,%d)\n",
			   old_score, beg1-seq1, beg1-seq1+diag);
	    }
	    
	    if (score >= K)
		msp_add(mt, end1-beg1, beg1-seq1, beg1-seq1+diag, score, 0);
	}

	comb_set(diag_lev,diag, (end1 - seq1) + W);

	return 0;
}


static msp_table_t *blast_search_unchecked(SEQ *seq1, SEQ *seq2,
 blast_table_t *bt,
 msp_table_t *msp_tab,
 ss_t ss, int X, int K, int P)
{
	uchar *s;
	blast_ecode_t ecode;
	static comb_t *diag_lev = 0;
	int i;

	const int W = bt->w.W;
	const int len1 = SEQ_LEN(seq1);
	const int len2 = SEQ_LEN(seq2);
	uchar *const beg2 = SEQ_CHARS(seq2);
	uchar *const end2 = beg2+len2;
	const signed char *const encoding = bt->w.encoding;
	const blast_ecode_t mask = (1UL << ((W-1)*2)) - 1;

        if (W<=1) return 0;

	{ unsigned int n = (len1+len2+1)*sizeof(int);
	  n = roundup(n, 64*1024);
	  diag_lev = comb_resize(diag_lev,n,len1);
        }

	for (s = beg2; s < end2; ) {
	restart:
		/* init */
		ecode = 0;
		for (i = 1; i < W && s < end2; ++i) {
			const int e = encoding[*s++];
			if (e < 0) goto restart;
			ecode = (ecode << 2) + e;
		}
		/* compute */
		for (; s < end2; ++s) {
			const int e = encoding[*s];
			const blast_epos_node_t *h;
			if (e < 0) goto restart; /* hit a run of 'X's */
			ecode = ((ecode & mask) << 2) + e;
			for (h = pos_get(&bt->w, ecode); h; h = h->link)
				if (h->ecode == ecode)
					msp_extend_hit(msp_tab, seq1, seq2, 
						ss, X, K, W, P,
						h->pos, s-beg2+1, diag_lev);
		} 
	}

	comb_clear(diag_lev);
	return msp_tab;
}

msp_table_t *blast_search(SEQ *seq1, SEQ *seq2,
 blast_table_t *bt,
 msp_table_t *msp,
 ss_t ss, int X, int K, int P)
{
	if (seq1 == 0) return 0;
	if (seq2 == 0) return 0;
	if (bt == 0) return 0;

	return blast_search_unchecked(seq1, seq2, bt, msp, ss, X, K, P);
}

static void blast_free_chain(bt_t *bt, blast_ecode_t ecode)
{
	blast_epos_node_t **p = pos_getp(bt, ecode);
	blast_epos_node_t *q = *p;

	while (q) {
		blast_epos_node_t *t = q->link;
		ZFREE(q);
		q = t;
	}
	*p = 0; /* clear table entry too */
}

void blast_table_free(blast_table_t *bt)
{
        if (bt == 0) return;

#ifdef USE_OBSTACK
	obstack_free(&bt_obstk, (void*)0);
#else
	{blast_ecode_t ecode;
	 for (ecode = 0; ecode < BLAST_POS_TAB_SIZE; ++ecode)
		blast_free_chain(&bt->w, ecode);
	}
#endif
	ZFREE(bt->w.epos_tab);
	ZFREE(bt);
}

void msp_free_table(msp_table_t *mt)
{
	if (mt && mt->msp) ZFREE(mt->msp);
	if (mt) ZFREE(mt);
}

/* -----------------------------  filter the MSPs  ---------------------------*/

msp_table_t *msp_compress(msp_table_t *mt)
{
	msp_t *p, *q;
	int i = 0;

	if (mt == 0)
		return 0;
	if (mt->msp == 0)
		return 0;

	for (q=p=MSP_TAB_FIRST(mt); MSP_TAB_MORE(mt,p); p=MSP_TAB_NEXT(p)) {
		if (p->filter == 0) {
			if (p != q)
				*q = *p;
			q = MSP_TAB_NEXT(q);		
			++i;
		} 
	}
	mt->num = i;
	return mt;
}

#define DEFINE_MSP_CMP(f) \
static int msp_cmp_##f(const void *a, const void *b) { \
	return ((const msp_t*)a)->f - ((const msp_t*)b)->f; \
}
DEFINE_MSP_CMP(pos1)

static msp_table_t *msp_sort_by(msp_table_t *mt, msp_cmp_t cmp)
{
	msp_t *msp; 
	unsigned int num;

	if ((mt == NULL) || ((num = mt->num) == 0))
		return 0;

        msp = mt->msp;
	qsort((char *) msp, num, sizeof(msp_t), cmp);
	return mt;
}

#define DEFINE_MSP_SORT(f) \
msp_table_t *msp_sort_##f(msp_table_t *mt) { \
      return msp_sort_by(mt, msp_cmp_##f); \
}
DEFINE_MSP_SORT(pos1)

/* --------------------------  sort the MSPs  ------------------------------- */

/* sort MSPs by score */
static int msp_cmp_score(const void *e, const void *f)
{
	int i;
	const msp_t *ee = (const msp_t *)e;
	const msp_t *ff = (const msp_t *)f;
	

	if ((i = (ee->score - ff->score)) < 0)
                return 1;
        else if (i > 0)
                return -1;
        else if ((i = (ee->pos1 - ff->pos1)) > 0)
                return 1;
	else if (i < 0)
		return -1;
        else if ((i = (ee->pos2 - ff->pos2)) > 0)
                return 1;
	else if (i < 0)
		return -1;
	else
                return 0;
} 

msp_table_t *msp_sort(msp_table_t *mt)
{
	return msp_sort_by(mt, msp_cmp_score);
}

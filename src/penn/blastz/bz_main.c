static const char rcsid[]=
"$Id: bz_main.c,v 1.1 2002/09/25 05:44:07 kent Exp $";

#include "bz_all.h"
#include <signal.h>

#define DEFAULT_E	30
#define DEFAULT_O	400
#define DEFAULT_W       8
#define DEFAULT_R       0
#define DEFAULT_G       0

static const char Usage[] =
"%s seq1 [seq2]\n\n\
	m(80M) bytes of space for trace-back information\n\
	b(0) 0: normal; 1: reverse seq1; 2 reverse seq2; 3 reverse both\n\
	c(0) 0: quiet; 1: print census\n\
	v(0) 0: quiet; 1: verbose progress reports to stderr\n\
	r(0) 0: old style report; 1: new style\n\
	B(2) 0: single strand; >0: both strands\n\
        C(0) 0: no chaining; 1: just output chain; 2: chain and extend;\n\
		3: just output HSPs\n\
	E(30) gap-extension penalty.\n\
        G(0) diagonal chaining penalty.\n\
        H(0) interpolate between alignments at threshold K = argument.\n\
	K(3000) threshold for MSPs\n\
	L(K) threshold for gapped alignments\n\
	M(50) mask threshold for seq1, if a bp is hit this many times\n\
	O(400) gap-open penalty.\n\
	P(1) 0: entropy not used; 1: entropy used; >1 entropy with feedback.\n\
	Q load the scoring matrix from a file.\n\
        R(0) antidiagonal chaining penalty.\n\
        T(1) 0: use W for word size;  1: use 12of19.\n\
	W(8) word size.\n\
	Y(O+300E) X-drop parameter for gapped extension.\n";


typedef struct bz_flags {
  int R, G;
} bf_t;

static bf_t bz_flags;
static int Connect(msp_t *q, msp_t *p, int scale);
static int verbose;
static int flag_1219;

// static ss_t ss;
// static ss_t sss;
static int ss[NACHARS][NACHARS];
static int sss[NACHARS][NACHARS];

enum { MSP_Scale = 100 };

static void mkmask_ss(ss_t ss, ss_t sss)
{
	int i, j, bad = ss['A']['X'];

	for (i = 0; i < NACHARS; ++i)
		for (j = 0; j < NACHARS; ++j)
		   sss[i][j] = ((isupper(i) && isupper(j)) ? ss[i][j] : bad);
}

/* connect - compute penalty for connecting two fragements */
static int Connect(msp_t *q, msp_t *p, int scale)
{
	int q_xend, q_yend, diag_diff, substitutions;

	if (p->pos1 <= q->pos1 || p->pos2 <= q->pos2)
		fatal("HSPs improperly ordered for chaining.");
	q_xend = q->pos1 + q->len - 1;
	q_yend = q->pos2 + q->len - 1;
	diag_diff = (p->pos1 - p->pos2) - (q->pos1 - q->pos2);
	if (diag_diff >= 0)
		substitutions = p->pos2 - q_yend - 1;
	else {
		substitutions = p->pos1 - q_xend - 1;
		diag_diff = -diag_diff;
	}
	return diag_diff*bz_flags.G + (substitutions >= 0 ?
		substitutions*bz_flags.R :
		-substitutions*scale*ss[(uchar)'A'][(uchar)'A']);
}

/* --------------- Webb added this Aug. 16, 2002 ---------------------------- */

#define MAX_SPACE 50000 /* max bp between alignments to search */
#define SMALL_SPACE 10000 /* size for most sensitive search */
#define NEW_W 7 /* word size for focused search */

static SEQ *fakeSEQ(SEQ *sf, int b, int e) {
	SEQ *xf = ckalloc(sizeof(SEQ));
	uchar *s = SEQ_CHARS(sf);
	int i, L;

	xf->fp = NULL;
	xf->flags = xf->count = 0;
	xf->offset = 0;
	xf->maskname = xf->fname = xf->header = NULL;
	xf->from = 1;
	xf->hlen = 0;
	xf->slen = L = e - b + 1;
	xf->seq = ckalloc((L+1)*sizeof(uchar));
	for (i = 0; i < L; ++i)
		xf->seq[i] = s[b+i-1];
	xf->seq[L] = '\0';
	return xf;
}

static align_t *focus1(int b1, int e1, int b2, int e2, SEQ *sf1, SEQ *sf2,
  gap_scores_t gs, ss_t ss, ss_t sss, int Y, int K, TBack tback) {
	blast_table_t *bt;
	static msp_table_t *mt = 0;
	msp_t *p;
	align_t *a = NULL, *aa;
	SEQ *xf1, *xf2;
	uchar *rf1;
	int f = 0;

	/* create SEQ objects for the tiny sequences */
	xf1 = fakeSEQ(sf1, b1, e1);
	xf2 = fakeSEQ(sf2, b2, e2);

	bt = blast_table_new(xf1, NEW_W);
	mt = msp_new_table();
	mt->num = 0;
	mt = blast_search(xf1, xf2, bt, mt, sss, Y , K, 0);	//0 = P

	/* chain */
	(void)msp_make_chain(mt, bz_flags.G, bz_flags.G, MSP_Scale, Connect);
	for (p = MSP_TAB_FIRST(mt); MSP_TAB_MORE(mt,p); p = MSP_TAB_NEXT(p)) {
		p->filter = 1 - p->cum_score;
		f |= p->filter;
	}

	msp_compress(mt);
	msp_sort_pos1(mt);

	rf1 = reverse_seq(SEQ_CHARS(xf1), SEQ_LEN(xf1));
	if (mt && MSP_TAB_NUM(mt) != 0)
		a = bz_extend_msps(xf1, rf1, xf2, mt, &gs, ss, Y, K, tback);
        blast_table_free(bt);
	msp_free_table(mt);
/*
	free(rf1);
	free(xf1->seq);
	free(xf1);
	free(xf2->seq);
	free(xf2);
*/

	/* shift the entries of each alignment */
	for (aa = a; aa != NULL; aa = aa->next_align) {
		aa->seq1 = SEQ_CHARS(sf1);
		aa->seq2 = SEQ_CHARS(sf2);
		aa->beg1 += (b1-1);
		aa->end1 += (b1-1);
		aa->beg2 += (b2-1);
		aa->end2 += (b2-1);
	}

	return a;
}

void focus(align_t *a, SEQ *sf1, SEQ *sf2, gap_scores_t gs, ss_t ss, ss_t sss,
  int Y, int interK, TBack tback) {
	align_t *behind, *ahead, *new;
	int b1, b2, a1, a2, new_K;

	if (a == NULL)
		return;
	b1 = b2 = 0;
	for (behind = a, ahead = a->next_align;
	     ahead;
	     behind = ahead, ahead = ahead->next_align) {
		b1 = MAX(b1, behind->end1);
		b2 = MAX(b2, behind->end2);
		a1 = ahead->beg1;
		a2 = ahead->beg2;
		if (b1 < a1 && a1 < b1 + MAX_SPACE &&
		    b2 < a2 && a2 < b2 + MAX_SPACE) {
			new_K = interK;
			if (a1 < b1 + SMALL_SPACE && a2 < b2 + SMALL_SPACE)
				new_K *= 0.9;
			new = focus1(b1, a1, b2, a2, sf1, sf2, gs, ss,
			  sss, Y, new_K, tback);
			if (new != NULL) {
				behind->next_align = new;
				while (new->next_align != NULL)
					new = new->next_align;
				new->next_align = ahead;
			}
		}
	}
}

/* --------------- end of what Webb added Aug. 16, 2002 --------------------- */

/* strand1 -- process one sequence from the second file in one orientation */
static void strand1(SEQ *sf1, uchar *rf1, SEQ *sf2, blast_table_t *bt, gap_scores_t gs,
 ss_t ss, ss_t sss, int X, int Y, int K, int L, int P, int chain, TBack tback,
 census_t census[], int mask_thresh, int interK)
{
	static msp_table_t *mt = 0;

	if (mt == 0) mt = msp_new_table(); // XXX - not reentrant

	mt->num = 0;
	mt = (flag_1219?blast_1219_search:blast_search)(sf1, sf2, bt, mt, sss, X, K, P);

	if (chain == 1 || chain == 2) {
	    msp_t *p;
	    int f = 0;
	    
	    (void)msp_make_chain(mt, bz_flags.G, bz_flags.G, MSP_Scale, Connect);
	    for (p = MSP_TAB_FIRST(mt); MSP_TAB_MORE(mt,p); p = MSP_TAB_NEXT(p)) {
		p->filter = 1 - p->cum_score;
		f |= p->filter;
	    }

	    msp_compress(mt);
	    msp_sort_pos1(mt);
	}
	if (chain == 1 || chain > 2) {
		print_align_header(sf1, sf2, ss, &gs, K, L);
	        print_msp_list(sf1, sf2, mt);
	} else if (mt != 0 && MSP_TAB_NUM(mt) != 0) {
		align_t *a;
		a = bz_extend_msps(sf1, rf1, sf2, mt, &gs, ss, Y, L, tback);
		/* next two lines added Aug. 16, 2002 */
		if (a && interK)
		       focus(a, sf1, sf2, gs, ss, sss, Y, interK, tback);
		if (a) {
			int n;
			print_align_header(sf1, sf2, ss, &gs, K, L);
			print_align_list(a);
			n = census_mask_align(a, SEQ_LEN(sf1), SEQ_CHARS(sf1), rf1, census, mask_thresh);
			printf("x {\n  n %d\n}\n", n);
		}
		free_align_list(a);
	}
	//msp_free_table(mt); // XXX
}

static void bye(int sig)
{
	exit(sig);
}

void reverse_inplace(uchar *s, int len)
{
        uchar *p = s + len - 1;

        while (s <= p) {
                register uchar t = *p;
                *p-- = *s;
                *s++ = t;
	}
}


int main(int argc, char *argv[])
{
	SEQ *sf1, *sf2;
	uchar *rf1;
	blast_table_t *bt;
	gap_scores_t gs;
	int W, X, Y, K, L, P;
	int mask_thresh, interK;
	int flag_strand, flag_chain, flag_size, flag_census, flag_reverse;
	char *scorefile;
	TBack tback;
	census_t *census;

	// mtrace();

	// flush stdio buffers when killed
	signal(SIGHUP, bye);
	signal(SIGINT, bye);
	signal(SIGTERM, bye);

	if (argc < 3)
		fatalf(Usage, argv[0]);
	ckargs("BCEFGHKLMOPQRTWYbcmrv", argc, argv, 2);

	if (get_cargval('Q', &scorefile))
		scores_from_file(scorefile, ss);
	else
		DNA_scores(ss);
	mkmask_ss(ss, sss);

	get_argval_pos('m', &flag_size, 80*1024*1024);
	get_argval_pos('b', &flag_reverse, 0);
	get_argval_pos('c', &flag_census, 0);
	get_argval_pos('v', &verbose, 0);
	get_argval_nonneg('B', &flag_strand, 2);
	get_argval_nonneg('C', &flag_chain, 0);
	get_argval_nonneg('E', &(gs.E), DEFAULT_E);
	get_argval_nonneg('G', &(bz_flags.G), DEFAULT_G);
	get_argval_nonneg('H', &interK, 0);
	get_argval_pos('K', &K, 3000);
	get_argval_pos('L', &L, K);
	get_argval_nonneg('M', &mask_thresh, 50);
	get_argval_nonneg('O', &(gs.O), DEFAULT_O);
	get_argval_nonneg('P', &P, 1);
	get_argval_nonneg('R', &(bz_flags.R), DEFAULT_R);
	get_argval_nonneg('T', &flag_1219, 1);
	get_argval_pos('W', &W, DEFAULT_W);
	get_argval_pos('X', &X, 10*ss[(uchar)'A'][(uchar)'A']);
	get_argval_pos('Y', &Y, (int)(gs.O+300*gs.E));

	if (flag_1219) W = 12;

        sf1 = seq_open(argv[1]);
        if (!seq_read(sf1))
		fatalf("Cannot read sequence from %s.", argv[1]);
        if (!is_DNA(SEQ_CHARS(sf1), SEQ_LEN(sf1)))
                fatal("The first sequence is not a DNA sequence.");
	if (flag_reverse & 01)
	    reverse_inplace(SEQ_CHARS(sf1), SEQ_LEN(sf1));

	rf1 = reverse_seq(SEQ_CHARS(sf1), SEQ_LEN(sf1));
	
	sf2 = seq_open(argv[2]);
	if (!seq_read(sf2))
		fatalf("Cannot read sequence from %s.", argv[2]);
	if (!is_DNA(SEQ_CHARS(sf2), SEQ_LEN(sf2)))
		fatal("The second sequence is not a DNA sequence.");

	bt = (flag_1219 ? blast_1219_new : blast_table_new)(sf1, W);

	if (flag_chain == 0 || flag_chain == 2) {
        	tback = ckalloc(sizeof(TB));
        	tback->space = ckalloc(flag_size*sizeof(uchar));
        	tback->len = flag_size;
	} else
		tback = NULL;

	census = new_census(SEQ_LEN(sf1));

	print_job_header(ss, &gs, K, L, mask_thresh);
	do {
		if (flag_reverse & 02)
		    reverse_inplace(SEQ_CHARS(sf2), SEQ_LEN(sf2));
		strand1(sf1, rf1, sf2, bt, gs, ss, sss, X, Y, K, L, P,
		    flag_chain, tback, census, mask_thresh, interK);
		if (flag_strand > 0) {
			seq_revcomp_inplace(sf2);
			strand1(sf1, rf1, sf2, bt, gs, ss, sss, X, Y, K, L, P,
			    flag_chain, tback, census, mask_thresh, interK);
		}
		fflush(stdout);
	} while (seq_read(sf2));
	print_intervals(stdout, census, SEQ_LEN(sf1), mask_thresh);
	if (flag_census) print_census(stdout, census, SEQ_LEN(sf1), 0);
	print_job_footer();
	//print_statistics();

        (flag_1219 ? blast_1219_free : blast_table_free)(bt);

        seq_close(sf2);
	seq_close(sf1);
	if (tback) {
		ckfree(tback->space);
		ckfree(tback);
	}
	return 0;
}

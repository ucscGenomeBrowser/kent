// -- a single match of length 12 at chosen positions in a 19-mer,
//  allowing one transition 
// -- a single match of length 11 at chosen positions in a 18-mer,
//  allowing one transition 

#include "util.h"
#include "seq.h"
#include "dna.h"
#include "bz_table.h"
#include "bz_hit19.h"

#include "astack.c"
// #include "cache.c"

//#define STATISTICS(code) code;
#define STATISTICS(code)

struct statistics {
    long add_word;
    long extend_hit;
    long a, b, c, d, e;
} STATS;

void print_statistics() {
    fprintf(stderr, "add_word=%ld\n", STATS.add_word);
    fprintf(stderr, "extend_hit=%ld\n", STATS.extend_hit);
    fprintf(stderr, "a=%ld\n", STATS.a);
    fprintf(stderr, "b=%ld\n", STATS.b);
    fprintf(stderr, "c=%ld\n", STATS.c);
    fprintf(stderr, "d=%ld\n", STATS.d);
    fprintf(stderr, "e=%ld\n", STATS.e);
}


static void print_ll(FILE *fp, unsigned long long x)
{
    int i;
    for (i = 36; i >= 0; i -= 2)
	fprintf(fp, "%c", "ACGT"[(x>>i)&3]);
}

static void print_l(FILE *fp, unsigned long x)
{
    int i;
    for (i = 22; i >= 0; i -= 2)
	fprintf(fp, "%c", "ACGT"[(x>>i)&3]);
}


//              8765432109876543210
// template     1110100110010101111 (12 of 19)

#define B(n) ((1ULL<<(n*2))-1)
static unsigned int map_19to12(unsigned long long i)
{
        unsigned int j =
                ((i & (B(3) << (2*16))) >> (2*7)) |
                ((i & (B(1) << (2*14))) >> (2*6)) |
                ((i & (B(2) << (2*10))) >> (2*4)) |
                ((i & (B(1) << (2* 7))) >> (2*2)) |
                ((i & (B(1) << (2* 5))) >> (2*1)) |
                ((i & (B(4) << (2* 0))) >> (2*0)) ;
	    // DEBUG
	    //fprintf(stdout, "%016llx %016x\n", i, j);
	    //print_ll(stdout, i); 
	    //fprintf(stdout, " ");
	    //print_l(stdout, j); 
	    //fprintf(stdout, "\n");
        return j;
}

//              765432109876543210
// template     111010010100110111  (11 of 18)

static unsigned int map_18to11(unsigned long long i)
{
        unsigned int j =
                ((i & (B(3) << (2*15))) >> (2*7)) |
                ((i & (B(1) << (2*13))) >> (2*6)) |
                ((i & (B(1) << (2*10))) >> (2*4)) |
                ((i & (B(1) << (2* 8))) >> (2*3)) |
                ((i & (B(2) << (2* 4))) >> (2*1)) |
                ((i & (B(3) << (2* 0))) >> (2*0)) ;
        //fprintf(stdout, "%016lx %016llx\n", j, i);
        return j;
}

#define N4E12 16777216	/* 4^12 */

/* -------------   build table of W-tuples in the first sequence  ----------- */

typedef struct hashpos {
	int pos;		/* position where word starts in sequence 1 */
	struct hashpos *next;
} Hash;

typedef struct blast_table_n bt_t;

/* add_word - add to the table a position in sequence 1 of a given word */
static void add_word(bt_t *bt, int ecode, int pos)
{
	Hash *h;

	if (bt->n_node >= bt->npool)
		fatal("blast hashtab overflowed (can't happen)");
	h = &(bt->pool[bt->n_node++]);
	h->pos = pos;
	h->next = bt->hashtab[ecode];
	bt->hashtab[ecode] = h;
	//STATISTICS(STATS.add_word++);
}

blast_table_t *blast_1219_new(SEQ *seq, int W)
{
	unsigned long long ei;
	unsigned int ecode;
	int i;
	const unsigned char *s;
	const int len1 = SEQ_LEN(seq);
	const unsigned char *const seq1 = SEQ_CHARS(seq);
	const unsigned char *const seq_end = seq1 + len1;
	blast_table_t *bt;

	if (W != 12)
		fatal("Cannot handle W != 12.");
	if (len1 < 19)
		fatal("Cannot handle seq1 len < 19.");

	bt = ckalloc(sizeof(*bt));
	bt->n.hashtab = (Hash **) ckallocz(N4E12*sizeof(Hash *));
	if (len1 > LONG_MAX/(W+1))
		fatalf("sequence too long: %ld\n", len1);
	bt->n.npool = (W+1)*(len1 - W + 1);
	if ((unsigned int)bt->n.npool > LONG_MAX/sizeof(Hash))
		fatalf("too many nodes: %d\n", bt->n.npool);
	bt->n.pool = (Hash *) ckalloc(bt->n.npool*sizeof(Hash));
	bt->n.n_node = 0;
	
	for (s = seq1; s < seq_end;  ) {
	restart:
//STATISTICS(STATS.a++);
		/* init -- load first 18bp, compute starts with 19th */
		ei = 0;
		for (i = 0; i < 18 && s < seq_end; ++i) {
			const int e = fasta_encoding[*s++];
			if (e < 0) goto restart; /* hit a run of 'X's */
			ei = (ei << 2) | e;
//printf("%c -> ", s[-1]); print_ll(stdout, ei); printf("\n");
		}
//STATISTICS(STATS.b++);
		/* compute */
		for ( ; s < seq_end; ++s) {
			const int e = fasta_encoding[*s];
			if (e < 0) goto restart; /* hit a run of 'X's */
			ei = (ei << 2) | e;
			ecode = map_19to12(ei);
//STATISTICS(STATS.c++);
			add_word(&bt->n, ecode, s-seq1+1);
			for (i = 0; i < W; ++i) {
				unsigned int t;
				// more general version:
				//j = (ecode & (3<<(2*i))) >> (2*i);
				//j = transition[j];
				//t = (ecode &~ (3<<(2*i))) | (j << (2*i));

				t = ecode ^ (2 << (2 * i)); /* A ~ G; C ~ T */
				add_word(&bt->n, t, s-seq1+1);
			}
		} 
	}
	return bt;
}

void blast_1219_free(blast_table_t *bt)
{
	free(bt->n.hashtab);
	free(bt->n.pool);
}

/* -----------------------   search the second sequence   --------------------*/


msp_table_t *blast_1219_search(SEQ *seq1, SEQ *seq2, blast_table_t *bt, msp_table_t *mt, ss_t ss, int X, int K, int P)
{
	unsigned long long ei;
	unsigned int ecode;
	int i;
	Hash *h;
	const unsigned char *s;
	const unsigned char *const seq = SEQ_CHARS(seq2);
	const unsigned char *const seq_end = seq + SEQ_LEN(seq2);
	const int len1 = SEQ_LEN(seq1);
	const int len2 = SEQ_LEN(seq2);
	static comb_t *diag_lev = 0;

        { // unsigned int n = (len1+len2+1)*sizeof(int);
          unsigned int n = (len1+len2+1);
// XXX
// XXX n is size in ints, right?  so no sizeof above.
// XXX
          n = roundup(n, 64*1024);
          diag_lev = comb_resize(diag_lev,n,len1);
        }

	for (s = seq; s < seq_end;  ) {
	restart:
		/* init -- load first 18bp, compute starts with 19th */
		ei = 0;
		for (i = 0; i < 18 && s < seq_end; ++i) {
			const int e = fasta_encoding[*s++];
			if (e < 0) goto restart; /* hit a run of 'X's */
			ei = (ei << 2) | e;
		}
		/* compute */
		for ( ; s < seq_end; ++s) {
			const int e = fasta_encoding[*s];
			if (e < 0) goto restart; /* hit a run of 'X's */
			ei = (ei << 2) | e;
			ecode = map_19to12(ei);
			for (h = bt->n.hashtab[ecode]; h; h = h->next) {
//STATISTICS(STATS.extend_hit++);
				msp_extend_hit(mt, seq1, seq2,
                                                ss, X, K, 19, P,
                                                h->pos, s-seq+1, diag_lev);
			}
		} 
	}
	comb_clear(diag_lev);
	return mt;
}

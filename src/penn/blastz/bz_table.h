/* $Id: bz_table.h,v 1.1 2002/09/25 05:44:08 kent Exp $ */

#include <inttypes.h>
#include <assert.h>

#ifndef ZFREE
#define ZFREE(p) /*CONSTCOND*/do{ckfree(p);(p)=0;}while(0)
#endif

typedef uint32_t blast_ecode_t;

enum { BLAST_POS_TAB_SIZE = 65536UL };  /* 65536 = 4**8 */

typedef struct blast_table_w {
        int W;
        int num;
        const signed char *encoding;
        struct blast_epos_node **epos_tab;
} blast_table_w;

typedef struct blast_table_n {
        struct hashpos **hashtab; /* array of pointers to positions in seq1, indexed by 12-mer */
        struct hashpos *pool;     /* pool of free nodes */
        int npool;      /* number of nodes available */
        int n_node;     /* index of next free node in pool */
} blast_table_n;

typedef union {
 struct blast_table_w w;
 struct blast_table_n n;
} blast_table_t;

#ifndef SCORE_T
#define SCORE_T long
#endif
typedef SCORE_T score_t;

typedef struct {
	int len, pos1, pos2;
	score_t score, cum_score;
	int filter;
} msp_t;

typedef struct msp_table {
	int size;
	int num;
	msp_t *msp;
} msp_table_t;
#define MSP_TAB_FIRST(t) ((t)->msp)
#define MSP_TAB_NEXT(m) ((m)+1)
#define MSP_TAB_NUM(t) ((t)->num)
#define MSP_TAB_MORE(t,m) ((m-MSP_TAB_FIRST(t))<MSP_TAB_NUM(t))
#define MSP_TAB_NTH(t,n) ((t)->msp[n])

typedef int (*msp_cmp_t)(const void *, const void *);

blast_table_t *blast_table_new(SEQ *, int);
msp_table_t *blast_search(SEQ *seq1, SEQ *seq2, blast_table_t *bt, msp_table_t *mt, ss_t ss, int X, int K, int T);
void blast_table_free(blast_table_t *bt);
void msp_free_table(msp_table_t *mt);
msp_table_t *msp_new_table(void);

msp_table_t *msp_compress(msp_table_t *mt);
msp_table_t *msp_sort_pos1(msp_table_t *mt);
msp_table_t *msp_sort(msp_table_t *mt);
int msp_add(msp_table_t *, int, int, int, score_t, int);

typedef int (*connect_t) (msp_t *, msp_t *, int);
int msp_make_chain (msp_table_t *, int, int, int, connect_t);

struct comb;
int msp_extend_hit(msp_table_t *mt,
                        SEQ *s1, SEQ *s2, ss_t ss, int X, int K, int W, int P,
                        int pos1, int pos2,
                        struct comb *diag_lev
                        );


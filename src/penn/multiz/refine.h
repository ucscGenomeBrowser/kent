/* RCSID("$Id: refine.h,v 1.1 2002/09/24 18:04:18 kent Exp $"); */
#include "util.h"
#include "seq.h"
#include "args.h"
#include "edit.h"
#include "dna.h"

#define DNODE 0		/* Diagonal Node */
#define MATCH_COL 0
#define VNODE 1		/* Vertical Node */
#define GAP_SEQ 1
#define HNODE 2		/* Horizontal Node */
#define GAP_AL 2
#define FREE_END 3	/* Free ends in Seq */

#define MININT -100000000
#define SS(c,d) ss[(uchar)c][(uchar)d]
#define GAP(w,x,y,z) gop[(gtype[w]<<6)+(gtype[x]<<4)+(gtype[y]<<2)+gtype[z]]
#define DASH	(uchar)'-'
#define FREE	(uchar)'*'

typedef struct al {
  uchar *col;
  int colno;
  int subcol;
  int unchanged;
  struct al *next, *prev;
} *align_ptr, align_node;

typedef struct pi {
  uchar *seq;
  char *name;
  int len;
  int beg;	/* beginning point in its contig */
  int end;	/* ending point in its contig */
  int row;
  align_ptr firstcol;
  align_ptr lastcol;
} *piece_ptr, piece;

typedef struct tback {
	uchar *space;           /* array to be used. */
	unsigned int len;       /* length of the tback_space. */
} TB, *TBack;

/* entry point to robin.c */
align_ptr robin(align_ptr A, int nRow, piece_ptr P, int nP, int width,
  ss_t ss, int *gop, int *gtype, int max_iter, int max_depth, TBack tback);

/* entry points to scores.c */
void gap_costs(int *gop, int *gtype, int penalty);
int row_score(uchar **AL, int nRow, int nCol, int row, ss_t ss, int *gop,
  int *gtype);
int linked_row_score(align_ptr from, align_ptr to, int nRow, int row,
  ss_t ss, int *gop, int *gtype);
int align_score(align_ptr from, align_ptr to, int nRow, ss_t ss, int *gop,
  int *gtype);
int packed_score(uchar **AL, int nRow, int nCol, ss_t ss,  int *gop,
  int *gtype);


/* in yama.c */
int row_yama(uchar **AL, uchar **GC, int nRow, int nCol, uchar *seq, int len,
  int left_type, int right_type, int *LB, int *RB, ss_t ss, int *gop,
  int *gtype, int target_row, TBack tback, uchar ***AL_new, int *M_new);

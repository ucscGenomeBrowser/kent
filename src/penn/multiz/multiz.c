/* multiz.c -- scan through a human-mouse maf file.  Scan along each alignment
   for mouse-rat maf entries whose first row (mouse sequence) intersects
   the second row (mouse sequence) of the human-mouse alignment.  Between such
   matches, just print a segment of the human-mouse maf entry.  For each such
   mouse-rat alignment, re-align the human segment to the mouse-rat pairwise
   alignment, and report the resulting human-mouse-rat alignment.

   Besides the human-mouse and mouse-rat axt files, the command line can
   end with "verbose", to produce human-readable variants of axt files
*/

#include "refine.h"
#include "maf.h"

#define WIDTH 10
#define MIN_PRINT 10
#define TBACK_SIZE 50000000

struct mafFile *mf2;
static TBack tback;
static gap_scores_t ds;
static ss_t ss;
static int gop[256], gtype[128];
static int verbose = 0;

static char dna_compl[256];
static char dna_compl[256] = 
  "                                                                "
  " TVGH  CD  M KN   YSA BWXR       tvgh  cd  m kn   ysa bwxr      "
  "                                                                "
  "                                                                ";
/* ................................................................ */
/* @ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~. */
/* ................................................................ */
/* ................................................................ */

void do_revcompl(char *s, int len)
{
	char *p = s + len - 1;

	while (s<=p) {
		char c;

		c = dna_compl[(uchar)*s]; 
		*s = dna_compl[(uchar)*p]; 
		*p = c;
		++s, --p;
	}
}


/* find the alignment from the specified chromosome with the smallest
  end1 >= pos.  If no alignments on chr end at pos or later, return NULL */
struct mafAli *ali_next(char *src, int pos, char strand) {
	struct mafAli *a, *best = NULL;
	struct mafComp *c;
	int end, best_end;

	for (a = mf2->alignments; a; a = a->next) {
		c = a->components;
		if (c->strand ==  strand)
			end = c->start + c->size - 1;
		else
			end = c->srcSize - c->start - 1;
		if (same_string(c->src, src) && end >= pos &&
		    (best == NULL || end < best_end)) {
			best = a;
			best_end = end;
		}
	}

	if (best == NULL)
		return NULL;

	c = best->components;
	if (c->strand != strand)
		for ( ; c; c = c->next) {
			c->start = c->srcSize - (c->start + c->size);
			c->strand = (c->strand == '-' ? '+' : '-');
			do_revcompl(c->text, best->textSize);
		}
	return best;
}

struct mafComp *new_mafComp(char *src, int start, int size, char strand,
  int srcSize, int len) {
	struct mafComp *a = ckalloc(sizeof(struct mafComp));

	a->src = copy_string(src);
	a->start = start;
	a->size = size;
	a->strand = strand;
	a->srcSize = srcSize;
	a->text = ckalloc((len+1)*sizeof(char));
	return a;
}

// Write 3-way alignment to stdout.
void write_maf3(uchar **AL_new, int M0, int MM, int score,
  char *src1, int start1, int size1, char strand1, int srcSize1,
  char *src2, int start2, int size2, char strand2, int srcSize2,
  char *src3, int start3, int size3, char strand3, int srcSize3) {
	struct mafAli *a = ckalloc(sizeof(struct mafAli));
	struct mafComp *comp;
	int i, c, j;

	a->score = score;
	a->next = NULL;
	a->textSize = MM - M0 + 1;
	a->components = comp =
	   new_mafComp(src1, start1, size1, strand1, srcSize1, a->textSize);
	for (i = M0, j = 0; i <= MM; ++i, ++j) {
		c = AL_new[i][0];
		comp->text[j] = (c == '*' ? '-' : c);
	}
	comp->text[j] = '\0';

	comp = comp->next =
	   new_mafComp(src2, start2, size2, strand2, srcSize2, a->textSize);
	for (i = M0, j = 0; i <= MM; ++i, ++j) {
		c = AL_new[i][1];
		comp->text[j] = (c == '*' ? '-' : c);
	}
	comp->text[j] = '\0';

	comp = comp->next =
	   new_mafComp(src3, start3, size3, strand3, srcSize3, a->textSize);
	for (i = M0, j = 0; i <= MM; ++i, ++j) {
		c = AL_new[i][2];
		comp->text[j] = (c == '*' ? '-' : c);
	}
	comp->text[j] = '\0';
	comp->next = NULL;
	mafWrite(stdout, a);

	mafAliFree(&a);
}

// write pairwise alignment to stdout
void write_maf2(char *src1, int start1, int size1, char strand1, int srcSize1,
	  char *src2, int start2, int size2, char strand2, int srcSize2,
	  char *text1, char *text2, int col0, int col) {
	struct mafAli *a;
	struct mafComp *comp;
	int i, j, score;

	if (col - col0 < MIN_PRINT)
		return;
	a = ckalloc(sizeof(struct mafAli));

	for (score = 0, i = col0; i < col; ++i) {
		score += ss[(uchar)text1[i]][(uchar)text2[i]];
		if (i > col0 && (text1[i] == '-' || text2[i] == '-') &&
		    text1[i-1] != '-' && text2[i-1] != '-')
			score -= ds.O;
	}

	a->score = score;
	a->next = NULL;
	a->textSize = col - col0;
	a->components = comp =
	   new_mafComp(src1, start1, size1, strand1, srcSize1, a->textSize);
	for (i = col0, j = 0; i < col; ++i, ++j)
		comp->text[j] = text1[i];
	comp->text[j] = '\0';

	comp = comp->next =
	   new_mafComp(src2, start2, size2, strand2, srcSize2, a->textSize);
	for (i = col0, j = 0; i < col; ++i, ++j)
		comp->text[j] = text2[i];
	comp->text[j] = '\0';

	comp->next = NULL;
	mafWrite(stdout, a);

	mafAliFree(&a);
}

/* process_maf -- the procedure that does the real work, breaking a human-mouse
*	alignment into shorter human-mouse-rat alignments, separated by
*	human-mouse alignments
*/
void process_maf(struct mafAli *a) {
	int p, q, p0, q0, colp, colp0, col_mr_start, col_mr_end, j, k,
		mpos, mrcol, hmcol, rat_pos_start, rat_pos_end, width;
	char rat_strand, c;
	uchar *human;
	struct mafAli *m;
	struct mafComp *a1, *a2, *m1, *m2;
	int K, M, N, i, score, *LB, *RB, M_new, M0, MM,
	  aend1, aend2, mend1, mend2;
	uchar **AL, **GC, **AL_new;

/*
printf("processing:\n");
mafWrite(stdout, a);
*/

	a1 = a->components;
	aend1 = a1->start + a1->size - 1;
	a2 = a1->next;
	aend2 = a2->start + a2->size - 1;

for (i = j = 0; i < a->textSize; ++i)
if (a2->text[i] != '-')
   ++j;
if (j != a2->size)
fatalf("a2->size = %d, j = %d\n", a2->size, j);

	/* as we sweep along the human-mouse alignment:
	*	colp = column in human-mouse alignment
	*	p = position in human at column colp (or next, if '-' at colp)
	*	q = position in mouse at column colp (or next, if '-' at colp)
	*/
	for (colp = 0, p = a1->start, q = a2->start;
	    (m = ali_next(a2->src, q, a2->strand)) != NULL &&
	     m->components->start <= a2->start+a2->size-1;
	      q = m1->start + m1->size) {
		m1 = m->components;
		mend1 = m1->start + m1->size - 1;
		m2 = m1->next;
		mend2 = m2->start + m2->size - 1;
// fprintf(stderr, "mr maf width %d, mus %d-%d, rat %d-%d\n",
// m->textSize, m1->start, mend1, m2->start, mend2);

		/* This mouse-rat alignment might start in the mouse sequence
		*  before the human-mouse block, or end after it.  Find the
		*  first column and the column just after the matching part,
		*  along with the corresponding rat positions.
		*/
		col_mr_start = 0;
		rat_pos_start = m2->start;
		if (m1->start < q) {
			if (colp != 0) {
				fprintf(stderr, "%s %d %d: ",
					m1->src, m1->start, mend1);
				fatal("mouse-rat not single_cov");
			}
			for (mpos = m1->start, mrcol = 0; mpos < q; ++mrcol) {
				if ((c = m1->text[mrcol]) == '\0')
					fatal("scanned off end of mouse-rat");
				else if (c != '-')
					++mpos;
				if (m2->text[mrcol] != '-')
					++rat_pos_start;
			}
			col_mr_start = mrcol;
		}
		col_mr_end = m->textSize;
		rat_pos_end = mend2;
		if (mend1 > aend2) {
			for (rat_pos_end = m2->start, mpos = m1->start,
			   mrcol = 0; mpos <= aend2; ++mrcol) {
				if ((c = m1->text[mrcol]) == '\0')
					fatal("scanned past end of mouse-rat");
				else if (c != '-')
					++mpos;
				if (m2->text[mrcol] != '-')
					++rat_pos_end;
			}
			col_mr_end = mrcol;
			if (m2->text[mrcol - 1] != '-')
				--rat_pos_end;
		}
// fprintf(stderr, "col_mr_start = %d, col_mr_end = %d\n",
// col_mr_start, col_mr_end);
		if (m1->start > q) {
			/* print a 2-axt: human-mouse alignment */
			/* mark start of 2-axt */
			p0 = p;
			q0 = q;
			colp0 = colp;
			/* scan just beyond it */
			while (q < m1->start) {
				if (a1->text[colp] != '-')
					++p;
				if (a2->text[colp] != '-')
					++q;
				++colp;

			}
			write_maf2(a1->src, p0, p-p0, '+', a1->srcSize,
			  a2->src, q0, q-q0, a2->strand, a2->srcSize,
			  a1->text, a2->text, colp0, colp);
		}
		/* print a 3-axt: human-mouse-rat alignment */
		/* set p0, q0 and col0 to the start of the segment in the
		*  human-mouse alignment
		*/
		p0 = p;
		q0 = q;
		colp0 = colp;
		/* move p, q and colp just beyond the segment */
		while (colp < a->textSize && q <= mend1) {
			if (a1->text[colp] != '-')
				++p;
			if (a2->text[colp] != '-')
				++q;
			++colp;
		}

		/* are the two mouse sequences identical? */
		for (hmcol = colp0, mrcol = col_mr_start;
		     hmcol < colp && mrcol < col_mr_end; ++hmcol, ++mrcol) {
			while (a2->text[hmcol] == '-')
				++hmcol;
			while (m1->text[mrcol] == '-')
				++mrcol;
			if (hmcol < colp &&
			    a2->text[hmcol] != m1->text[mrcol]) {
				for (hmcol = colp0; hmcol < colp; ++hmcol)
					putchar(a2->text[hmcol]);
				putchar('\n');
				for (mrcol = col_mr_start; mrcol < col_mr_end;
				     ++mrcol)
					putchar(m1->text[mrcol]);
				putchar('\n');
				fatal("mismatching mouse entry");
			}
		}
		
		rat_strand = (a2->strand == m1->strand) ? '+' : '-';
		human = ckalloc((colp - colp0 + 1)*sizeof(uchar));
		for (hmcol = colp0, j = 0; hmcol < colp; ++hmcol)
			if ((c = a1->text[hmcol]) != '-')
				human[j++] = (uchar)c;
		human[j] = '\0';

		M = col_mr_end - col_mr_start;
		if (M < MIN_PRINT)
			continue;
		N = strlen((const char *)human);

		AL = (uchar **)ckalloc(M*sizeof(uchar *)) - 1;

		for (i = 1; i <= M; ++i) {
			AL[i] = ckalloc(2*sizeof(uchar));
			AL[i][0] = m1->text[col_mr_start+i-1];
			AL[i][1] = m2->text[col_mr_start+i-1];
		}

		GC = (uchar **)ckalloc((M+1)*sizeof(uchar *));
		for (i = 0; i <= M; ++i)
			GC[i] = (uchar *)ckalloc(2*sizeof(uchar));
		for (i = 0; i <= M; ++i)
			GC[i][0] = GC[i][1] = DASH;

		LB = ckalloc((M+1)*sizeof(int));
		RB = ckalloc((M+1)*sizeof(int));
		
		/* determine sausage in mus-hum D.P. grid (K rows) */
		for (K = 0, i = col_mr_start; i < col_mr_end; ++i)
			if (m1->text[i] != '-')
				++K;
		for (j = 0, i = colp0; i < colp; ++i)
			if (a2->text[i] != '-')
				++j;
		if (j != K)
			fatalf("%d mouse entries in *m, %d in *a", K, j);
		LB[0] = RB[0] = 0;

		/* remember; we're thinking of *a as a mouse-human alignment */
		for (i = j = 0, hmcol = colp0; hmcol < colp; ++hmcol)
			if (a2->text[hmcol] == '-') {	/* dash-over-x */
				++j;
				++RB[i];
			} else {
				++i;
				if (a1->text[hmcol] != '-')
					++j;
				LB[i] = RB[i] = j;
			}
		if (i != K)
			fatal("i != K");
		if (j != N)
			fatal("j != N");
		width = MIN(K, WIDTH);
		for (i = 0; i < K - width; ++i)
			RB[i] = MAX(MIN(RB[i]+width,N),RB[i+width]);
		for (i = K - width; i <= K; ++i)
			RB[i] = N;
		for (i = K; i > width; --i)
			LB[i] = MIN(MAX(LB[i]-width,0),LB[i-width]);
		for (i = width; i >= 0; --i)
			LB[i] = 0;

		/* spread sausage to the full M rows */
		for (j = K, i = M; i > 0; --i) {
			LB[i] = LB[j];
			RB[i] = RB[j];
			if (m1->text[col_mr_start+i-1] != '-')
				--j;
		}
		if (j != 0)
			fatalf("j = %d != 0", j);
		for (i = j = 0; i <= M; ++i)
			j += (RB[i]-LB[i]+1);
// fprintf(stderr, "Ave. diameter of sausage is %4.1f\n", (float)j/(float)i);

		score = row_yama(AL, GC, 2, M, human-1, N, 0, 0, LB, RB,
			ss, gop, gtype, 0, tback, &AL_new, &M_new);

		M0 = 1;
		MM = M_new;
		/* fix up gaps very near the ends */
		for (k = 1; k < 4; ++k)
			if (AL_new[1][k] != '-' &&
			   (AL_new[2][k] == '-' || AL_new[3][k] == '-')) {
// fprintf(stderr, "removed fluff at start of row %d\n", k);
				for (i = 3; i <= MM; ++i)
					if (AL_new[i][k] != '-')
						break;
				if (i > MM)
					fatalf("row %d is all dashes", k);
				for (j = --i; j > 0; --j)
					if (AL_new[j][k] != '-')
						AL_new[i--][k] = AL_new[j][k];
				while (i > 0)
					AL_new[i--][k] = '-';
			}
		if (AL_new[1][0] != '-') {
			for (k = 2; k < MM; ++k)
				if (AL_new[k][0] != '-')
					break;
			if (k > 4) {
				AL_new[k-1][0] = AL_new[1][0];
				AL_new[1][0] = '-';
			}
		}
		while (AL_new[M0][0] == '-' && AL_new[M0][1] == '-' &&
		    AL_new[M0][2] == '-')
			++M0;
		for (k = 0; k < 3; ++k)
			if (AL_new[MM][k] != '-' &&
			   (AL_new[MM-1][k] == '-' || AL_new[MM-2][k] == '-')) {
// fprintf(stderr, "removed fluff at end of row %d\n", k);
				for (i = MM-2; i > 0; --i)
					if (AL_new[i][k] != '-')
						break;
				if (i < 1)
					fatalf("row %d is all dashes", k);
				for (j = ++i; j <= MM; ++j)
					if (AL_new[j][k] != '-')
						AL_new[i++][k] = AL_new[j][k];
				while (i <= M_new)
					AL_new[i++][k] = '-';
			}
		if (AL_new[MM][0] == '-') {
			for (k = MM-1; k > 1; --k)
				if (AL_new[k][0] != '-')
					break;
			if (MM - k > 3) {
				AL_new[k+1][0] = AL_new[MM][0];
				AL_new[MM][0] = '-';
			}
		}
		while (AL_new[MM][0] == '-' && AL_new[MM][1] == '-' &&
		    AL_new[MM][2] == '-')
			--MM;

		if (verbose == 0)
			write_maf3(AL_new, M0, MM, score,
			  a1->src, p0, p-p0, '+', a1->srcSize,
			  a2->src, q0, q-q0, a2->strand, a2->srcSize,
			  m2->src, rat_pos_start, rat_pos_end-rat_pos_start+1,
			    rat_strand, m2->srcSize); 
		else
			for (i = M0; i <= MM; i += 50) {
				for (j = i; j <= MM && j < i+50; ++j) {
					c = AL_new[j][0];
					putchar(c == '*' ? '-' : c);
				}
				putchar('\n');
				for (k = 1; k <= 2; ++k) {
					for (j = i; j <= MM && j < i+50; ++j) {
						c = AL_new[j][k];
						if (c == '*')
							c = '-';
						else if (c == AL_new[j][0])
							c = '.';
						putchar(c);
					}
					putchar('\n');
				}
				putchar('\n');
			}

		free(AL_new[1]); free(AL_new+1);
		free(LB); free(RB);
		free(GC[0]);
		for (i = 1; i <= M; ++i) {
			free(AL[i]);
			free(GC[i]);
		}
		free(AL+1);
		free(GC);
		free(human);
	}
	if (colp < a->textSize)
		write_maf2(a1->src, p, aend1-p+1, '+', a1->srcSize,
		     a2->src, q, aend2-q+1, a2->strand, a2->srcSize,
		     a1->text, a2->text, colp, a->textSize+1);
}

int main(int argc, char **argv)
{
	struct mafAli *m;
	struct mafComp *c;
	struct mafFile *hmp;	/* human-mouse .maf file file */
	int i;

	if (argc == 4) {
		verbose = 1;
		--argc;
	}
	if (argc != 3)
		fatal("args: hum-mus-axt-file mus-rat-axt-file");


	dna_compl['-'] = '-';
	DNA_scores(ss);
	ds.E = 30;
	ds.O = 400;
	for (i = 0; i < 128; ++i)
		ss[i][DASH] = ss[DASH][i] = -ds.E;
	ss[DASH][DASH] = 0;
	for (i = 0; i < 128; ++i)
		ss[i][FREE] = ss[FREE][i] = 0;
	gap_costs(gop, gtype, ds.O);

	tback = ckalloc(sizeof(TB));
	tback->space = ckalloc(TBACK_SIZE*sizeof(uchar));
	tback->len = TBACK_SIZE;

	/* initialize for random access to mouse-rat axt files */
	mf2 = mafReadAll(argv[2]);
	for (m = mf2->alignments; m; m = m->next)
		if ((c = m->components) == NULL || c->strand != '+' ||
		    (c = c->next) == NULL || c->next != NULL)
			fatalf("%s contain a non-pairwise alignment", argv[2]); 

	mafWriteStart(stdout, "multiz");
	hmp = mafOpen(argv[1]);
	while ((m = mafNext(hmp)) != NULL) {
		if ((c = m->components) == NULL || c->strand != '+' ||
		    (c = c->next) == NULL || c->next != NULL)
			fatalf("%s contain a non-pairwise alignment", argv[1]); 
		process_maf(m);
		mafAliFree(&m);
	}
	mafFileFree(&mf2);
	mafWriteEnd(stdout);

	free(tback->space);
	free(tback);

	return 0;
}

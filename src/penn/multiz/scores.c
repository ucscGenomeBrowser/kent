/* RCSID("$Id: scores.c,v 1.1 2002/09/24 18:04:18 kent Exp $"); */

#include "refine.h"

void gap_costs(int *gop, int *gtype, int gap_open)
{
	int i, X, F, D;

	for (i = 0; i < 128; ++i)
		gtype[i] = 0;
	gtype[DASH] = 1;
	gtype[FREE] = 2;

	for (i = 0; i < 256; ++i)
		gop[i] = 0;
	D = DASH;
	F = FREE;
	X = (uchar)'A';
	GAP(X,X,X,D) = gap_open;
	GAP(X,X,D,X) = gap_open;
	GAP(X,F,D,X) = gap_open;
	GAP(X,D,D,X) = gap_open;
	GAP(F,X,X,D) = gap_open;
	GAP(F,D,X,D) = gap_open;
	GAP(D,X,X,D) = gap_open;
	GAP(D,F,D,X) = gap_open;
	GAP(D,D,X,D) = gap_open;
	GAP(D,D,D,X) = gap_open;
}

/*  AL is stored by columns, so AL[j][i] is the character at row i, column j.
*   Column indices run from 1 to nCol, inclusive. */
int row_score(uchar **AL, int nRow, int nCol, int row, ss_t ss, int *gop,
  int *gtype)
{
	int i, j, score;
	uchar *col, *pcol, ci, cr, pi, pr;

	score = 0;
	col = AL[1];
	cr = col[row];
	for (i = 0; i < nRow; ++i)
		if (i != row) {
			ci = col[i];
			score += SS(cr,ci);
		}
	for (j = 2; j <= nCol; ++j) {
		pcol = AL[j-1];
		col = AL[j];
		pr = pcol[row];
		cr = col[row];
		for (i = 0; i < nRow; ++i)
			if (i != row) {
				pi = pcol[i];
				ci = col[i];
			   	score += SS(cr, ci);
				score -= GAP(pr,pi,cr,ci);
		   	}
	}
	return score;
}

int linked_row_score(align_ptr ap, align_ptr to, int nRow, int row,
  ss_t ss, int *gop, int *gtype)
{
	align_ptr bp, end;
	uchar *acol, *bcol, ai, ar, bi, br;
	int i, score;

	score = 0;
	acol = ap->col;
	ar = acol[row];
	for (i = 0; i < nRow; ++i)
		if (i != row) {
			ai = acol[i];
			score += SS(ai,ar);
		}
	end = to->next;
	for (bp = ap->next; bp != end; ap = bp, bp = bp->next) {
		acol = ap->col;
		bcol = bp->col;
		ar = acol[row];
		br = bcol[row];
		for (i = 0; i < nRow; ++i)
			if (i != row) {
				ai = acol[i];
				bi = bcol[i];
			   	score += SS(br, bi);
				score -= GAP(ar,ai,br,bi);
		   	}
	}
	return score;
}

int align_score(align_ptr ap, align_ptr stop, int nRow, ss_t ss, int *gop,
int *gtype)
{
	align_ptr bp;
	uchar *acol, *bcol, ai, ar, bi, br;
	int row, i, score;

	score = 0;
	acol = ap->col;
	for (row = 0; row < nRow; ++row) {
		ar = acol[row];
		for (i = row+1; i < nRow; ++i) {
			ai = acol[i];
			score += SS(ai,ar);
		}
	}
	for (bp = ap->next; bp != stop->next; ap = bp, bp = bp->next) {
		acol = ap->col;
		bcol = bp->col;
		for (row = 0; row < nRow; ++row) {
			ar = acol[row];
			br = bcol[row];
			for (i = row + 1; i < nRow; ++i) {
				ai = acol[i];
				bi = bcol[i];
			   	score += SS(br, bi);
				score -= GAP(ar,ai,br,bi);
		   	}
		}
	}
	return score;
}

int packed_score(uchar **AL, int nRow, int nCol, ss_t ss,  int *gop,
  int *gtype)
{
	uchar *acol, *bcol, ai, ar, bi, br;
	int row, i, j, score;

	score = 0;
	acol = AL[1];
	for (row = 0; row < nRow; ++row) {
		ar = acol[row];
		for (i = row+1; i < nRow; ++i) {
			ai = acol[i];
			score += SS(ai,ar);
		}
	}
	for (j = 2; j <= nCol; ++j) {
		acol = AL[j-1];
		bcol = AL[j];
		for (row = 0; row < nRow; ++row) {
			ar = acol[row];
			br = bcol[row];
			for (i = row + 1; i < nRow; ++i) {
				ai = acol[i];
				bi = bcol[i];
			   	score += SS(br, bi);
				score -= GAP(ar,ai,br,bi);
		   	}
		}
	}
	return score;
}

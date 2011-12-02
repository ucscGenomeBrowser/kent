/* Score mafs and subsets of maf. 
 * This module is from Webb Miller at PSU. 
 * Some description of maf scoring is included in hgLoadMaf.c comments*/

#include "common.h"
#include "maf.h"



typedef struct gap_scores {
	int E;
	int O;
} gap_scores_t;

#define CLEN(s) (sizeof((s))-1)
#define NACHARS 128
#define SS(c,d) ss[(uchar)c][(uchar)d]
#define GAP(w,x,y,z) gop[(gtype[w]<<6)+(gtype[x]<<4)+(gtype[y]<<2)+gtype[z]]
#define DASH '-'

typedef int ss_t[NACHARS][NACHARS];
typedef unsigned char uchar;

static ss_t ss;
static gap_scores_t ds;
static int gop[256], gtype[128];

static const uchar nchars[] = "ACGT";
static const int HOXD70_sym[4][4] = {
  {  91, -114,  -31, -123 },
  {-114,  100, -125,  -31 },
  { -31, -125,  100, -114 },
  {-123,  -31, -114,   91 },
};

/* DNA_scores --------------------------  substitution scoring matrix for DNA */
static void DNA_scores(ss_t ss)
{
	int i, j, bad, a, b, A, B;

	for (i = 0; i < NACHARS; ++i)
		for (j = 0; j < NACHARS; ++j)
			ss[i][j] = -100;
	for (i = 0; i < (signed)CLEN(nchars); ++i) {
		A = nchars[i];
		a = tolower(A);
		for (j = 0; j < (signed)CLEN(nchars); ++j) {
			B = nchars[j];
			b = tolower(B);
			ss[A][B] = ss[a][B] = ss[A][b] = ss[a][b] =
				HOXD70_sym[i][j];
		}
	}
	bad = -1000;
	for (i = 0; i < NACHARS; ++i)
		ss['X'][i] = ss[i]['X'] = ss['x'][i] = ss[i]['x'] = bad;
}


static void gap_costs(int *gop, int *gtype, int gap_open)
{
	int i, X, D;

	for (i = 0; i < 128; ++i)
		gtype[i] = 0;
	D = DASH;
	gtype[D] = 1;

	for (i = 0; i < 256; ++i)
		gop[i] = 0;
	X = (uchar)'A';
	GAP(X,X,X,D) = gap_open;
	GAP(X,X,D,X) = gap_open;
	GAP(X,D,D,X) = gap_open;
	GAP(D,X,X,D) = gap_open;
	GAP(D,D,X,D) = gap_open;
	GAP(D,D,D,X) = gap_open;
}

double mafScoreRangeMultiz(struct mafAli *maf, int start, int size)
/* Return score of a subset of an alignment.  Parameters are:
 *    maf - the alignment
 *    start - the (zero based) offset to start calculating score
 *    size - the size of the subset
 * The following relationship should hold:
 *   scoreRange(maf,start,size) =
 *	scoreRange(maf,0,start+size) - scoreRange(maf,0,start)
 */
{
uchar ai, ar, bi, br;
int i;
double score;
struct mafComp *c1, *c2;

if (start < 0 || size <= 0 || 
    start+size > maf->textSize) {
	errAbort( "mafScoreRange: start = %d, size = %d, textSize = %d\n",
		start, size, maf->textSize);
}
if (ss['A']['A'] != HOXD70_sym[0][0]) {
	DNA_scores(ss);
	ds.E = 30;
	ds.O = 400;
	for (i = 0; i < 128; ++i)
		ss[i][DASH] = ss[DASH][i] = -ds.E;
	ss[DASH][DASH] = 0;
	gap_costs(gop, gtype, ds.O);   /* quasi-natural gap costs */
}
score = 0.0;
for (i = start; i < start+size; ++i) {
	for (c1 = maf->components; c1 != NULL; c1 = c1->next) {
		if (c1->size == 0) continue;
		br = c1->text[i];
		for (c2 = c1->next; c2 != NULL; c2 = c2->next) {
			if (c2->size == 0) continue;
			bi = c2->text[i];
			score += SS(br, bi);
			if (i > 0) {
				ar = c1->text[i-1];
				ai = c2->text[i-1];
				score -= GAP(ar,ai,br,bi);
			}
		}
	}
}
return score;
}

double mafScoreMultiz(struct mafAli *maf)
/* Return score of a maf (calculated rather than what is
 * stored in the structure. */
{
return mafScoreRangeMultiz(maf, 0, maf->textSize);
}

double mafScoreMultizMaxCol(int species)
/* Return maximum possible score for a column. */
{
int i, count = 0;
for (i=1; i<species; ++i)
    count += i;
return 100.0*count; 
}

void mafColMinMaxScore(struct mafAli *maf, 
	double *retMin, double *retMax)
/* Get min/max maf scores for a column. */
{
*retMax = mafScoreMultizMaxCol(slCount(maf->components));
*retMin = -*retMax;
}


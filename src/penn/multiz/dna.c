#include "util.h"
#include "seq.h"
#include "dna.h"

#ifndef __lint
static const char rcsid[] =
"$Id: dna.c,v 1.1 2002/09/24 18:04:17 kent Exp $";
#endif

static const uchar nchars[] = "ACGT";
static const int HOXD70_sym[4][4] = {
  {  91, -114,  -31, -123 },
  {-114,  100, -125,  -31 },
  { -31, -125,  100, -114 },
  {-123,  -31, -114,   91 },
};
#define CLEN(s) (sizeof((s))-1)

/* DNA characters */
const uchar dchars[] = "ABCDGHKMNRSTVWXY";

/* DNA_scores --------------------------  substitution scoring matrix for DNA */
void DNA_scores(ss_t ss)
{
	int i, j, bad, a, b, A, B;

	for (i = 0; i < NACHARS; ++i)
		for (j = 0; j < NACHARS; ++j)
			ss[i][j] = -100;
	for (i = 0; i < CLEN(nchars); ++i) {
		A = nchars[i];
		a = tolower(A);
		for (j = 0; j < CLEN(nchars); ++j) {
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

int is_dchar(int ch)
{
	return !!strchr((const char*)dchars, toupper(ch));
}

static int scores_from_fp(FILE *fp, ss_t ss)
{
	char buf[1024];
	uchar code[255];
	char *p;
	int n, i, j, s;

	if (fgets(buf, sizeof buf, fp) == 0)
		fatal("cannot read score file");

	/* XXX - strtok isn't thread safe */
	for (n=0, p=strtok(buf, " \t\n"); p; p=strtok(0, " \t\n"), ++n) {
		code[n] = *(uchar*)p;
	}

	/*
	 * This alters the values in ss as indicated by the file.
	 * Conceivably one might want to overwrite the whole thing,
	 * setting values for irregular = A vs C and bad = 100 * irregular;
	 */

	for (i=0; i < n; ++i) {
		for (j=0; j < n; ++j) {
			if (fscanf(fp, " %d", &s) == 1) {
				ss[code[i]][code[j]] = s;
			} else {
				fatal("cannot read score file");
			}
		}
	}
	return 1;
}

int scores_from_file(const char *fname, ss_t ss)
{
	FILE *fp;
	int rval;

	fp = ckopen(fname, "r");
	rval = scores_from_fp(fp, ss);
	fclose(fp);
	return rval;
}

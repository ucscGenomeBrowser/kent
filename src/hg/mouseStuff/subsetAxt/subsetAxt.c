/* subsetAxt - Rescore alignments and output those over threshold. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "subsetAxt - Rescore alignments and output those over threshold\n"
  "usage:\n"
  "   subsetAxt in.axt out.axt matrix threshold\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct axt
/* This contains information about one xeno alignment. */
    {
    struct axt *next;
    char *qName;
    int qStart, qEnd;
    char qStrand;
    char *tName;
    int tStart, tEnd;
    char tStrand;
    int score;
    int symCount;
    char *qSym, *tSym;
    };

void axtFree(struct axt **pEl)
/* Free axt. */
{
struct axt *el = *pEl;
if (el != NULL)
    {
    freeMem(el->qName);
    freeMem(el->tName);
    freeMem(el->qSym);
    freeMem(el->tSym);
    freez(pEl);
    }
}

struct scoreScheme
/* A scoring scheme or DNA alignment. */
    {
    struct scoreMatrix *next;
    int matrix[5][5];   /* Look up with A_BASE_VAL etc. */
    int gapOpen;	/* Gap open cost. */
    int gapExtend;	/* Gap extension. */
    };

void shortScoreScheme(struct lineFile *lf)
/* Complain about score file being to short. */
{
errAbort("Scoring matrix file %s too short\n", lf->fileName);
}

struct scoreScheme *scoreSchemeRead(char *fileName)
/* Read in scoring scheme from file. Looks like
    A    C    G    T
    91 -114  -31 -123
    -114  100 -125  -31
    -31 -125  100 -114
    -123  -31 -114   91
    O = 400, E = 30
*/
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *row[4], *parts[32];
static int trans[4] = {A_BASE_VAL, C_BASE_VAL, G_BASE_VAL, T_BASE_VAL};
int i,j, partCount;
struct scoreScheme *ss;
boolean gotO = FALSE, gotE = FALSE;

AllocVar(ss);
if (!lineFileRow(lf, row))
    shortScoreScheme(lf);
if (row[0][0] != 'A' || row[1][0] != 'C' || row[2][0] != 'G' 
	|| row[3][0] != 'T')
    errAbort("%s doesn't seem to be a score matrix file", lf->fileName);
for (i=0; i<4; ++i)
    {
    if (!lineFileRow(lf, row))
	shortScoreScheme(lf);
    for (j=0; j<4; ++j)
	ss->matrix[trans[i]][trans[j]] = lineFileNeedNum(lf, row, j);
    }
lineFileNeedNext(lf, &line, NULL);
partCount = chopString(line, " =,\t", parts, ArraySize(parts));
for (i=0; i<partCount-1; i += 2)
    {
    if (sameString(parts[i], "O"))
        {
	gotO = TRUE;
	ss->gapOpen = atoi(parts[i+1]);
	}
    if (sameString(parts[i], "E"))
        {
	gotE = TRUE;
	ss->gapExtend = atoi(parts[i+1]);
	}
    }
if (!gotO || !gotE)
    errAbort("Expecting O = and E = in last line of %s", lf->fileName);
if (ss->gapOpen <= 0 || ss->gapExtend <= 0)
    errAbort("Must have positive gap scores");
return ss;
}


struct axt *axtNext(struct lineFile *lf)
/* Read in next record from .axt file and return it.
 * Returns NULL at EOF. */
{
char *words[10], *line;
int wordCount, symCount;
struct axt *axt;

wordCount = lineFileChop(lf, words);
if (wordCount <= 0)
    return NULL;
if (wordCount < 8)
    errAbort("Expecting at least 8 words line %d of %s\n", lf->lineIx, lf->fileName);
AllocVar(axt);

axt->qName = cloneString(words[4]);
axt->qStart = lineFileNeedNum(lf, words, 5) - 1;
axt->qEnd = lineFileNeedNum(lf, words, 6);
axt->qStrand = words[7][0];
axt->tName = cloneString(words[1]);
axt->tStart = lineFileNeedNum(lf, words, 2) - 1;
axt->tEnd = lineFileNeedNum(lf, words, 3);
axt->tStrand = words[7][1];
if (wordCount > 8)
    axt->score = lineFileNeedNum(lf, words, 8);
if (axt->tStrand == 0)
    axt->tStrand = '+';
lineFileNeedNext(lf, &line, NULL);
axt->symCount = symCount = strlen(line);
axt->tSym = cloneMem(line, symCount+1);
lineFileNeedNext(lf, &line, NULL);
if (strlen(line) != symCount)
    errAbort("Symbol count inconsistent between sequences line %d of %s",
    	lf->lineIx, lf->fileName);
axt->qSym = cloneMem(line, symCount+1);
lineFileNext(lf, &line, NULL);	/* Skip blank line */
return axt;
}

int countNonDash(char *a, int size)
/* Count number of non-dash characters. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (a[i] != '-') 
        ++count;
return count;
}

static int baseVal[256];
/* Table to look up integer values for bases. */

void initBaseVal()
/* Init base val array */
{
int i;
for (i=0; i<ArraySize(baseVal); ++i)
    baseVal[i] = N_BASE_VAL;
baseVal['a'] = baseVal['A'] = A_BASE_VAL;
baseVal['c'] = baseVal['C'] = C_BASE_VAL;
baseVal['g'] = baseVal['G'] = G_BASE_VAL;
baseVal['t'] = baseVal['T'] = T_BASE_VAL;
}

boolean findHsp(char *as, char *bs, int size, 
	struct scoreScheme *ss, int minScore,
	int *retStart, int *retEnd, int *retScore)
/* Find first HSP between a and b. */
{
int start = 0, best = 0, bestScore = 0, end;
int score = 0;
char a,b;
boolean lastGap = FALSE;
int gapStart = ss->gapOpen;
int gapExt = ss->gapExtend;

for (end = 0; end < size; ++end)
    {
    a = as[end];
    b = bs[end];
    if (a == '-' || b == '-')
        {
	if (lastGap)
	    score -= gapExt;
	else
	    {
	    score -= gapStart;
	    lastGap = TRUE;
	    }
	}
    else
        {
	score += ss->matrix[baseVal[a]][baseVal[b]];
	lastGap = FALSE;
	}
    if (score > bestScore)
        {
	bestScore = score;
	best = end;
	}
    if (score < 0 || end == size-1)
        {
	if (bestScore >= minScore)
	    {
	    *retStart = start;
	    *retEnd = best+1;
	    *retScore = bestScore;
	    return TRUE;
	    }
	start = end+1;
	score = 0;
	}
    }
return FALSE;
}

void axtWrite(struct axt *axt, FILE *f)
/* Output axt to axt file. */
{
static int ix = 0;
fprintf(f, "%d %s %d %d %s %d %d %c",
	++ix, axt->tName, axt->tStart+1, axt->tEnd, 
	axt->qName, axt->qStart+1, axt->qEnd, axt->qStrand);
if (axt->tStrand == '-')
    fprintf(f, "%c", axt->tStrand);
fprintf(f, " %d", axt->score);
fputc('\n', f);
mustWrite(f, axt->tSym, axt->symCount);
fputc('\n', f);
mustWrite(f, axt->qSym, axt->symCount);
fputc('\n', f);
fputc('\n', f);
}

void outputSubAxt(FILE *f, struct axt *axt, int start, int size, int score)
/* Output subset of axt to axt file. */
{
struct axt a;
a = *axt;
a.symCount = size;
a.score = score;
a.qStart += countNonDash(a.qSym, start);
a.qEnd = a.qStart + countNonDash(a.qSym + start, size);
a.tStart += countNonDash(a.tSym, start);
a.tEnd = a.tStart + countNonDash(a.tSym + start, size);
a.qSym += start;
a.tSym += start;
axtWrite(&a, f);
}

void subsetOne(struct axt *axt, struct scoreScheme *ss, 
	int threshold, FILE *f)
/* Find subsets of axt that are over threshold and output. */
{
char *q = axt->qSym, *t = axt->tSym;
int size = axt->symCount, offset = 0;
int start, end, score;

while (findHsp(q + offset, t + offset, size - offset, ss, threshold,
	&start, &end, &score))
    {
    assert(start < end);
    assert(end <= size - offset);
    outputSubAxt(f, axt, start+offset, end-start, score);
    offset += end;
    if (offset >= size)
        break;
    }
}

void subsetAxt(char *inName, char *outName, char *scoreFile, int threshold)
/* subsetAxt - Rescore alignments and output those over threshold. */
{
struct scoreScheme *ss = scoreSchemeRead(scoreFile);
struct lineFile *lf = lineFileOpen(inName, TRUE);
FILE *f = mustOpen(outName, "w");
struct axt *axt;

initBaseVal();
if (threshold <= 0)
    errAbort("Threshold must be a positive number");
while ((axt = axtNext(lf)) != NULL)
    {
    subsetOne(axt, ss, threshold, f);
    axtFree(&axt);
    axt = NULL;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
subsetAxt(argv[1], argv[2], argv[3], atoi(argv[4]));
return 0;
}

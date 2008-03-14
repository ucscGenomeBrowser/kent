/* AXT - A simple alignment format with four lines per
 * alignment.  The first specifies the names of the
 * two sequences that align and the position of the
 * alignment, as well as the strand.  The next two
 * lines contain the alignment itself including dashes
 * for inserts.  The alignment is separated from the
 * next alignment with a blank line. 
 *
 * This file contains routines to read such alignments.
 * Note that though the coordinates are one based and
 * closed on disk, they get converted to our usual half
 * open zero based in memory. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "dnautil.h"
#include "axt.h"

static char const rcsid[] = "$Id: axt.c,v 1.49 2008/03/14 04:27:53 angie Exp $";

void axtFree(struct axt **pEl)
/* Free an axt. */
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

void axtFreeList(struct axt **pList)
/* Free a list of dynamically allocated axt's */
{
struct axt *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    axtFree(&el);
    }
*pList = NULL;
}


struct axt *axtReadWithPos(struct lineFile *lf, off_t *retOffset)
/* Read next axt, and if retOffset is not-NULL, fill it with
 * offset of start of axt. */
{
char *words[10], *line;
int wordCount, symCount;
struct axt *axt;

wordCount = lineFileChop(lf, words);
if (retOffset != NULL)
    *retOffset = lineFileTell(lf);
if (wordCount <= 0)
    return NULL;
if (wordCount < 8)
    {
    errAbort("Expecting at least 8 words line %d of %s got %d\n", lf->lineIx, lf->fileName,
    	wordCount);
    }
AllocVar(axt);

axt->qName = cloneString(words[4]);
axt->qStart = lineFileNeedNum(lf, words, 5) - 1;
axt->qEnd = lineFileNeedNum(lf, words, 6);
axt->qStrand = words[7][0];
axt->tName = cloneString(words[1]);
axt->tStart = lineFileNeedNum(lf, words, 2) - 1;
axt->tEnd = lineFileNeedNum(lf, words, 3);
axt->tStrand = '+';
if (wordCount > 8)
    axt->score = lineFileNeedNum(lf, words, 8);
lineFileNeedNext(lf, &line, NULL);
axt->symCount = symCount = strlen(line);
axt->tSym = cloneMem(line, symCount+1);
lineFileNeedNext(lf, &line, NULL);
if (strlen(line) != symCount)
    errAbort("Symbol count %d != %d inconsistent between sequences line %d and prev line of %s",
    	symCount, (int)strlen(line), lf->lineIx, lf->fileName);
axt->qSym = cloneMem(line, symCount+1);
lineFileNext(lf, &line, NULL);	/* Skip blank line */
return axt;
}

struct axt *axtRead(struct lineFile *lf)
/* Read in next record from .axt file and return it.
 * Returns NULL at EOF. */
{
return axtReadWithPos(lf, NULL);
}

void axtWrite(struct axt *axt, FILE *f)
/* Output axt to axt file. */
{
static int ix = 0;
fprintf(f, "%d %s %d %d %s %d %d %c",
	ix++, axt->tName, axt->tStart+1, axt->tEnd, 
	axt->qName, axt->qStart+1, axt->qEnd, axt->qStrand);
fprintf(f, " %d", axt->score);
fputc('\n', f);
mustWrite(f, axt->tSym, axt->symCount);
fputc('\n', f);
mustWrite(f, axt->qSym, axt->symCount);
fputc('\n', f);
fputc('\n', f);
if ((strlen(axt->tSym) != strlen(axt->qSym)) || (axt->symCount > strlen(axt->tSym)))
    fprintf(stderr,"Symbol count %d != %d || %d > %d inconsistent in %s in "
	    "record %d.\n",
	    (int)strlen(axt->qSym), (int)strlen(axt->tSym), axt->symCount,
	    (int)strlen(axt->tSym), axt->qName, ix);
}

int axtCmpQuery(const void *va, const void *vb)
/* Compare to sort based on query position. */
{
const struct axt *a = *((struct axt **)va);
const struct axt *b = *((struct axt **)vb);
int dif;
dif = strcmp(a->qName, b->qName);
if (dif == 0)
    dif = a->qStart - b->qStart;
return dif;
}

int axtCmpTarget(const void *va, const void *vb)
/* Compare to sort based on target position. */
{
const struct axt *a = *((struct axt **)va);
const struct axt *b = *((struct axt **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = a->tStart - b->tStart;
return dif;
}

int axtCmpScore(const void *va, const void *vb)
/* Compare to sort based on score. */
{
const struct axt *a = *((struct axt **)va);
const struct axt *b = *((struct axt **)vb);
return b->score - a->score;
}

int axtCmpTargetScoreDesc(const void *va, const void *vb)
/* Compare to sort based on target name and score descending. */
{
const struct axt *a = *((struct axt **)va);
const struct axt *b = *((struct axt **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = b->score - a->score;
return dif;
}

boolean axtCheck(struct axt *axt, struct lineFile *lf)
/* Return FALSE if there's a problem with axt. */
{
int tSize = countNonDash(axt->tSym, axt->symCount);
int qSize = countNonDash(axt->qSym, axt->symCount);
if (tSize != axt->tEnd - axt->tStart)
    {
    warn("%d non-dashes, but %d bases to cover at line %d of %s", 
    	tSize, axt->tEnd - axt->tStart, lf->lineIx, lf->fileName);
    return FALSE;
    }
if (qSize != axt->qEnd - axt->qStart)
    {
    warn("%d non-dashes, but %d bases to cover at line %d of %s", 
    	tSize, axt->qEnd - axt->qStart, lf->lineIx, lf->fileName);
    return FALSE;
    }
return TRUE;
}

int axtScoreUngapped(struct axtScoreScheme *ss, char *q, char *t, int size)
/* Score ungapped alignment. */
{
int score = 0;
int i;
for (i=0; i<size; ++i)
    score += ss->matrix[(int)q[i]][(int)t[i]];
return score;
}

int axtScoreSym(struct axtScoreScheme *ss, int symCount, char *qSym, char *tSym)
/* Return score without setting up an axt structure. */
{
int i;
char q,t;
int score = 0;
boolean lastGap = FALSE;
int gapStart = ss->gapOpen;
int gapExt = ss->gapExtend;

dnaUtilOpen();
for (i=0; i<symCount; ++i)
    {
    q = qSym[i];
    t = tSym[i];
    if (q == '-' || t == '-')
        {
	if (lastGap)
	    score -= gapExt;
	else
	    {
	    /* Use gapStart+gapExt to be consistent with blastz: */
	    score -= (gapStart + gapExt);
	    lastGap = TRUE;
	    }
	}
    else
        {
	score += ss->matrix[(int)q][(int)t];
	lastGap = FALSE;
	}
    }
return score;
}

boolean gapNotMasked(char q, char t)
/* return true if gap on one side and upper case on other side */
{
if (q=='-' && t=='-')
    return FALSE;
if (q=='-' && t<'a')
    return TRUE;
if (t=='-' && q<'a')
    return TRUE;
return FALSE;
}


int axtScoreSymFilterRepeats(struct axtScoreScheme *ss, int symCount, char *qSym, char *tSym)
/* Return score without setting up an axt structure. Do not penalize gaps if repeat masked (lowercase)*/
{
int i;
char q,t;
int score = 0;
boolean lastGap = FALSE;
int gapStart = ss->gapOpen;
int gapExt = ss->gapExtend;

dnaUtilOpen();
for (i=0; i<symCount; ++i)
    {
    q = qSym[i];
    t = tSym[i];
    if ((q == '-' || t == '-') && gapNotMasked(q,t))
        {
	if (lastGap)
	    score -= gapExt;
	else
	    {
	    /* Use gapStart+gapExt to be consistent with blastz: */
	    score -= (gapStart + gapExt);
	    lastGap = TRUE;
	    }
	}
    else
        {
	score += ss->matrix[(int)q][(int)t];
	lastGap = FALSE;
	}
    }
return score;
}

int axtScoreFilterRepeats(struct axt *axt, struct axtScoreScheme *ss)
/* Return calculated score of axt. */
{
return axtScoreSymFilterRepeats(ss, axt->symCount, axt->qSym, axt->tSym);
}

int axtScore(struct axt *axt, struct axtScoreScheme *ss)
/* Return calculated score of axt. */
{
return axtScoreSym(ss, axt->symCount, axt->qSym, axt->tSym);
}

int axtScoreDnaDefault(struct axt *axt)
/* Score DNA-based axt using default scheme. */
{
static struct axtScoreScheme *ss;
if (ss == NULL)
    ss = axtScoreSchemeDefault();
return axtScore(axt, ss);
}

int axtScoreProteinDefault(struct axt *axt)
/* Score protein-based axt using default scheme. */
{
static struct axtScoreScheme *ss;
if (ss == NULL)
    ss = axtScoreSchemeProteinDefault();
return axtScore(axt, ss);
}

boolean axtGetSubsetOnT(struct axt *axt, struct axt *axtOut,
			int newStart, int newEnd, struct axtScoreScheme *ss,
			boolean includeEdgeGaps)
/* Return FALSE if axt is not in the new range.  Otherwise, set axtOut to
 * a subset that goes from newStart to newEnd in target coordinates. 
 * If includeEdgeGaps, don't trim target gaps before or after the range. */
{
if (axt == NULL)
    return FALSE;
if (newStart < axt->tStart)
    newStart = axt->tStart;
if (newEnd > axt->tEnd)
    newEnd = axt->tEnd;
if (newEnd <= newStart) 
    return FALSE;
if (newStart == axt->tStart && newEnd == axt->tEnd)
    {
    axt->score = axtScore(axt, ss);
    *axtOut = *axt;
    return TRUE;
    }
else
    {
    struct axt a = *axt;
    char *tSymStart = skipIgnoringDash(a.tSym, newStart - a.tStart, TRUE);
    char *tSymEnd = skipIgnoringDash(tSymStart, newEnd - newStart, FALSE);
    if (includeEdgeGaps)
	{
	while (tSymStart > a.tSym)
	    if (*(--tSymStart) != '-')
		{
		tSymStart++;
		break;
		}
	while (tSymEnd < a.tSym + newEnd)
	    if (*(++tSymEnd) != '-')
		{
		tSymEnd--;
		break;
		}
	}
    int symCount = tSymEnd - tSymStart;
    char *qSymStart = a.qSym + (tSymStart - a.tSym);
    a.qStart += countNonDash(a.qSym, qSymStart - a.qSym);
    a.qEnd = a.qStart + countNonDash(qSymStart, symCount);
    a.tStart = newStart;
    a.tEnd = newEnd;
    a.symCount = symCount;
    a.qSym = qSymStart;
    a.tSym = tSymStart;
    a.score = axtScore(&a, ss);
    if ((a.qStart < a.qEnd && a.tStart < a.tEnd) ||
	(includeEdgeGaps && (a.qStart < a.qEnd || a.tStart < a.tEnd)))
	{
	*axtOut = a;
	return TRUE;
	}
    return FALSE;
    }
}

void axtSubsetOnT(struct axt *axt, int newStart, int newEnd, 
	struct axtScoreScheme *ss, FILE *f)
/* Write out subset of axt that goes from newStart to newEnd
 * in target coordinates. */
{
struct axt axtSub;
if (axtGetSubsetOnT(axt, &axtSub, newStart, newEnd, ss, FALSE))
    axtWrite(&axtSub, f);
}

int axtTransPosToQ(struct axt *axt, int tPos)
/* Convert from t to q coordinates */
{
char *tSym = skipIgnoringDash(axt->tSym, tPos - axt->tStart, TRUE);
int symIx = tSym - axt->tSym;
int qPos = countNonDash(axt->qSym, symIx);
return qPos + axt->qStart;
}

static void shortScoreScheme(struct lineFile *lf)
/* Complain about score file being to short. */
{
errAbort("Scoring matrix file %s too short\n", lf->fileName);
}

static void propagateCase(struct axtScoreScheme *ss)
/* Propagate score matrix from lower case to mixed case
 * in matrix. */
{
static int twoCase[2][4] = {{'a', 'c', 'g', 't'},{'A','C','G','T'},};
int i1,i2,j1,j2;

/* Propagate to other case combinations. */
for (i1=0; i1<=1; ++i1)
    for (i2=0; i2<=1; ++i2)
       {
       if (i1 == 0 && i2 == 0)
           continue;
       for (j1=0; j1<4; ++j1)
	   for (j2=0; j2<4; ++j2)
	      {
	      ss->matrix[twoCase[i1][j1]][twoCase[i2][j2]] = ss->matrix[twoCase[0][j1]][twoCase[0][j2]];
	      }
       }
}

struct axtScoreScheme *axtScoreSchemeDefault()
/* Return default scoring scheme (after blastz).  Do NOT axtScoreSchemeFree
 * this. */
{
static struct axtScoreScheme *ss;

if (ss != NULL)
    return ss;
AllocVar(ss);

/* Set up lower case elements of matrix. */
ss->matrix['a']['a'] = 91;
ss->matrix['a']['c'] = -114;
ss->matrix['a']['g'] = -31;
ss->matrix['a']['t'] = -123;

ss->matrix['c']['a'] = -114;
ss->matrix['c']['c'] = 100;
ss->matrix['c']['g'] = -125;
ss->matrix['c']['t'] = -31;

ss->matrix['g']['a'] = -31;
ss->matrix['g']['c'] = -125;
ss->matrix['g']['g'] = 100;
ss->matrix['g']['t'] = -114;

ss->matrix['t']['a'] = -123;
ss->matrix['t']['c'] = -31;
ss->matrix['t']['g'] = -114;
ss->matrix['t']['t'] = 91;

propagateCase(ss);
ss->gapOpen = 400;
ss->gapExtend = 30;
return ss;
}

struct axtScoreScheme *axtScoreSchemeSimpleDna(int match, int misMatch, int gapOpen, int gapExtend)
/* Return a relatively simple scoring scheme for DNA. */
{
static struct axtScoreScheme *ss;
AllocVar(ss);

/* Set up lower case elements of matrix. */
ss->matrix['a']['a'] = match;
ss->matrix['a']['c'] = -misMatch;
ss->matrix['a']['g'] = -misMatch;
ss->matrix['a']['t'] = -misMatch;

ss->matrix['c']['a'] = -misMatch;
ss->matrix['c']['c'] = match;
ss->matrix['c']['g'] = -misMatch;
ss->matrix['c']['t'] = -misMatch;

ss->matrix['g']['a'] = -misMatch;
ss->matrix['g']['c'] = -misMatch;
ss->matrix['g']['g'] = match;
ss->matrix['g']['t'] = -misMatch;

ss->matrix['t']['a'] = -misMatch;
ss->matrix['t']['c'] = -misMatch;
ss->matrix['t']['g'] = -misMatch;
ss->matrix['t']['t'] = match;

propagateCase(ss);
ss->gapOpen = gapOpen;
ss->gapExtend = gapExtend;
return ss;
}

struct axtScoreScheme *axtScoreSchemeRnaDefault()
/* Return default scoring scheme for RNA/DNA alignments
 * within the same species.  Do NOT axtScoreSchemeFree */
{
static struct axtScoreScheme *ss;
if (ss == NULL)
    ss = axtScoreSchemeSimpleDna(100, 200, 300, 300);
return ss;
}

struct axtScoreScheme *axtScoreSchemeRnaFill()
/* Return scoreing scheme a little more relaxed than 
 * RNA/DNA defaults for filling in gaps. */
{
static struct axtScoreScheme *ss;
if (ss == NULL)
    ss = axtScoreSchemeSimpleDna(100, 100, 200, 200);
return ss;
}

struct axtScoreScheme *axtScoreSchemeFromBlastzMatrix(char *text, int gapOpen, int gapExtend)
/* return scoring schema from a string in the following format */
/* 91,-90,-25,-100,-90,100,-100,-25,-25,-100,100,-90,-100,-25,-90,91 */
{
char *matrixDna[32];
struct axtScoreScheme *ss = axtScoreSchemeDefault();
int matrixSize = chopString(text, ",", matrixDna, 32);
if (matrixSize != 16)
    return ss;
if (ss == NULL)
    return NULL;
ss->gapOpen = gapOpen;
ss->gapExtend = gapExtend;
ss->matrix['a']['a'] = atoi(matrixDna[0]);
ss->matrix['a']['c'] = atoi(matrixDna[1]);
ss->matrix['a']['g'] = atoi(matrixDna[2]);
ss->matrix['a']['t'] = atoi(matrixDna[3]);

ss->matrix['c']['a'] = atoi(matrixDna[4]);
ss->matrix['c']['c'] = atoi(matrixDna[5]);
ss->matrix['c']['g'] = atoi(matrixDna[6]);
ss->matrix['c']['t'] = atoi(matrixDna[7]);

ss->matrix['g']['a'] = atoi(matrixDna[8]);
ss->matrix['g']['c'] = atoi(matrixDna[9]);
ss->matrix['g']['g'] = atoi(matrixDna[10]);
ss->matrix['g']['t'] = atoi(matrixDna[11]);

ss->matrix['t']['a'] = atoi(matrixDna[12]);
ss->matrix['t']['c'] = atoi(matrixDna[13]);
ss->matrix['t']['g'] = atoi(matrixDna[14]);
ss->matrix['t']['t'] = atoi(matrixDna[15]);
return ss;
}

char blosumText[] = {
"#  Matrix made by matblas from blosum62.iij\n"
"#  * column uses minimum score\n"
"#  BLOSUM Clustered Scoring Matrix in 1/2 Bit Units\n"
"#  Blocks Database = /data/blocks_5.0/blocks.dat\n"
"#  Cluster Percentage: >= 62\n"
"#  Entropy =   0.6979, Expected =  -0.5209\n"
"   A  R  N  D  C  Q  E  G  H  I  L  K  M  F  P  S  T  W  Y  V  B  Z  X  *\n"
"A  4 -1 -2 -2  0 -1 -1  0 -2 -1 -1 -1 -1 -2 -1  1  0 -3 -2  0 -2 -1  0 -4 \n"
"R -1  5  0 -2 -3  1  0 -2  0 -3 -2  2 -1 -3 -2 -1 -1 -3 -2 -3 -1  0 -1 -4 \n"
"N -2  0  6  1 -3  0  0  0  1 -3 -3  0 -2 -3 -2  1  0 -4 -2 -3  3  0 -1 -4 \n"
"D -2 -2  1  6 -3  0  2 -1 -1 -3 -4 -1 -3 -3 -1  0 -1 -4 -3 -3  4  1 -1 -4 \n"
"C  0 -3 -3 -3  9 -3 -4 -3 -3 -1 -1 -3 -1 -2 -3 -1 -1 -2 -2 -1 -3 -3 -2 -4 \n"
"Q -1  1  0  0 -3  5  2 -2  0 -3 -2  1  0 -3 -1  0 -1 -2 -1 -2  0  3 -1 -4 \n"
"E -1  0  0  2 -4  2  5 -2  0 -3 -3  1 -2 -3 -1  0 -1 -3 -2 -2  1  4 -1 -4 \n"
"G  0 -2  0 -1 -3 -2 -2  6 -2 -4 -4 -2 -3 -3 -2  0 -2 -2 -3 -3 -1 -2 -1 -4 \n"
"H -2  0  1 -1 -3  0  0 -2  8 -3 -3 -1 -2 -1 -2 -1 -2 -2  2 -3  0  0 -1 -4 \n"
"I -1 -3 -3 -3 -1 -3 -3 -4 -3  4  2 -3  1  0 -3 -2 -1 -3 -1  3 -3 -3 -1 -4 \n"
"L -1 -2 -3 -4 -1 -2 -3 -4 -3  2  4 -2  2  0 -3 -2 -1 -2 -1  1 -4 -3 -1 -4 \n"
"K -1  2  0 -1 -3  1  1 -2 -1 -3 -2  5 -1 -3 -1  0 -1 -3 -2 -2  0  1 -1 -4 \n"
"M -1 -1 -2 -3 -1  0 -2 -3 -2  1  2 -1  5  0 -2 -1 -1 -1 -1  1 -3 -1 -1 -4 \n"
"F -2 -3 -3 -3 -2 -3 -3 -3 -1  0  0 -3  0  6 -4 -2 -2  1  3 -1 -3 -3 -1 -4 \n"
"P -1 -2 -2 -1 -3 -1 -1 -2 -2 -3 -3 -1 -2 -4  7 -1 -1 -4 -3 -2 -2 -1 -2 -4 \n"
"S  1 -1  1  0 -1  0  0  0 -1 -2 -2  0 -1 -2 -1  4  1 -3 -2 -2  0  0  0 -4 \n"
"T  0 -1  0 -1 -1 -1 -1 -2 -2 -1 -1 -1 -1 -2 -1  1  5 -2 -2  0 -1 -1  0 -4 \n"
"W -3 -3 -4 -4 -2 -2 -3 -2 -2 -3 -2 -3 -1  1 -4 -3 -2 11  2 -3 -4 -3 -2 -4 \n"
"Y -2 -2 -2 -3 -2 -1 -2 -3  2 -1 -1 -2 -1  3 -3 -2 -2  2  7 -1 -3 -2 -1 -4 \n"
"V  0 -3 -3 -3 -1 -2 -2 -3 -3  3  1 -2  1 -1 -2 -2  0 -3 -1  4 -3 -2 -1 -4 \n"
"B -2 -1  3  4 -3  0  1 -1  0 -3 -4  0 -3 -3 -2  0 -1 -4 -3 -3  4  1 -1 -4 \n"
"Z -1  0  0  1 -3  3  4 -2  0 -3 -3  1 -1 -3 -1  0 -1 -3 -2 -2  1  4 -1 -4 \n"
"X  0 -1 -1 -1 -2 -1 -1 -1 -1 -1 -1 -1 -1 -1 -2  0  0 -2 -1 -1 -1 -1 -1 -4 \n"
"* -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4 -4  1 \n"
};

static void badProteinMatrixLine(int lineIx, char *fileName)
/* Explain line syntax for protein matrix and abort */
{
errAbort("Expecting letter and 25 numbers line %d of %s", lineIx, fileName);
}

struct axtScoreScheme *axtScoreSchemeFromProteinText(char *text, char *fileName)
/* Parse text into a scoring scheme.  This should be in BLAST protein matrix
 * format as in blosumText above. */
{
char *line, *nextLine;
int lineIx = 0;
int realCount = 0;
char columns[24];
char *row[25];
int i;
struct axtScoreScheme *ss;

AllocVar(ss);
for (line = text; line != NULL; line = nextLine)
    {
    nextLine = strchr(line, '\n');
    if (nextLine != NULL)
        *nextLine++ = 0;
    ++lineIx;
    line = skipLeadingSpaces(line);
    if (line[0] == '#' || line[0] == 0)
        continue;
    ++realCount;
    if (realCount == 1)
        {
	int wordCount = chopLine(line, row);
	if (wordCount != 24)
	    errAbort("Not a good protein matrix - expecting 24 letters line %d of %s", lineIx, fileName);
	for (i=0; i<wordCount; ++i)
	    {
	    char *letter = row[i];
	    if (strlen(letter) != 1)
		errAbort("Not a good protein matrix - got word not letter line %d of %s", lineIx, fileName);
	    columns[i] = letter[0];
	    }
	}
    else
        {
	int wordCount = chopLine(line, row);
	char letter, lcLetter;
	if (wordCount != 25)
	    badProteinMatrixLine(lineIx, fileName);
	letter = row[0][0];
	if (strlen(row[0]) != 1 || isdigit(letter))
	    badProteinMatrixLine(lineIx, fileName);
	lcLetter = tolower(letter);
	for (i=1; i<wordCount; ++i)
	    {
	    char *s = row[i];
	    int val;
	    char otherLetter, lcOtherLetter;
	    if (s[0] == '-') ++s;
	    if (!isdigit(s[0]))
		badProteinMatrixLine(lineIx, fileName);
	    otherLetter = columns[i-1];
	    lcOtherLetter = tolower(otherLetter);
	    val = atoi(row[i]);
	    ss->matrix[(int)letter][(int)otherLetter] = val;
	    ss->matrix[(int)lcLetter][(int)otherLetter] = val;
	    ss->matrix[(int)letter][(int)lcOtherLetter] = val;
	    ss->matrix[(int)lcLetter][(int)lcOtherLetter] = val;
	    }
	}
    }
if (realCount < 25)
    errAbort("Unexpected end of %s", fileName);
return ss;
}

struct axtScoreScheme *axtScoreSchemeProteinDefault()
/* Returns default protein scoring scheme.  This is
 * scaled to be compatible with the blastz one. */
{
static struct axtScoreScheme *ss;
int i,j;
if (ss != NULL)
    return ss;
ss = axtScoreSchemeFromProteinText(blosumText, "blosum62");
for (i=0; i<128; ++i)
    for (j=0; j<128; ++j)
        ss->matrix[i][j] *= 19;
ss->gapOpen = 11 * 19;
ss->gapExtend = 1 * 19;
return ss;
}

void axtScoreSchemeFree(struct axtScoreScheme **pObj)
/* Free up score scheme. */
{
freez(pObj);
}

struct axtScoreScheme *axtScoreSchemeProteinRead(char *fileName)
{
char *string;
struct axtScoreScheme *ss;

readInGulp(fileName, &string, NULL);
ss = axtScoreSchemeFromProteinText(string, fileName);
freeMem(string);

return ss;
}

struct axtScoreScheme *axtScoreSchemeReadLf(struct lineFile *lf )
/* Read in scoring scheme from file. Looks like
    A    C    G    T
    91 -114  -31 -123
    -114  100 -125  -31
    -31 -125  100 -114
    -123  -31 -114   91
    O = 400, E = 30
*/
{
char *line, *row[4], *parts[32];
int i,j, partCount;
struct axtScoreScheme *ss;
boolean gotO = FALSE, gotE = FALSE;
static int trans[4] = {'a', 'c', 'g', 't'};

AllocVar(ss);
ss->extra = NULL;
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
if (lineFileNext(lf, &line, NULL))
    {
    ss->extra = cloneString(line);
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
    }
else
    {
    ss->gapOpen = 400;
    ss->gapExtend = 30;
    }
propagateCase(ss);
return ss;
}

struct axtScoreScheme *axtScoreSchemeRead(char *fileName)
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
struct axtScoreScheme *ss = axtScoreSchemeReadLf(lf);
return ss;
}

void axtScoreSchemeDnaWrite(struct axtScoreScheme *ss, FILE *f, char *name)
/* output the score dna based score matrix in meta Data format to File f,
name should be set to the name of the program that is using the matrix */
{
if (ss == NULL)
    return;
if (f == NULL)
    return;
fprintf(f, "##matrix=%s 16 %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
        name,
    ss->matrix['a']['a'],
    ss->matrix['a']['c'],
    ss->matrix['a']['g'],
    ss->matrix['a']['t'],

    ss->matrix['c']['a'],
    ss->matrix['c']['c'],
    ss->matrix['c']['g'],
    ss->matrix['c']['t'],

    ss->matrix['g']['a'],
    ss->matrix['g']['c'],
    ss->matrix['g']['g'],
    ss->matrix['g']['t'],

    ss->matrix['t']['a'],
    ss->matrix['t']['c'],
    ss->matrix['t']['g'],
    ss->matrix['t']['t']);
fprintf(f, "##gapPenalties=%s O=%d E=%d\n", name, ss->gapOpen, ss->gapExtend);
if (ss->extra!=NULL)
    {
    stripChar(ss->extra,' ');
    stripChar(ss->extra,'"');
    fprintf(f, "##blastzParms=%s\n", ss->extra);
    }
}

void axtSwap(struct axt *axt, int tSize, int qSize)
/* Flip target and query on one axt. */
{
struct axt old = *axt;

/* Copy non-strand dependent stuff */
axt->qName = old.tName;
axt->tName = old.qName;
axt->qSym = old.tSym;
axt->tSym = old.qSym;
axt->qStart = old.tStart;
axt->qEnd = old.tEnd;
axt->tStart = old.qStart;
axt->tEnd = old.qEnd;

/* Copy strand dependent stuff. */
assert(axt->tStrand != '-');

if (axt->qStrand == '-')
    {
    /* axt's are really set up so that the target is on the
     * + strand and the query is on the minus strand.
     * Therefore we need to reverse complement both 
     * strands while swapping to preserve this. */
    reverseIntRange(&axt->tStart, &axt->tEnd, qSize);
    reverseIntRange(&axt->qStart, &axt->qEnd, tSize);
    reverseComplement(axt->qSym, axt->symCount);
    reverseComplement(axt->tSym, axt->symCount);
    }
}

void axtBundleFree(struct axtBundle **pObj)
/* Free a axtBundle. */
{
struct axtBundle *obj = *pObj;
if (obj != NULL)
    {
    axtFreeList(&obj->axtList);
    freez(pObj);
    }
}

void axtBundleFreeList(struct axtBundle **pList)
/* Free a list of axtBundles. */
{
struct axtBundle *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    axtBundleFree(&el);
    }
*pList = NULL;
}

void axtAddBlocksToBoxInList(struct cBlock **pList, struct axt *axt)
/* Add blocks (gapless subalignments) from (non-NULL!) axt to block list. 
 * Note: list will be in reverse order of axt blocks. */
{
boolean thisIn, lastIn = FALSE;
int qPos = axt->qStart, tPos = axt->tStart;
int qStart = 0, tStart = 0;
int i;

for (i=0; i<=axt->symCount; ++i)
    {
    int advanceQ = (isalpha(axt->qSym[i]) ? 1 : 0);
    int advanceT = (isalpha(axt->tSym[i]) ? 1 : 0);
    thisIn = (advanceQ && advanceT);
    if (thisIn)
        {
	if (!lastIn)
	    {
	    qStart = qPos;
	    tStart = tPos;
	    }
	}
    else
        {
	if (lastIn)
	    {
	    int size = qPos - qStart;
	    assert(size == tPos - tStart);
	    if (size > 0)
	        {
		struct cBlock *b;
		AllocVar(b);
		b->qStart = qStart;
		b->qEnd = qPos;
		b->tStart = tStart;
		b->tEnd = tPos;
		slAddHead(pList, b);
		}
	    }
	}
    lastIn = thisIn;
    qPos += advanceQ;
    tPos += advanceT;
    }
}

void axtPrintTraditionalExtra(struct axt *axt, int maxLine,
			      struct axtScoreScheme *ss, FILE *f,
			      boolean reverseTPos, boolean reverseQPos)
/* Print out an alignment with line-breaks.  If reverseTPos is true, then
 * the sequence has been reverse complemented, so show the coords starting
 * at tEnd and decrementing down to tStart; likewise for reverseQPos. */
{
int qPos = axt->qStart;
int tPos = axt->tStart;
int symPos;
int aDigits = digitsBaseTen(axt->qEnd);
int bDigits = digitsBaseTen(axt->tEnd);
int digits = max(aDigits, bDigits);
int qFlipOff = axt->qEnd + axt->qStart;
int tFlipOff = axt->tEnd + axt->tStart;

for (symPos = 0; symPos < axt->symCount; symPos += maxLine)
    {
    /* Figure out which part of axt to use for this line. */
    int lineSize = axt->symCount - symPos;
    int lineEnd, i;
    if (lineSize > maxLine)
        lineSize = maxLine;
    lineEnd = symPos + lineSize;

    /* Draw query line including numbers. */
    fprintf(f, "%0*d ", digits, (reverseQPos ? qFlipOff - qPos: qPos+1));
    for (i=symPos; i<lineEnd; ++i)
        {
	char c = axt->qSym[i];
	fputc(c, f);
	if (c != '.' && c != '-')
	    ++qPos;
	}
    fprintf(f, " %0*d\n", digits, (reverseQPos? qFlipOff - qPos + 1 : qPos));

    /* Draw line with match/mismatch symbols. */
    spaceOut(f, digits+1);
    for (i=symPos; i<lineEnd; ++i)
        {
	char q = axt->qSym[i];
	char t = axt->tSym[i];
	char out = ' ';
	if (q == t)
	    out = '|';
	else if (ss != NULL && ss->matrix[(int)q][(int)t] > 0)
	    out = '+';
	fputc(out, f);
	}
    fputc('\n', f);

    /* Draw target line including numbers. */
    fprintf(f, "%0*d ", digits, (reverseTPos ? tFlipOff - tPos : tPos+1));
    for (i=symPos; i<lineEnd; ++i)
        {
	char c = axt->tSym[i];
	fputc(c, f);
	if (c != '.' && c != '-')
	    ++tPos;
	}
    fprintf(f, " %0*d\n", digits, (reverseTPos ? tFlipOff - tPos + 1: tPos));

    /* Draw extra empty line. */
    fputc('\n', f);
    }
}

double axtIdWithGaps(struct axt *axt)
/* Return ratio of matching bases to total symbols in alignment. */
{
int i;
int matchCount = 0;
for (i=0; i<axt->symCount; ++i)
    {
    if (toupper(axt->qSym[i]) == toupper(axt->tSym[i]))
        ++matchCount;
    }
return (double)matchCount/axt->symCount;
}

void axtPrintTraditional(struct axt *axt, int maxLine, struct axtScoreScheme *ss, FILE *f)
/* Print out an alignment with line-breaks. */
{
axtPrintTraditionalExtra(axt, maxLine, ss, f, FALSE, FALSE);
}

double axtCoverage(struct axt *axt, int qSize, int tSize)
/* Return % of q and t covered. */
{
double cov = axt->tEnd - axt->tStart + axt->qEnd - axt->qStart;
return cov/(qSize+tSize);
}

void axtOutPretty(struct axt *axt, int lineSize, FILE *f)
/* Output axt in pretty format. */
{
char *q = axt->qSym;
char *t = axt->tSym;
int size = axt->symCount;
int oneSize, sizeLeft = size;
int i;

fprintf(f, ">%s:%d%c%d %s:%d-%d %d\n", 
	axt->qName, axt->qStart, axt->qStrand, axt->qEnd,
	axt->tName, axt->tStart, axt->tEnd, axt->score);
while (sizeLeft > 0)
    {
    oneSize = sizeLeft;
    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, q, oneSize);
    fputc('\n', f);

    for (i=0; i<oneSize; ++i)
        {
	if (toupper(q[i]) == toupper(t[i]) && isalpha(q[i]))
	    fputc('|', f);
	else
	    fputc(' ', f);
	}
    fputc('\n', f);

    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, t, oneSize);
    fputc('\n', f);
    fputc('\n', f);
    sizeLeft -= oneSize;
    q += oneSize;
    t += oneSize;
    }
}

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
#include "linefile.h"
#include "dnautil.h"
#include "axt.h"

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


struct axt *axtRead(struct lineFile *lf)
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

static char *skipIgnoringDash(char *a, int size, bool skipTrailingDash)
/* Count size number of characters, and any 
 * dash characters. */
{
while (size > 0)
    {
    if (*a++ != '-')
        --size;
    }
if (skipTrailingDash)
    while (*a == '-')
       ++a;
return a;
}


static int countNonDash(char *a, int size)
/* Count number of non-dash characters. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (a[i] != '-') 
        ++count;
return count;
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


int axtScore(struct axt *axt, struct axtScoreScheme *ss)
/* Return calculated score of axt. */
{
int i, symCount = axt->symCount;
char *qSym = axt->qSym, *tSym = axt->tSym;
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
	    score -= gapStart;
	    lastGap = TRUE;
	    }
	}
    else
        {
	score += ss->matrix[ntChars[q]][ntChars[t]];
	lastGap = FALSE;
	}
    }
return score;
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

void axtSubsetOnT(struct axt *axt, int newStart, int newEnd, 
	struct axtScoreScheme *ss, FILE *f)
/* Write out subset of axt that goes from newStart to newEnd
 * in target coordinates. */
{
if (newStart < axt->tStart)
    newStart = axt->tStart;
if (newEnd > axt->tEnd)
    newStart = axt->tEnd;
if (newEnd <= newStart) 
    return;
if (newStart == axt->tStart && newEnd == axt->tEnd)
    {
    axt->score = axtScore(axt, ss);
    axtWrite(axt, f);
    }
else
    {
    struct axt a = *axt;
    char *tSymStart = skipIgnoringDash(a.tSym, newStart - a.tStart, TRUE);
    char *tSymEnd = skipIgnoringDash(tSymStart, newEnd - newStart, FALSE);
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
    if (a.qStart < a.qEnd && a.tStart < a.tEnd)
	axtWrite(&a, f);
    }
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

struct axtScoreScheme *axtScoreSchemeDefault()
/* Return default scoring scheme (after blastz). */
{
struct axtScoreScheme *ss;
AllocVar(ss);
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

ss->gapOpen = 400;
ss->gapExtend = 30;
return ss;
}

static char blosumText[] = {
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
	char letter;
	if (wordCount != 25)
	    badProteinMatrixLine(lineIx, fileName);
	letter = row[0][0];
	if (strlen(row[0]) != 1 || isdigit(letter))
	    badProteinMatrixLine(lineIx, fileName);
	for (i=1; i<wordCount; ++i)
	    {
	    char *s = row[i];
	    if (s[0] == '-') ++s;
	    if (!isdigit(s[0]))
		badProteinMatrixLine(lineIx, fileName);
	    ss->matrix[letter][columns[i-1]] = atoi(row[i]);
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
struct axtScoreScheme *ss;
int i,j;
ss = axtScoreSchemeFromProteinText(blosumText, "blosum62");
for (i=0; i<128; ++i)
    for (j=0; j<128; ++j)
        ss->matrix[i][j] *= 33;
ss->gapOpen = 500;
ss->gapExtend = 100;
return ss;
}

void axtScoreSchemeFree(struct axtScoreScheme **pObj)
/* Free up score scheme. */
{
freez(pObj);
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
char *line, *row[4], *parts[32];
int i,j, partCount;
struct axtScoreScheme *ss;
boolean gotO = FALSE, gotE = FALSE;
static int trans[4] = {'a', 'c', 'g', 't'};

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


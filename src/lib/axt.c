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
	score += ss->matrix[ntVal5[q]][ntVal5[t]];
	lastGap = FALSE;
	}
    }
return score;
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
ss->matrix[A_BASE_VAL][A_BASE_VAL] = 91;
ss->matrix[A_BASE_VAL][C_BASE_VAL] = -114;
ss->matrix[A_BASE_VAL][G_BASE_VAL] = -31;
ss->matrix[A_BASE_VAL][T_BASE_VAL] = -123;

ss->matrix[C_BASE_VAL][A_BASE_VAL] = -114;
ss->matrix[C_BASE_VAL][C_BASE_VAL] = 100;
ss->matrix[C_BASE_VAL][G_BASE_VAL] = -125;
ss->matrix[C_BASE_VAL][T_BASE_VAL] = -31;

ss->matrix[G_BASE_VAL][A_BASE_VAL] = -31;
ss->matrix[G_BASE_VAL][C_BASE_VAL] = -125;
ss->matrix[G_BASE_VAL][G_BASE_VAL] = 100;
ss->matrix[G_BASE_VAL][T_BASE_VAL] = -114;

ss->matrix[T_BASE_VAL][A_BASE_VAL] = -123;
ss->matrix[T_BASE_VAL][C_BASE_VAL] = -31;
ss->matrix[T_BASE_VAL][G_BASE_VAL] = -114;
ss->matrix[T_BASE_VAL][T_BASE_VAL] = 91;

ss->gapOpen = 400;
ss->gapExtend = 30;
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
char *line, *row[4], *parts[32];
int i,j, partCount;
struct axtScoreScheme *ss;
boolean gotO = FALSE, gotE = FALSE;
static int trans[4] = {A_BASE_VAL, C_BASE_VAL, G_BASE_VAL, T_BASE_VAL};

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


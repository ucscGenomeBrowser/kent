/* blastOut.c - stuff to output an alignment in blast format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "axt.h"
#include "obscure.h"
#include "genoFind.h"

struct axtRef
/* A reference to an axt. */
    {
    struct axtRef *next;
    struct axt *axt;
    };

int axtRefCmpScore(const void *va, const void *vb)
/* Compare to sort based on score. */
{
const struct axtRef *a = *((struct axtRef **)va);
const struct axtRef *b = *((struct axtRef **)vb);
return b->axt->score - a->axt->score;
}

struct targetHits
/* Collection of hits to a single target. */
    {
    struct targetHits *next;
    char *name;	    	    /* Target name */
    int size;		    /* Target size */
    struct axtRef *axtList; /* List of axts, sorted by score. */
    int score;		    /* Score of best element */
    };

static void targetHitsFree(struct targetHits **pObj)
/* Free one target hits structure. */
{
struct targetHits *obj = *pObj;
if (obj != NULL)
    {
    freeMem(obj->name);
    slFreeList(&obj->axtList);
    freez(pObj);
    }
}

static void targetHitsFreeList(struct targetHits **pList)
/* Free a list of dynamically allocated targetHits's */
{
struct targetHits *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    targetHitsFree(&el);
    }
*pList = NULL;
}


static int targetHitsCmpScore(const void *va, const void *vb)
/* Compare to sort based on score. */
{
const struct targetHits *a = *((struct targetHits **)va);
const struct targetHits *b = *((struct targetHits **)vb);

return b->score - a->score;
}

static int blastzToWublastScore(int bzScore)
/* Convert from 100 points/match blastz score to 5 points/match
 * wu-blast score. */
{
return bzScore/19;
}

static double blastzScoreToWuBits(int bzScore, boolean isProt)
/* Convert from blastz score to bit score.  The magic number
 * 32.1948 was derived from the wu-blast bit to score ratio.
 * I'm not sure I agree with this, but am doing it to be compatible.   
 * I'd tend to give 2 bits for each matching base more or less. 
 * This is much less. */
{
int wuScore = blastzToWublastScore(bzScore);
if (isProt)
    return wuScore * 0.355;
else
    return wuScore * 0.1553;
}

static double blastzScoreToWuExpectation(int bzScore, double databaseSize)
/* I'm puzzled by the wu-blast expectation score.  I would
 * think it would be  databaseSize / (2^bitScore)  but
 * it's not.   I think the best I can do is approximate
 * it with something scaled to be close to this expectation. */
{
double logProbOne = -log(2) * bzScore / 32.1948;
return databaseSize * exp(logProbOne);
}

int blastzScoreToNcbiBits(int bzScore)
/* Convert blastz score to bit score in NCBI sense. */
{
return round(bzScore * 0.0205);
}

double blastzScoreToNcbiExpectation(int bzScore)
/* Convert blastz score to expectation in NCBI sense. */
{
double bits = bzScore * 0.0205;
double logProb = -bits*log(2);
return 3.0e9 * exp(logProb);
}

static double expectationToProbability(double e)
/* Convert expected number of hits to probability of at least
 * one hit.  This is a crude approximation, but actually pretty precise
 * for e < 0.1, which is all that really matters.... */
{
if (e < 0.999)
    return e;
else
    return 0.999;
}

static int countMatches(char *a, char *b, int size)
/* Count number of characters that match between a and b. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (toupper(a[i]) == toupper(b[i]))
        ++count;
return count;
}

static int plusStrandPos(int pos, int size, char strand, boolean isEnd)
/* Return position on plus strand, one based. */
{
if (strand == '-')
    {
    pos = size - pos;
    if (isEnd)
       ++pos;
    }
else
    {
    if (!isEnd)
        ++pos;
    }
return pos;
}

static void blastiodAxtOutput(FILE *f, struct axt *axt, int tSize, int qSize, 
	int lineSize, boolean isProt)
/* Output base-by-base part of alignment in blast-like fashion. */
{
int tOff = axt->tStart;
int qOff = axt->qStart;
int lineStart, lineEnd, i;
int tDig = digitsBaseTen(axt->tEnd);
int qDig = digitsBaseTen(axt->qEnd);
int digits = max(tDig, qDig);
struct axtScoreScheme *ss = NULL;

if (isProt)
    ss = axtScoreSchemeProteinDefault();
for (lineStart = 0; lineStart < axt->symCount; lineStart = lineEnd)
    {
    lineEnd = lineStart + lineSize;
    if (lineEnd > axt->symCount) lineEnd = axt->symCount;
    fprintf(f, "Query: %*d ", digits, plusStrandPos(qOff, qSize, axt->qStrand, FALSE));
    for (i=lineStart; i<lineEnd; ++i)
	{
	char c = axt->qSym[i];
	fputc(c, f);
	if (c != '-')
	    ++qOff;
	}
    fprintf(f, " %-d\n", plusStrandPos(qOff, qSize, axt->qStrand, TRUE));
    fprintf(f, "       %*s ", digits, " ");
    for (i=lineStart; i<lineEnd; ++i)
        {
	char q, t, c;
	q = axt->qSym[i];
	t = axt->tSym[i];
	if (isProt)
	    {
	    if (q == t)
	        c = q;
	    else if (ss->matrix[q][t] > 0)
	        c = '+';
	    else
	        c = ' ';
	    }
	else
	    c = ((toupper(q) == toupper(t)) ? '|' : ' ');

	fputc(c, f);
	}
    fprintf(f, "\n");
    fprintf(f, "Sbjct: %*d ", digits, plusStrandPos(tOff, tSize, axt->tStrand, FALSE));
    for (i=lineStart; i<lineEnd; ++i)
	{
	char c = axt->tSym[i];
	fputc(c, f);
	if (c != '-')
	    ++tOff;
	}
    fprintf(f, " %-d\n", plusStrandPos(tOff, tSize, axt->tStrand, TRUE));
    fprintf(f, "\n");
    }
}

static struct targetHits *bundleIntoTargets(struct axtBundle *abList)
/* BLAST typically outputs everything on the same query and target
 * in one clump.  This routine rearranges axts in abList to do this. */
{
struct targetHits *targetList = NULL, *target;
struct hash *targetHash = newHash(10);
struct axtBundle *ab;
struct axtRef *ref;

/* Build up a list of targets in database hit by query sorted by
 * score of hits. */
for (ab = abList; ab != NULL; ab = ab->next)
    {
    struct axt *axt, *next;
    for (axt = ab->axtList; axt != NULL; axt = next)
	{
	next = axt->next;
	target = hashFindVal(targetHash, axt->tName);
	if (target == NULL)
	    {
	    AllocVar(target);
	    slAddHead(&targetList, target);
	    hashAdd(targetHash, axt->tName, target);
	    target->name = cloneString(axt->tName);
	    target->size = ab->tSize;
	    }
	if (axt->score > target->score)
	    target->score = axt->score;
	AllocVar(ref);
	ref->axt = axt;
	slAddHead(&target->axtList, ref);
	}
    ab->axtList = NULL;
    }
slSort(&targetList, targetHitsCmpScore);
for (target = targetList; target != NULL; target = target->next)
    slSort(&target->axtList, axtRefCmpScore);

hashFree(&targetHash);
return targetList;
}

static char *nameForStrand(char strand)
/* Return Plus/Minus for +/- */
{
if (strand == '-')
    return "Minus";
else
    return "Plus";
}

static void wuBlastOut(struct axtBundle *abList, int queryIx, boolean isProt, 
	FILE *f, 
	char *databaseName, int databaseSeqCount, double databaseLetterCount, 
	char *ourId)
/* Do wublast-like output at end of processing query. */
{
char asciiNum[32];
struct targetHits *targetList = NULL, *target;
struct axtBundle *ab;
char *queryName;
int isRc;
int querySize = abList->qSize;

/* Print out stuff that doesn't depend on query or database. */
if (ourId == NULL)
    ourId = "axtBlastOut";
fprintf(f, "BLAST%c 2.0MP-WashU [%s]\n", (isProt ? 'P' : 'N'), ourId);
fprintf(f, "\n");
fprintf(f, "Copyright (C) 2000-2002 Jim Kent\n");
fprintf(f, "All Rights Reserved\n");
fprintf(f, "\n");
fprintf(f, "Reference:  Kent, WJ. (2002) BLAT - The BLAST-like alignment tool\n");
fprintf(f, "\n");
if (!isProt)
    {
    fprintf(f, "Notice:  this program and its default parameter settings are optimized to find\n");
    fprintf(f, "nearly identical sequences very rapidly.  For slower but more sensitive\n");
    fprintf(f, "alignments please use other methods.\n");
    fprintf(f, "\n");
    }

/* Print query and database info. */
queryName = abList->axtList->qName;
fprintf(f, "Query=  %s\n", queryName);
fprintf(f, "        (%d letters; record %d)\n", abList->qSize, queryIx);
fprintf(f, "\n");
fprintf(f, "Database:  %s\n",  databaseName);
sprintLongWithCommas(asciiNum, databaseLetterCount);
fprintf(f, "           %d sequences; %s total letters\n",  databaseSeqCount, asciiNum);
fprintf(f, "Searching....10....20....30....40....50....60....70....80....90....100%% done\n");
fprintf(f, "\n");

targetList = bundleIntoTargets(abList);

/* Print out summary of hits. */
fprintf(f, "                                                                     Smallest\n");
fprintf(f, "                                                                       Sum\n");
fprintf(f, "                                                              High  Probability\n");
fprintf(f, "Sequences producing High-scoring Segment Pairs:              Score  P(N)      N\n");
fprintf(f, "\n");
for (target = targetList; target != NULL; target = target->next)
    {
    double expectation = blastzScoreToWuExpectation(target->score, databaseLetterCount);
    double p = expectationToProbability(expectation);
    fprintf(f, "%-61s %4d  %8.1e %2d\n", target->name, 
    	blastzToWublastScore(target->score), p, slCount(target->axtList));
    }

/* Print out details on each target. */
for (target = targetList; target != NULL; target = target->next)
    {
    fprintf(f, "\n\n>%s\n", target->name);
    fprintf(f, "        Length = %d\n", target->size);
    fprintf(f, "\n");
    for (isRc=0; isRc <= 1; ++isRc)
	{
	boolean saidStrand = FALSE;
	char strand = (isRc ? '-' : '+');
	char *strandName = nameForStrand(strand);
	struct axtRef *ref;
	struct axt *axt;
	for (ref = target->axtList; ref != NULL; ref = ref->next)
	    {
	    axt = ref->axt;
	    if (axt->qStrand == strand)
		{
		int matches = countMatches(axt->qSym, axt->tSym, axt->symCount);
		if (!saidStrand)
		    {
		    saidStrand = TRUE;
		    if (!isProt)
			fprintf(f, "  %s Strand HSPs:\n\n", strandName);
		    }
		fprintf(f, " Score = %d (%2.1f bits), Expect = %5.1e, P = %5.1e\n",
		     blastzToWublastScore(axt->score), 
		     blastzScoreToWuBits(axt->score, isProt),
		     blastzScoreToWuExpectation(axt->score, databaseLetterCount),
		     blastzScoreToWuExpectation(axt->score, databaseLetterCount));
		fprintf(f, " Identities = %d/%d (%d%%), Positives = %d/%d (%d%%), Strand = %s / Plus\n",
		     matches, axt->symCount, round(100.0 * matches / axt->symCount),
		     matches, axt->symCount, round(100.0 * matches / axt->symCount),
		     strandName);
		fprintf(f, "\n");
		blastiodAxtOutput(f, axt, target->size, querySize, 60, isProt);
		}
	    }
	}
    }

/* Cleanup time. */
targetHitsFreeList(&targetList);
}

void ncbiPrintE(FILE *f, double e)
/* Print a small number NCBI style. */
{
if (e <= 0.0)
    fprintf(f, "0.0");
else if (e <= 1.0e-100)
    {
    char buf[256], *s;
    sprintf(buf, "%e", e);
    s = strchr(buf, 'e');
    if (s == NULL)
	fprintf(f, "0.0");
    else
        fprintf(f, "%s", s);
    }
else
    fprintf(f, "%1.0e", e);
}


static void ncbiBlastOut(struct axtBundle *abList, int queryIx, boolean isProt, 
	FILE *f, 
	char *databaseName, int databaseSeqCount, double databaseLetterCount, 
	char *ourId)
/* Do ncbiblast-like output at end of processing query. */
{
char asciiNum[32];
struct targetHits *targetList = NULL, *target;
struct axtBundle *ab;
char *queryName;
int isRc;
int querySize = abList->qSize;

/* Print out stuff that doesn't depend on query or database. */
if (ourId == NULL)
    ourId = "axtBlastOut";
fprintf(f, "BLAST%c 2.2.4 [%s]\n", (isProt ? 'P' : 'N'), ourId);
fprintf(f, "\n");
fprintf(f, "Reference:  Kent, WJ. (2002) BLAT - The BLAST-like alignment tool\n");
fprintf(f, "\n");

/* Print query and database info. */
queryName = abList->axtList->qName;
fprintf(f, "Query=  %s\n", queryName);
fprintf(f, "        (%d letters)\n", abList->qSize);
fprintf(f, "\n");
fprintf(f, "Database:  %s\n",  databaseName);
sprintLongWithCommas(asciiNum, databaseLetterCount);
fprintf(f, "           %d sequences; %s total letters\n",  databaseSeqCount, asciiNum);
fprintf(f, "\n");

targetList = bundleIntoTargets(abList);

/* Print out summary of hits. */
fprintf(f, "                                                                 Score    E\n");
fprintf(f, "Sequences producing significant alignments:                      (bits) Value\n");
fprintf(f, "\n");
for (target = targetList; target != NULL; target = target->next)
    {
    int bit = blastzScoreToNcbiBits(target->score);
    double expectation = blastzScoreToNcbiExpectation(target->score);
    fprintf(f, "%-67s  %4d   ", target->name, bit);
    ncbiPrintE(f, expectation);
    fprintf(f, "\n");
    }
fprintf(f, "\n");
fprintf(f, "ALIGNMENTS\n");

/* Print out details on each target. */
for (target = targetList; target != NULL; target = target->next)
    {
    struct axtRef *ref;
    struct axt *axt;
    int matches;
    fprintf(f, "\n\n>%s\n", target->name);
    fprintf(f, "          Length = %d\n", target->size);
    for (ref = target->axtList; ref != NULL; ref = ref->next)
	{
	axt = ref->axt;
	matches = countMatches(axt->qSym, axt->tSym, axt->symCount);
	fprintf(f, "\n");
	fprintf(f, " Score = %d bits, Expect = ",
	     blastzScoreToNcbiBits(axt->score));
	ncbiPrintE(f, blastzScoreToNcbiExpectation(axt->score));
	fprintf(f, "\n");
	fprintf(f, " Identities = %d/%d (%d%%)\n",
	     matches, axt->symCount, round(100.0 * matches / axt->symCount));
	fprintf(f, " Strand = %s / %s\n", nameForStrand(axt->qStrand),
	    nameForStrand(axt->tStrand));
	fprintf(f, "\n");
	fprintf(f, "\n");
	blastiodAxtOutput(f, axt, target->size, querySize, 60, isProt);
	}
    }

/* Cleanup time. */
targetHitsFreeList(&targetList);
}


void axtBlastOut(struct axtBundle *abList, int queryIx, boolean isProt, 
	FILE *f, char *databaseName, int databaseSeqCount, double databaseLetterCount, 
	boolean isWu, char *ourId)
/* Output a bundle of axt's on the same query sequence in blast format.
 * The parameters in detail are:
 *   ab - the list of bundles of axt's. 
 *   f  - output file handle
 *   databaseSeqCount - number of sequences in database
 *   databaseLetterCount - number of bases or aa's in database
 *   isWu - TRUE if want wu-blast rather than blastall format
 *   ourId - optional (may be NULL) thing to put in header
 */
{
if (isWu)
    wuBlastOut(abList, queryIx, isProt, f, databaseName,
   	databaseSeqCount, databaseLetterCount, ourId);
else
    ncbiBlastOut(abList, queryIx, isProt, f, databaseName,
        databaseSeqCount, databaseLetterCount, ourId);
}


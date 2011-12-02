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

static int axtRefCmpScore(const void *va, const void *vb)
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
/* Convert from blastz score to bit score.  The magic number's
 * here are taken from comparing wu-blast scores to axtScores on
 * a couple of sequences.  This doesn't really seem to be a bit
 * score.  It's not very close to 2 bits per base. */
{
int wuScore = blastzToWublastScore(bzScore);
if (isProt)
    return wuScore * 0.3545;
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

static int blastzScoreToNcbiBits(int bzScore)
/* Convert blastz score to bit score in NCBI sense. */
{
return round(bzScore * 0.0205);
}

static int blastzScoreToNcbiScore(int bzScore)
/* Conver blastz score to NCBI matrix score. */
{
return round(bzScore * 0.0529);
}

static double blastzScoreToNcbiExpectation(int bzScore)
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

static int countGaps(char *a, char *b, int size)
/* Count number of inserts in either strand. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
   {
   if (a[i] == '-')
       ++count;
   if (b[i] == '-')
       ++count;
   }
return count;
}

static int countGapOpens(char *a, char *b, int size)
/* Count number of inserts in either strand. */
{
int i, count = 0;
boolean inGap = FALSE;
for (i=0; i<size; ++i)
    {
    if (a[i] == '-' || b[i] == '-')
        {
	if (!inGap)
	    {
	    ++count;
	    inGap = TRUE;
	    }
	}
    else
        {
	inGap = FALSE;
	}
    }
return count;
}


static int countPositives(char *a, char *b, int size)
/* Count positive (not necessarily identical) protein matches. */
{
int count = 0;
int i;
struct axtScoreScheme *ss = axtScoreSchemeProteinDefault();
for (i=0; i<size; ++i)
    {
    if (ss->matrix[(int)a[i]][(int)b[i]] > 0)
        ++count;
    }
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

static int calcDigitCount(struct axt *axt, int tSize, int qSize)
/* Figure out how many digits needed for blast position display. */
{
int tDig, qDig;
if (axt->qStrand == '-')
    qDig = digitsBaseTen(qSize - axt->qStart + 1);
else
    qDig = digitsBaseTen(axt->qEnd);
if (axt->tStrand == '-')
    tDig = digitsBaseTen(tSize - axt->tStart + 1);
else
    tDig = digitsBaseTen(axt->tEnd);
return max(tDig, qDig);
}

static void blastiodAxtOutput(FILE *f, struct axt *axt, int tSize, int qSize, 
	int lineSize, boolean isProt, boolean isTranslated)
/* Output base-by-base part of alignment in blast-like fashion. */
{
int tOff = axt->tStart;
int dt = (isTranslated ? 3 : 1);
int qOff = axt->qStart;
int lineStart, lineEnd, i;
int digits = calcDigitCount(axt, tSize, qSize);
struct axtScoreScheme *ss = NULL;

if (isProt)
    ss = axtScoreSchemeProteinDefault();
for (lineStart = 0; lineStart < axt->symCount; lineStart = lineEnd)
    {
    lineEnd = lineStart + lineSize;
    if (lineEnd > axt->symCount) lineEnd = axt->symCount;
    fprintf(f, "Query: %-*d ", digits, plusStrandPos(qOff, qSize, axt->qStrand, FALSE));
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
	    else if (ss->matrix[(int)q][(int)t] > 0)
	        c = '+';
	    else
	        c = ' ';
	    }
	else
	    c = ((toupper(q) == toupper(t)) ? '|' : ' ');

	fputc(c, f);
	}
    fprintf(f, "\n");
    fprintf(f, "Sbjct: %-*d ", digits, plusStrandPos(tOff, tSize, axt->tStrand, FALSE));
    for (i=lineStart; i<lineEnd; ++i)
	{
	char c = axt->tSym[i];
	fputc(c, f);
	if (c != '-')
	    tOff += dt;
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
    struct axt *axt;
    for (axt = ab->axtList; axt != NULL; axt = axt->next)
	{
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

static char *progType(boolean isProt, struct axtBundle *ab, boolean isUpper)
/* Return blast 'program' */
{
if (!isProt)
    return isUpper ? "BLASTN" : "blastn";
else
    {
    if (ab->axtList->frame != 0)
        return isUpper ? "TBLASTN" : "tblastn";
    else
        return isUpper ? "BLASTP" : "blastp";
    }
}

static void wuBlastOut(struct axtBundle *abList, int queryIx, boolean isProt, 
	FILE *f, 
	char *databaseName, int databaseSeqCount, double databaseLetterCount, 
	char *ourId)
/* Do wublast-like output at end of processing query. */
{
char asciiNum[32];
struct targetHits *targetList = NULL, *target;
char *queryName;
int isRc;
int querySize = abList->qSize;
boolean isTranslated = (abList->axtList->frame != 0);

/* Print out stuff that doesn't depend on query or database. */
if (ourId == NULL)
    ourId = "axtBlastOut";
fprintf(f, "%s 2.0MP-WashU [%s]\n", progType(isProt, abList, TRUE), ourId);
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
		int positives = countPositives(axt->qSym, axt->tSym, axt->symCount);
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
		fprintf(f, " Identities = %d/%d (%d%%), Positives = %d/%d (%d%%)",
		     matches, axt->symCount, round(100.0 * matches / axt->symCount),
		     positives, axt->symCount, round(100.0 * positives / axt->symCount));
		if (isProt)
		    {
		    if (axt->frame != 0)
		        fprintf(f, ", Frame = %c%d", axt->tStrand, axt->frame);
		    fprintf(f, "\n");
		    }
		else
		    fprintf(f, ", Strand = %s / Plus\n", strandName);
		fprintf(f, "\n");
		blastiodAxtOutput(f, axt, target->size, querySize, 60, isProt, isTranslated);
		}
	    }
	}
    }

/* Cleanup time. */
targetHitsFreeList(&targetList);
}

static void ncbiPrintE(FILE *f, double e)
/* Print a small number NCBI style. */
{
if (e <= 0.0)
    fprintf(f, "0.0");
else if (e <= 1.0e-100)
    {
    char buf[256], *s;
    safef(buf, sizeof(buf), "%e", e);
    s = strchr(buf, 'e');
    if (s == NULL)
	fprintf(f, "0.0");
    else
        fprintf(f, "%s", s);
    }
else
    fprintf(f, "%1.0e", e);
}

/* special global variable needed for Known Genes track building.  Fan 1/21/03 */
int answer_for_kg;
static void ncbiBlastOut(struct axtBundle *abList, int queryIx, boolean isProt, 
	FILE *f, char *databaseName, int databaseSeqCount, 
	double databaseLetterCount, char *ourId, double minIdentity)
/* Do ncbiblast-like output at end of processing query. */
{
char asciiNum[32];
struct targetHits *targetList = NULL, *target;
char *queryName;
int querySize = abList->qSize;
boolean isTranslated = (abList->axtList->frame != 0);

/* Print out stuff that doesn't depend on query or database. */
if (ourId == NULL)
    ourId = "axtBlastOut";
fprintf(f, "%s 2.2.11 [%s]\n", progType(isProt, abList, TRUE), ourId);
fprintf(f, "\n");
fprintf(f, "Reference:  Kent, WJ. (2002) BLAT - The BLAST-like alignment tool\n");
fprintf(f, "\n");

/* Print query and database info. */
queryName = abList->axtList->qName;
fprintf(f, "Query= %s\n", queryName);
fprintf(f, "         (%d letters)\n", abList->qSize);
fprintf(f, "\n");
fprintf(f, "Database: %s \n",  databaseName);
sprintLongWithCommas(asciiNum, databaseLetterCount);
fprintf(f, "           %d sequences; %s total letters\n",  databaseSeqCount, asciiNum);
fprintf(f, "\n");
fprintf(f, "Searching.done\n");

targetList = bundleIntoTargets(abList);

/* Print out summary of hits. */
fprintf(f, "                                                                 Score    E\n");
fprintf(f, "Sequences producing significant alignments:                      (bits) Value\n");
fprintf(f, "\n");
for (target = targetList; target != NULL; target = target->next)
    {
    struct axtRef *ref;
    struct axt *axt;
    int matches;
    double identity, expectation;
    int bit;
    
    for (ref = target->axtList; ref != NULL; ref = ref->next)
	{
	axt = ref->axt;
	
	matches = countMatches(axt->qSym, axt->tSym, axt->symCount);
	identity = round(100.0 * matches / axt->symCount);
	/* skip output if minIdentity not reached */
	if (identity < minIdentity) continue;
    
    	bit = blastzScoreToNcbiBits(axt->score);
        expectation = blastzScoreToNcbiExpectation(axt->score);
    	fprintf(f, "%-67s  %4d   ", target->name, bit);
    	ncbiPrintE(f, expectation);
    	fprintf(f, "\n");
    	}
    }
fprintf(f, "\n");

/* Print out details on each target. */
for (target = targetList; target != NULL; target = target->next)
    {
    struct axtRef *ref;
    struct axt *axt;
    int matches, gaps;
    char *oldName;
    
    int ii = 0;
    double identity;
    oldName = strdup("");

    for (ref = target->axtList; ref != NULL; ref = ref->next)
	{
	ii++;
	axt = ref->axt;
	
	matches = countMatches(axt->qSym, axt->tSym, axt->symCount);
	identity = round(100.0 * matches / axt->symCount);
	
	/* skip output if minIdentity not reached */
	if (identity < minIdentity) continue;
        
	/* print target sequence name and length only once */ 
	if (!sameWord(oldName, target->name))
	    {
	    fprintf(f, "\n\n>%s \n", target->name);
	    fprintf(f, "          Length = %d\n", target->size);
	    oldName = strdup(target->name);
	    }

	fprintf(f, "\n");
	fprintf(f, " Score = %d bits (%d), Expect = ",
	     blastzScoreToNcbiBits(axt->score),
	     blastzScoreToNcbiScore(axt->score));
	ncbiPrintE(f, blastzScoreToNcbiExpectation(axt->score));
	fprintf(f, "\n");
	
	if (isProt)
	    {
	    int positives = countPositives(axt->qSym, axt->tSym, axt->symCount);
	    gaps = countGaps(axt->qSym, axt->tSym, axt->symCount);
	    fprintf(f, " Identities = %d/%d (%d%%),",
		 matches, axt->symCount, round(100.0 * matches / axt->symCount));
	    fprintf(f, " Positives = %d/%d (%d%%),",
		 positives, axt->symCount, round(100.0 * positives / axt->symCount));
	    fprintf(f, " Gaps = %d/%d (%d%%)\n",
		 gaps, axt->symCount, round(100.0 * gaps / axt->symCount));
	    if (axt->frame != 0) 
		fprintf(f, " Frame = %c%d\n", axt->tStrand, axt->frame);
	    /* set the special global variable, answer_for_kg.  
   	       This is needed for Known Genes track building.  Fan 1/21/03 */
            answer_for_kg=axt->symCount - matches;
	    }
	else
	    {
	    fprintf(f, " Identities = %d/%d (%d%%)\n",
		 matches, axt->symCount, round(100.0 * matches / axt->symCount));
	    /* blast displays dna searches as +- instead of blat's default -+ */
	    if (!isTranslated)
		if ((axt->qStrand == '-') && (axt->tStrand == '+'))
		    {
		    reverseIntRange(&axt->qStart, &axt->qEnd, querySize);
		    reverseIntRange(&axt->tStart, &axt->tEnd, target->size);
		    reverseComplement(axt->qSym, axt->symCount);
		    reverseComplement(axt->tSym, axt->symCount);
		    axt->qStrand = '+';
		    axt->tStrand = '-';
		    }
	    fprintf(f, " Strand = %s / %s\n", nameForStrand(axt->qStrand),
		nameForStrand(axt->tStrand));
	    }
	fprintf(f, "\n");
	blastiodAxtOutput(f, axt, target->size, querySize, 60, isProt, isTranslated);
	}
    }

fprintf(f, "  Database: %s\n", databaseName);

/* Cleanup time. */
targetHitsFreeList(&targetList);
}

static void xmlBlastOut(struct axtBundle *abList, int queryIx, boolean isProt, 
	FILE *f, char *databaseName, int databaseSeqCount, 
	double databaseLetterCount, char *ourId)
/* Do ncbi blast xml-like output at end of processing query. 
 * WARNING - still not completely baked.  Format at NCBI seems
 * to be missing some end tags actually when I checked. -jk */
{
char *queryName = abList->axtList->qName;
int querySize = abList->qSize;
int hitNum = 0;
struct targetHits *targetList = NULL, *target;

if (ourId == NULL)
    ourId = "axtBlastOut";
fprintf(f, "<?xml version=\"1.0\"?>\n");
fprintf(f, "<!DOCTYPE BlastOutput PUBLIC \"-//NCBI//NCBI BlastOutput/EN\" \"NCBI_BlastOutput.dtd\">\n");
fprintf(f, "<BlastOutput>\n");
fprintf(f, "  <BlastOutput_program>%s</BlastOutput_program>\n", 
	progType(isProt, abList, FALSE));
fprintf(f, "  <BlastOutput_version>%s 2.2.4 [%s]</BlastOutput_version>\n",
	progType(isProt, abList, FALSE), ourId);
fprintf(f, "  <BlastOutput_reference>~Reference: Kent, WJ. (2002) BLAT - The BLAST-like alignment tool</BlastOutput_reference>\n");
fprintf(f, "  <BlastOutput_db>%s</BlastOutput_db>\n", databaseName);
fprintf(f, "  <BlastOutput_query-ID>%d</BlastOutput_query-ID>\n", queryIx);
fprintf(f, "  <BlastOutput_query-def>%s</BlastOutput_query-def>\n", queryName);
fprintf(f, "  <BlastOutput_query-len>%d</BlastOutput_query-len>\n", querySize);

if (isProt)
    {
    fprintf(f, "  <BlastOutput_param>\n");
    fprintf(f, "    <Parameters_matrix>BLOSUM62</Parameters_matrix>\n");
    fprintf(f, "    <Parameters_expect>0.001</Parameters_expect>\n");
    fprintf(f, "    <Parameters_expect>10</Parameters_expect>\n");
    fprintf(f, "    <Parameters_gap-extend>1</Parameters_gap-extend>\n");
    fprintf(f, "  </BlastOutput_param>\n");
    }

fprintf(f, "  <BlastOutput_iterations>\n");
fprintf(f, "    <Iteration>\n");
fprintf(f, "      <Iteration_iter-num>1</Iteration_iter-num>\n");
fprintf(f, "      <Iteration_hits>\n");

/* Print out details on each target. */
targetList = bundleIntoTargets(abList);
for (target = targetList; target != NULL; target = target->next)
    {
    struct axtRef *ref;
    fprintf(f, "      <hit>\n");
    fprintf(f, "        <Hit_num>%d</Hit_num>\n", hitNum);
    fprintf(f, "        <Hit_id>%s</Hit_id>\n", target->name);
    fprintf(f, "        <Hit_accession>%s</Hit_accession>\n", target->name);
    fprintf(f, "        <Hit_len>%d</Hit_len>\n", target->size);
    fprintf(f, "        <Hit_hsps>\n");
    for (ref = target->axtList; ref != NULL; ref = ref->next)
        {
	struct axt *axt = ref->axt;
	int hspIx = 0;
	fprintf(f, "        <Hsp>\n");
	fprintf(f, "          <Hsp_num>%d</Hsp_num>\n", ++hspIx);
	fprintf(f, "          <Hsp_bit-score>%d</Hsp_bit-score>\n", 
		blastzScoreToNcbiBits(axt->score));
	fprintf(f, "          <Hsp_score>%d</Hsp_score>\n",
		blastzScoreToNcbiScore(axt->score));
	fprintf(f, "          <Hsp_evalue>%f</Hsp_evalue>\n",
	        blastzScoreToNcbiExpectation(axt->score));
	fprintf(f, "          <Hsp_query-from>%d</Hsp_query-from>\n", 
		axt->qStart+1);
	fprintf(f, "          <Hsp_query-to>%d</Hsp_query-to>\n", axt->qEnd);
	fprintf(f, "          <Hsp_hit-from>%d</Hsp_hit-from>\n", 
		axt->tStart+1);
	fprintf(f, "          <Hsp_hit-to>%d</Hsp_hit-to>\n", axt->tEnd);
	fprintf(f, "        </Hsp>\n");
	}

    fprintf(f, "        </Hit_hsps>\n");
    }

fprintf(f, "      </Iteration_hits>\n");
fprintf(f, "    </Iteration>\n");
fprintf(f, "  </BlastOutput_iterations>\n");
fprintf(f, "</BlastOutput>\n");
}

static void printAxtTargetBlastTab(FILE *f, struct axt *axt, int targetSize)
/* Print out target in tabular blast-oriented format. */
{
int s = axt->tStart, e = axt->tEnd;
if (axt->tStrand == '-')
    reverseIntRange(&s, &e, targetSize);
if (axt->tStrand == axt->qStrand)
    {
    fprintf(f, "%d\t", s+1);
    fprintf(f, "%d\t", e);
    }
else
    {
    fprintf(f, "%d\t", e);
    fprintf(f, "%d\t", s+1);
    }
}

static void tabBlastOut(struct axtBundle *abList, int queryIx, boolean isProt, 
	FILE *f, char *databaseName, int databaseSeqCount, 
	double databaseLetterCount, char *ourId, boolean withComment)
/* Do NCBI tabular blast output. */
{
char *queryName = abList->axtList->qName;
int querySize = abList->qSize;
struct targetHits *targetList = NULL, *target;

if (withComment)
    {
    // use date from CVS, unless checked out with -kk, then ignore.
    char * rcsDate = "$Date: 2009/02/26 00:05:49 $";
    char dateStamp[11];
    if (strlen(rcsDate) > 17)
        safencpy(dateStamp, sizeof(dateStamp), rcsDate+7, 10);
    else
        safecpy(dateStamp, sizeof(dateStamp), "");
    dateStamp[10] = 0;
    fprintf(f, "# BLAT %s [%s]\n", gfVersion, dateStamp);
    fprintf(f, "# Query: %s\n", queryName);
    fprintf(f, "# Database: %s\n", databaseName);
    fprintf(f, "%s\n", 
    	"# Fields: Query id, Subject id, % identity, alignment length, "
	"mismatches, gap openings, q. start, q. end, s. start, s. end, "
	"e-value, bit score");
    }

/* Print out details on each target. */
targetList = bundleIntoTargets(abList);
for (target = targetList; target != NULL; target = target->next)
    {
    struct axtRef *ref;
    for (ref = target->axtList; ref != NULL; ref = ref->next)
        {
	struct axt *axt = ref->axt;
	int matches = countMatches(axt->qSym, axt->tSym, axt->symCount);
	int gaps = countGaps(axt->qSym, axt->tSym, axt->symCount);
	int gapOpens = countGapOpens(axt->qSym, axt->tSym, axt->symCount);
	fprintf(f, "%s\t", axt->qName);
	fprintf(f, "%s\t", axt->tName);
	fprintf(f, "%.2f\t", 100.0 * matches/axt->symCount);
	fprintf(f, "%d\t", axt->symCount);
	fprintf(f, "%d\t", axt->symCount - matches - gaps);
	fprintf(f, "%d\t", gapOpens);
	if (axt->qStrand == '-')
	    {
	    int s = axt->qStart, e = axt->qEnd;
	    reverseIntRange(&s, &e, querySize);
	    fprintf(f, "%d\t", s+1);
	    fprintf(f, "%d\t", e);
	    printAxtTargetBlastTab(f, axt, target->size);
	    }
	else
	    {
	    fprintf(f, "%d\t", axt->qStart + 1);
	    fprintf(f, "%d\t", axt->qEnd);
	    printAxtTargetBlastTab(f, axt, target->size);
	    }
	fprintf(f, "%3.1e\t", blastzScoreToNcbiExpectation(axt->score));
	fprintf(f, "%d.0\n", blastzScoreToNcbiBits(axt->score));
	}
    }

/* Cleanup time. */
targetHitsFreeList(&targetList);
}

void axtBlastOut(struct axtBundle *abList, 
	int queryIx, boolean isProt, FILE *f, 
	char *databaseName, int databaseSeqCount, double databaseLetterCount, 
	char *blastType, char *ourId, double minIdentity)
/* Output a bundle of axt's on the same query sequence in blast format.
 * The parameters in detail are:
 *   ab - the list of bundles of axt's. 
 *   f  - output file handle
 *   databaseSeqCount - number of sequences in database
 *   databaseLetterCount - number of bases or aa's in database
 *   blastType - blast/wublast/blast8/blast9/xml
 *   ourId - optional (may be NULL) thing to put in header
 */
{
if (abList == NULL)
    return;
if (sameWord(blastType, "wublast"))
    wuBlastOut(abList, queryIx, isProt, f, databaseName,
   	databaseSeqCount, databaseLetterCount, ourId);
else if (sameWord(blastType, "xml"))
    xmlBlastOut(abList, queryIx, isProt, f, databaseName,
        databaseSeqCount, databaseLetterCount, ourId);
else if (sameWord(blastType, "blast"))
    ncbiBlastOut(abList, queryIx, isProt, f, databaseName,
        databaseSeqCount, databaseLetterCount, ourId, minIdentity);
else if (sameWord(blastType, "blast8"))
    tabBlastOut(abList, queryIx, isProt, f, databaseName,
    	databaseSeqCount, databaseLetterCount, ourId, FALSE);
else if (sameWord(blastType, "blast9"))
    tabBlastOut(abList, queryIx, isProt, f, databaseName,
    	databaseSeqCount, databaseLetterCount, ourId, TRUE);
else
    errAbort("Unrecognized blastType %s in axtBlastOut", blastType);
}


/* chainScore - score chains. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dnaseq.h"
#include "twoBit.h"
#include "fa.h"
#include "axt.h"
#include "psl.h"
#include "boxClump.h"
#include "chainBlock.h"
#include "portable.h"
#include "gapCalc.h"
#include "chainConnect.h"

int minScore = 1000;
char *gapFileName = NULL;

struct axtScoreScheme *scoreScheme = NULL;

struct gapCalc *gapCalc = NULL;

struct seqPair
/* Pair of sequences. */
  {
    struct seqPair *next;
    char *name;	                /* Allocated in hash */
    char *qName;		/* Name of query sequence. */
    char *tName;		/* Name of target sequence. */
    char qStrand;		/* Strand of query sequence. */
    struct chain *chain; /* List of alignments. */
    };

int seqPairCmp(const void *va, const void *vb)
/* Compare to sort based on tName,qName. */
{
const struct seqPair *a = *((struct seqPair **)va);
const struct seqPair *b = *((struct seqPair **)vb);
int dif;
dif = strcmp(a->tName, b->tName);
if (dif == 0)
    dif = strcmp(a->qName, b->qName);
if (dif == 0)
    dif = (int)a->qStrand - (int)b->qStrand;
return dif;
}

void loadIfNewSeq(struct twoBitFile *tbf, char *newName, char strand,
	char **pName, struct dnaSeq **pSeq, char *pStrand)
/* Load sequence unless it is already loaded.  Reverse complement
 * if necessary. */
{
struct dnaSeq *seq;
if (sameString(newName, *pName))
    {
    if (strand != *pStrand)
        {
	seq = *pSeq;
	reverseComplement(seq->dna, seq->size);
	*pStrand = strand;
	}
    }
else
    {
    freeDnaSeq(pSeq);
    *pName = newName;
    *pSeq = seq = twoBitReadSeqFrag(tbf, newName, 0, 0);
    *pStrand = strand;
    if (strand == '-')
        reverseComplement(seq->dna, seq->size);
    verbose(2, "Loaded %d bases of %s in %s\n", seq->size, newName, tbf->fileName);
    }
}
void loadFaSeq(struct hash *faHash, char *newName, char strand, 
	char **pName, struct dnaSeq **pSeq, char *pStrand)
/* retrieve sequence from hash.  Reverse complement
 * if necessary. */
{
struct dnaSeq *seq;
if (sameString(newName, *pName))
    {
    if (strand != *pStrand)
        {
	seq = *pSeq;
	reverseComplement(seq->dna, seq->size);
	*pStrand = strand;
	}
    }
else
    {
    *pName = newName;
    *pSeq = seq = hashFindVal(faHash, newName);
    *pStrand = strand;
    if (strand == '-')
        reverseComplement(seq->dna, seq->size);
    verbose(2, "Loaded %d bases from %s fa\n", seq->size, newName);
    }
}

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainScore - score chains\n"
  "usage:\n"
  "   chainScore in.chain t.2bit q.2bit out.chain\n"
  "options:\n"
  "   -faQ  q.2bit is read as a fasta file\n"
  "   -minScore=N  Minimum score for chain, default %d\n"
  "   -scoreScheme=fileName Read the scoring matrix from a blastz-format file\n"
  "   -linearGap=filename Read piecewise linear gap from tab delimited file\n"
  "   sample linearGap file \n"
  "tablesize 11\n"
  "smallSize 111\n"
  "position 1 2 3 11 111 2111 12111 32111 72111 152111 252111\n"
  "qGap 350 425 450 600 900 2900 22900 57900 117900 217900 317900\n"
  "tGap 350 425 450 600 900 2900 22900 57900 117900 217900 317900\n"
  "bothGap 750 825 850 1000 1300 3300 23300 58300 118300 218300 318300\n"
  , minScore

  );
}

static struct optionSpec options[] = {
    { "faQ",         OPTION_BOOLEAN },
    { "minScore",    OPTION_INT },
    { "linearGap",   OPTION_STRING },
    { "scoreScheme", OPTION_STRING },
    {NULL, 0},
};

void scorePair(struct seqPair *sp,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct chain **pChainList,
	struct chain *chainList)
/* Chain up blocks and output. */
{
struct chain  *chain, *next;

for (chain = chainList; chain != NULL; chain = chain->next)
    {
    chain->score = chainCalcScore(chain, scoreScheme, gapCalc, qSeq, tSeq);
    }

/* Move chains scoring over threshold to master list. */
for (chain = chainList; chain != NULL; chain = next)
    {
    next = chain->next;
    if (chain->score >= minScore)
        {
	slAddHead(pChainList, chain);
	}
    else 
        {
	chainFree(&chain);
	}
    }
}

void doChainScore(char *chainIn, char *tTwoBitFile, char *qTwoBitFile, char *chainOut)
{
char qStrand = 0, tStrand = 0;
struct dnaSeq *qSeq = NULL, *tSeq = NULL;
char *qName = "",  *tName = "";
FILE *f = mustOpen(chainOut, "w");
struct chain *chainList = NULL, *chain;
struct dnaSeq *seq, *seqList = NULL;
struct hash *faHash = newHash(0);
struct hash *chainHash = newHash(0);
FILE *faF;
struct seqPair *spList = NULL, *sp;
struct dyString *dy = dyStringNew(512);
struct lineFile *chainsLf = lineFileOpen(chainIn, TRUE);
struct twoBitFile *tTbf = twoBitOpen(tTwoBitFile);
struct twoBitFile *qTbf = NULL;

while ((chain = chainRead(chainsLf)) != NULL)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", chain->qName, chain->qStrand, chain->tName);
    sp = hashFindVal(chainHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(chainHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(chain->qName);
	sp->tName = cloneString(chain->tName);
	sp->qStrand = chain->qStrand;
	}
    slAddHead(&sp->chain, chain);
    }
slSort(&spList, seqPairCmp);
lineFileClose(&chainsLf);

if (optionExists("faQ"))
    {
    faF = mustOpen(qTwoBitFile, "r");
    while ( faReadMixedNext(faF, TRUE, NULL, TRUE, NULL, &seq))
        {
        hashAdd(faHash, seq->name, seq);
        slAddHead(&seqList, seq);
        }
    fclose(faF);
    }
else
    qTbf = twoBitOpen(qTwoBitFile);
for (sp = spList; sp != NULL; sp = sp->next)
    {
    if (optionExists("faQ"))
        {
        assert (faHash != NULL);
        loadFaSeq(faHash, sp->qName, sp->qStrand, &qName, &qSeq, &qStrand);
        }
    else
        loadIfNewSeq(qTbf, sp->qName, sp->qStrand, &qName, &qSeq, &qStrand);
    loadIfNewSeq(tTbf, sp->tName, '+', &tName, &tSeq, &tStrand);
    scorePair(sp, qSeq, tSeq, &chainList, sp->chain);
    }


slSort(&chainList, chainCmpScore);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    assert(chain->qStart == chain->blockList->qStart 
	&& chain->tStart == chain->blockList->tStart);
    chainWrite(chain, f);
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
{
char *scoreSchemeName = NULL;
optionInit(&argc, argv, options);
minScore = optionInt("minScore", minScore);
gapFileName = optionVal("linearGap", NULL);
scoreSchemeName = optionVal("scoreScheme", NULL);
if (scoreSchemeName != NULL)
    {
    printf("Reading scoring matrix from %s\n", scoreSchemeName);
    scoreScheme = axtScoreSchemeRead(scoreSchemeName);
    }
else
    scoreScheme = axtScoreSchemeDefault();
dnaUtilOpen();
if (gapFileName)
    gapCalc=gapCalcFromFile(gapFileName);
else
    gapCalc = gapCalcDefault();
if (argc != 5)
    usage();
doChainScore(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

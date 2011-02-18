/* axtChain - Chain together axt alignments.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dnaseq.h"
#include "nib.h"
#include "twoBit.h"
#include "fa.h"
#include "axt.h"
#include "psl.h"
#include "boxClump.h"
#include "portable.h"
#include "chainBlock.h"
#include "gapCalc.h"
#include "chainConnect.h"

static char const rcsid[] = "$Id: axtChain.c,v 1.37 2009/01/15 06:37:16 markd Exp $";

/* Variables set via command line. */
int minScore = 1000;
char *detailsName = NULL;
char *gapFileName = NULL;
struct gapCalc *gapCalc = NULL;	/* Gap scoring scheme to use. */
struct axtScoreScheme *scoreScheme = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtChain - Chain together axt alignments.\n"
  "usage:\n"
  "   axtChain -linearGap=loose in.axt tNibDir qNibDir out.chain\n"
  "Where tNibDir/qNibDir are either directories full of nib files, or the\n"
  "name of a .2bit file\n"
  "options:\n"
  "   -psl Use psl instead of axt format for input\n"
  "   -faQ qNibDir is a fasta file with multiple sequences for query\n"
  "   -faT tNibDir is a fasta file with multiple sequences for target\n"
  "   -minScore=N  Minimum score for chain, default %d\n"
  "   -details=fileName Output some additional chain details\n"
  "   -scoreScheme=fileName Read the scoring matrix from a blastz-format file\n"
  "   -linearGap=<medium|loose|filename> Specify type of linearGap to use.\n"
  "              *Must* specify this argument to one of these choices.\n"
  "              loose is chicken/human linear gap costs.\n"
  "              medium is mouse/human linear gap costs.\n"
  "              Or specify a piecewise linearGap tab delimited file.\n"
  "   sample linearGap file (loose)\n"
  "%s"
  , minScore, gapCalcSampleFileContents()
  );
}


struct seqPair
/* Pair of sequences. */
    {
    struct seqPair *next;
    char *name;	                /* Allocated in hash */
    char *qName;		/* Name of query sequence. */
    char *tName;		/* Name of target sequence. */
    char qStrand;		/* Strand of query sequence. */
    struct cBlock *blockList; /* List of alignments. */
    int axtCount;		/* Count of axt's that make this up (just for stats) */
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

void addPslBlocks(struct cBlock **pList, struct psl *psl)
/* Add blocks (gapless subalignments) from psl to block list. */
{
int i;
for (i=0; i<psl->blockCount; ++i)
    {
    struct cBlock *b;
    int size;
    AllocVar(b);
    size = psl->blockSizes[i];
    b->qStart = b->qEnd = psl->qStarts[i];
    b->qEnd += size;
    b->tStart = b->tEnd = psl->tStarts[i];
    b->tEnd += size;
    slAddHead(pList, b);
    }
}

struct twoBitFile *twoBitOpenCached(char *path)
/* Return open two bit file associated with path. */
{
static struct hash *hash = NULL;
struct twoBitFile *tbf;
if (hash == NULL)
    hash = newHash(8);
tbf = hashFindVal(hash, path);
if (tbf == NULL)
    {
    tbf = twoBitOpen(path);
    hashAdd(hash, path, tbf);
    }
return tbf;
}

void loadIfNewSeq(char *seqPath, boolean isTwoBit, char *newName, char strand, 
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
    char fileName[512];
    freeDnaSeq(pSeq);
    if (isTwoBit)
        {
	struct twoBitFile *tbf = twoBitOpenCached(seqPath);
	*pSeq = seq = twoBitReadSeqFrag(tbf, newName, 0, 0);
	verbose(1, "Loaded %d bases of %s from %s\n", seq->size, newName, seqPath);
	}
    else
	{
	snprintf(fileName, sizeof(fileName), "%s/%s.nib", seqPath, newName);
	*pSeq = seq = nibLoadAllMasked(NIB_MASK_MIXED, fileName);
	verbose(1, "Loaded %d bases in %s\n", seq->size, fileName);
	}
    *pName = newName;
    *pStrand = strand;
    if (strand == '-')
	reverseComplement(seq->dna, seq->size);
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
    verbose(1, "Loaded %d bases from %s fa\n", seq->size, newName);
    }
}

void removeExactOverlaps(struct cBlock **pBoxList)
/* Remove from list blocks that start in exactly the same
 * place on both coordinates. */
{
struct cBlock *newList = NULL, *b, *next, *last = NULL;
slSort(pBoxList, cBlockCmpBoth);
for (b = *pBoxList; b != NULL; b = next)
    {
    next = b->next;
    if (last != NULL && b->qStart == last->qStart && b->tStart == last->tStart)
        {
	/* Fold this block into previous one. */
	if (last->qEnd < b->qEnd) last->qEnd = b->qEnd;
	if (last->tEnd < b->tEnd) last->tEnd = b->tEnd;
	freeMem(b);
	}
    else
	{
	slAddHead(&newList, b);
	last = b;
	}
    }
slReverse(&newList);
*pBoxList = newList;
}


#ifdef TESTONLY
void abortChain(struct chain *chain, char *message)
/* Report chain problem and abort. */
{
errAbort("%s tName %s, tStart %d, tEnd %d, qStrand %c, qName %s, qStart %d, qEnd %d", message, chain->tName, chain->tStart, chain->tEnd, chain->qStrand, chain->qName, chain->qStart, chain->qEnd);
}

void checkChainScore(struct chain *chain, struct dnaSeq *qSeq, struct dnaSeq *tSeq)
/* Check that chain score is reasonable. */
{
struct cBlock *b;
int totalBases = 0;
double maxPerBase = 100, maxScore;
int gapCount = 0;
int size = 0;
int gaplessScore = 0;
int oneScore = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    {
    size = b->qEnd - b->qStart;
    if (size != b->tEnd - b->tStart)
        abortChain(chain, "q/t size mismatch");
    totalBases += b->qEnd - b->qStart;
    ++gapCount;
    }
maxScore = totalBases * maxPerBase;
if (maxScore < chain->score)
    {
    verbose(1, "maxScore %f, chainScore %f\n", maxScore, chain->score);
    for (b = chain->blockList; b != NULL; b = b->next)
        {
	size = b->qEnd - b->qStart;
	oneScore = chainScoreBlock(qSeq->dna + b->qStart, tSeq->dna + b->tStart, size, scoreData.ss->matrix);
        verbose(1, " q %d, t %d, size %d, score %d\n",
             b->qStart, b->tStart, size, oneScore);
	gaplessScore += oneScore;
	}
    verbose(1, "gaplessScore %d\n", gaplessScore);
    abortChain(chain, "score too big");
    }
}
#endif /* TESTONLY */

static void checkBlockRange(char *what, struct dnaSeq *seq, int start, int end)
/* check block is in range of sequence */
{
if (end > seq->size)
    errAbort("%s %s block %d-%d exceeds sequence length %d", what, seq->name, start, end, seq->size);
}

void chainPair(struct seqPair *sp,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, struct chain **pChainList,
	FILE *details)
/* Chain up blocks and output. */
{
struct chain *chainList, *chain, *next;
struct cBlock *b;
long startTime, dt;
int size = 0;
struct chainConnect cc;

verbose(1, "chainPair %s\n", sp->name);

/* Set up info for connect function. */
ZeroVar(&cc);
cc.query = qSeq;
cc.target = tSeq;
cc.ss = scoreScheme;
cc.gapCalc = gapCalc;

/* Score blocks. */
for (b = sp->blockList; b != NULL; b = b->next)
    {
    size = b->qEnd - b->qStart;
    checkBlockRange("query", qSeq, b->qStart, b->qEnd);
    checkBlockRange("target", tSeq, b->tStart, b->tEnd);
    b->score = axtScoreUngapped(scoreScheme, qSeq->dna + b->qStart, tSeq->dna + b->tStart, size);
    }


/* Get chain list and clean it up a little. */
startTime = clock1000();
chainList = chainBlocks(sp->qName, qSeq->size, sp->qStrand, 
	sp->tName, tSeq->size, &sp->blockList, 
	(ConnectCost)chainConnectCost, (GapCost)chainConnectGapCost, 
	&cc, details);
dt = clock1000() - startTime;
verbose(1, "Main chaining step done in %ld milliseconds\n", dt);
for (chain = chainList; chain != NULL; chain = chain->next)
    {
    chainRemovePartialOverlaps(chain, qSeq, tSeq, scoreScheme->matrix);
    chainMergeAbutting(chain);
    chain->score = chainCalcScore(chain, scoreScheme, gapCalc,
    	qSeq, tSeq);
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

struct seqPair *readAxtBlocks(char *fileName, struct hash *pairHash, FILE *f)
/* Read in axt file and parse blocks into pairHash */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct dyString *dy = newDyString(512);
struct axt *axt;
struct seqPair *spList = NULL, *sp;

lineFileSetMetaDataOutput(lf, f);
lineFileSetUniqueMetaData(lf);
while ((axt = axtRead(lf)) != NULL)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%c%s", axt->qName, axt->qStrand, axt->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(axt->qName);
	sp->tName = cloneString(axt->tName);
	sp->qStrand = axt->qStrand;
	}
    axtAddBlocksToBoxInList(&sp->blockList, axt);
    sp->axtCount += 1;
    axtFree(&axt);
    }
lineFileClose(&lf);
dyStringFree(&dy);
slSort(&spList, seqPairCmp);
return spList;
}

struct seqPair *readPslBlocks(char *fileName, struct hash *pairHash, FILE *f)
/* Read in psl file and parse blocks into pairHash */
{
struct seqPair *spList = NULL, *sp;
struct lineFile *lf = pslFileOpenWithUniqueMeta(fileName, f);
struct dyString *dy = newDyString(512);
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    dyStringClear(dy);
    dyStringPrintf(dy, "%s%s%s", psl->qName, psl->strand, psl->tName);
    sp = hashFindVal(pairHash, dy->string);
    if (sp == NULL)
        {
	AllocVar(sp);
	slAddHead(&spList, sp);
	hashAddSaveName(pairHash, dy->string, sp, &sp->name);
	sp->qName = cloneString(psl->qName);
	sp->tName = cloneString(psl->tName);
	sp->qStrand = psl->strand[0];
	}
    addPslBlocks(&sp->blockList, psl);
    sp->axtCount += 1;
    pslFree(&psl);
    }

lineFileClose(&lf);
dyStringFree(&dy);
return spList;
}

void axtChain(char *axtIn, char *tNibDir, char *qNibDir, char *chainOut)
/* axtChain - Chain together axt alignments.. */
{
struct hash *pairHash = newHash(0);  /* Hash keyed by qSeq<strand>tSeq */
struct seqPair *spList = NULL, *sp;
FILE *f = mustOpen(chainOut, "w");
char *qName = "",  *tName = "";
struct dnaSeq *qSeq = NULL, *tSeq = NULL;
char qStrand = 0, tStrand = 0;
struct chain *chainList = NULL, *chain;
FILE *details = NULL;
struct dnaSeq *seq, *seqList = NULL;
struct hash *faHash = newHash(0);
struct hash *tFaHash = newHash(0);
FILE *faF;
boolean qIsTwoBit = twoBitIsFile(qNibDir);
boolean tIsTwoBit = twoBitIsFile(tNibDir);

axtScoreSchemeDnaWrite(scoreScheme, f, "axtChain");

if (detailsName != NULL)
    details = mustOpen(detailsName, "w");
/* Read input file and divide alignments into various parts. */
if (optionExists("psl"))
    spList = readPslBlocks(axtIn, pairHash, f);
else
    spList = readAxtBlocks(axtIn, pairHash, f);

if (optionExists("faQ"))
    {
    faF = mustOpen(qNibDir, "r");
    while ( faReadMixedNext(faF, TRUE, NULL, TRUE, NULL, &seq))
        {
        hashAdd(faHash, seq->name, seq);
        slAddHead(&seqList, seq);
        }
    fclose(faF);
    }
if (optionExists("faT"))
    {
    faF = mustOpen(tNibDir, "r");
    while ( faReadMixedNext(faF, TRUE, NULL, TRUE, NULL, &seq))
        {
        hashAdd(tFaHash, seq->name, seq);
        slAddHead(&seqList, seq);
        }
    fclose(faF);
    }
for (sp = spList; sp != NULL; sp = sp->next)
    {
    slReverse(&sp->blockList);
    removeExactOverlaps(&sp->blockList);
    verbose(1, "%d blocks after duplicate removal\n", slCount(sp->blockList));
    if (optionExists("faQ"))
        {
        assert (faHash != NULL);
        loadFaSeq(faHash, sp->qName, sp->qStrand, &qName, &qSeq, &qStrand);
        }
    else
	{
        loadIfNewSeq(qNibDir, qIsTwoBit, sp->qName, sp->qStrand, 
		&qName, &qSeq, &qStrand);
        }
    if (optionExists("faT"))
        {
        assert (tFaHash != NULL);
        loadFaSeq(tFaHash, sp->tName, '+', &tName, &tSeq, &tStrand);
        }
    else 
	{
        loadIfNewSeq(tNibDir, tIsTwoBit, sp->tName, '+', 
		&tName, &tSeq, &tStrand);
	}
    chainPair(sp, qSeq, tSeq, &chainList, details);
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

struct gapCalc *gapCalcReadOrDefault(char *fileName)
/* Return gaps from file, or default if fileName is NULL. */
{
if (fileName == NULL)
    errAbort("Must specify linear gap costs.  Use 'loose' or 'medium' for defaults\n");
return gapCalcFromFile(fileName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *scoreSchemeName = NULL;
optionHash(&argc, argv);
minScore = optionInt("minScore", minScore);
detailsName = optionVal("details", NULL);
gapFileName = optionVal("linearGap", NULL);
scoreSchemeName = optionVal("scoreScheme", NULL);

if (argc != 5)
    usage();

if (scoreSchemeName != NULL)
    {
    verbose(1, "Reading scoring matrix from %s\n", scoreSchemeName);
    scoreScheme = axtScoreSchemeRead(scoreSchemeName);
    }
else
    scoreScheme = axtScoreSchemeDefault();
dnaUtilOpen();
gapCalc = gapCalcReadOrDefault(gapFileName);
/* testGaps(); */
axtChain(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

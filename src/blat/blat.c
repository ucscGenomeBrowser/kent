/* blat - Standalone BLAT fast sequence search command line tool. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nib.h"
#include "psl.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "genoFind.h"

/* Variables that can be set from command line. */
int tileSize = 10;
int minMatch = 3;
int minBases = 20;
int maxGap = 2;
int repMatch = 1024;
boolean noHead = FALSE;
char *ooc = NULL;
boolean isProt = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blat - Standalone BLAT fast sequence search command line tool\n"
  "usage:\n"
  "   blat database query output.psl\n"
  "where:\n"
  "   database is either a .fa file, a .nib file, or a list of .fa or .nib files\n"
  "   query is similarly a .fa, .nib, or list of .fa or .nib files\n"
  "   output.psl is where to put the output.\n"
  "options:\n"
  "   -tileSize=N sets the size of perfectly matches.  Usually between 8 and 12\n"
  "               Default is 10\n"
  "   -minMatch=N sets the number of perfect tile matches.  Usually set from 2 to 4\n"
  "               Default is 3\n"
  "   -minBases=N sets minimum number of matching bases.  Default is 20\n"
  "   -maxGap=N   sets the size of maximum gap between tiles in a clump.  Usually set\n"
  "               from 0 to 3.  Default is 2.\n"
  "   -repMatch=N sets the number of repetitions of a tile allowed before\n"
  "               it is masked.  Typically this is 1024 for tileSize 12,\n"
  "               4096 for tile size 11, 16384 for tile size 10.\n"
  "               Default is 16384\n"
  "   -noHead     suppress .psl header (so it's just a tab-separated file)\n"
  "   -ooc=N.ooc  Use overused tile file N.ooc\n"
  "   -prot       Query sequence is protein\n"
  );
}

boolean isNib(char *fileName)
/* Return TRUE if file is a nib file. */
{
return endsWith(fileName, ".nib") || endsWith(fileName, ".NIB");
}

void getFileArray(char *fileName, char ***retFiles, int *retFileCount)
/* Check if file if .fa or .nib.  If so return just that
 * file in a list of one.  Otherwise read all file and treat file
 * as a list of filenames.  */
{
boolean gotSingle = FALSE;
char *buf;		/* This will leak memory but won't matter. */

/* Detect nib files by suffix since that is standard. */
if (isNib(fileName))
    gotSingle = TRUE;
/* Detect .fa files (where suffix is not standardized)
 * by first character being a '>'. */
else
    {
    FILE *f = mustOpen(fileName, "r");
    char c = fgetc(f);
    fclose(f);
    if (c == '>')
        gotSingle = TRUE;
    }
if (gotSingle)
    {
    char **files;
    *retFiles = AllocArray(files, 1);
    files[0] = cloneString(fileName);
    *retFileCount = 1;
    return;
    }
else
    {
    readAllWords(fileName, retFiles, retFileCount, &buf);
    }
}

bioSeq *getSeqList(int fileCount, char *files[], struct hash *hash)
/* From an array of .fa and .nib file names, create a
 * list of dnaSeqs. */
{
int i;
char *fileName;
bioSeq *seqList = NULL, *seq;
int count = 0; 
unsigned long totalSize = 0;

for (i=0; i<fileCount; ++i)
    {
    fileName = files[i];
    if (isNib(fileName))
        {
	FILE *f;
	int size;

	nibOpenVerify(fileName, &f, &size);
	seq = nibLdPart(fileName, f, size, 0, size);
	seq->name = fileName;
	carefulClose(&f);
	slAddHead(&seqList, seq);
	hashAddUnique(hash, seq->name, seq);
	totalSize += size;
	count += 1;
	}
    else
        {
	struct dnaSeq *list, *next;
	list = faReadAllSeq(fileName, !isProt);
	for (seq = list; seq != NULL; seq = next)
	    {
	    next = seq->next;
	    slAddHead(&seqList, seq);
	    hashAddUnique(hash, seq->name, seq);
	    totalSize += seq->size;
	    count += 1;
	    }
	}
    }
printf("Indexing %lu letters in %d sequences\n", totalSize, count);
slReverse(&seqList);
return seqList;
}

void searchOneStrand(struct dnaSeq *seq, struct genoFind *gf, FILE *psl, boolean isRc)
/* Search for seq in index, align it, and write results to psl. */
{
struct gfClump *clumpList = gfFindClumps(gf, seq);
gfAlignSeqClumps(clumpList, seq, isRc, ffCdna, minBases, gfSavePsl, psl);
gfClumpFreeList(&clumpList);
}

int aaScoreMatch(char a, char b)
/* Score match between two amino acids. */
{
/* Current implementation is just a crude place-holder. */
if (a == b)  return 3;
else return -1;
}

static int maxDown = 10;

void extendHitRight(int qMax, int tMax,
	char **pEndQ, char **pEndT, int (*scoreMatch)(char a, char b))
/* Extend endQ/endT as much to the right as possible. */
{
int maxScore = 0;
int score = 0;
int maxPos = -1;
int last = min(qMax, tMax);
int i;
char *q = *pEndQ, *t = *pEndT;

uglyf("  extend hit right qMax %d, tMax %d\n", qMax, tMax);
for (i=0; i<last; ++i)
    {
    score += scoreMatch(q[i], t[i]);
    if (score > maxScore)
	 {
         maxScore = score;
	 maxPos = i;
	 }
    else
         {
	 if (i - maxPos >= maxDown)
	     break;
	 }
    }
uglyf("  Extended to right %d, score up %d\n", maxPos+1, maxScore);
*pEndQ = q+maxPos+1;
*pEndT = t+maxPos+1;
}

void extendHitLeft(int qMax, int tMax,
	char **pStartQ, char **pStartT, int (*scoreMatch)(char a, char b))
/* Extend startQ/startT as much to the left as possible. */
{
int maxScore = 0;
int score = 0;
int maxPos = 0;
int last = -min(qMax, tMax);
int i;
char *q = *pStartQ, *t = *pStartT;

uglyf("  extend hit left qMax %d, tMax %d\n", qMax, tMax);
for (i=-1; i>=last; --i)
    {
    score += scoreMatch(q[i], t[i]);
    if (score > maxScore)
	 {
         maxScore = score;
	 maxPos = i;
	 }
    else
         {
	 if (i - maxPos >= maxDown)
	     break;
	 }
    }
uglyf("  Extended to left %d, score up %d\n", -maxPos, maxScore);
*pStartQ = q+maxPos;
*pStartT = t+maxPos;
}


struct ffAli *clumpToHsp(struct gfClump *clump, aaSeq *qSeq, int tileSize)
/* Extend clump to HSP (high scoring local sequence pair:
 * that is longest alignment without gaps */
{
struct gfSeqSource *target = clump->target;
aaSeq *tSeq = target->seq;
int maxDown = 10;
AA *qs, *ts, *qe, *te;
int maxScore = 0, maxPos = 0, score, pos;
struct gfHit *hit;
int qStart, tStart, qEnd, tEnd, newQ, newT;
boolean outOfIt = TRUE;		/* Logically outside of a clump. */

if (tSeq == NULL)
    internalErr();

/* The termination condition of this loop is a little complicated.
 * We want to output something either when the next hit can't be
 * merged into the previous, or at the end of the list.  To avoid
 * duplicating the output code we're forced to complicate the loop
 * termination logic. */
for (hit = clump->hitList; ; hit = hit->next)
    {
    if (hit != NULL)
        {
	newQ = hit->qStart;
	newT = hit->tStart - target->start;
	uglyf("  hit %s %d %s %d\n", qSeq->name, hit->qStart, tSeq->name, hit->tStart);
	}

    /* See if it's time to output merged (diagonally adjacent) hits. */
    if (!outOfIt)	/* Not first time through. */
        {
	if (hit == NULL || newQ != qEnd || newT != tEnd)
	    {
	    uglyf("Output clump %d-%d %d-%d\n", qStart, qEnd, tStart, tEnd);
	    qs = qSeq->dna + qStart;
	    ts = tSeq->dna + tStart;
	    qe = qSeq->dna + qEnd;
	    te = tSeq->dna + tEnd;
	    extendHitRight(qSeq->size - qEnd, tSeq->size - tEnd,
		&qe, &te, aaScoreMatch);
	    extendHitLeft(qStart, tStart, &qs, &ts, aaScoreMatch);
	    outOfIt = TRUE;
	    }
	}
    if (hit == NULL)
        break;

    if (outOfIt)
        {
	qStart = newQ;
	qEnd = qStart + tileSize;
	tStart = newT;
	tEnd = tStart + tileSize;
	outOfIt = FALSE;
	}
    else
        {
	qEnd = newQ + tileSize;
	tEnd = newT + tileSize;
	}
    }
return NULL;	/* ~~~ */
}

void searchOneProt(aaSeq *seq, struct genoFind *gf, FILE *psl)
/* Search for protein seq in index and write results to psl. */
{
struct gfClump *clump, *clumpList = gfFindClumps(gf, seq);
struct ffAli *hspList = NULL, *hsp;

uglyf("Found %d clumps in %s\n", slCount(clumpList), seq->name);
for (clump = clumpList; clump != NULL; clump = clump->next)
    {
    gfClumpDump(gf, clump, uglyOut);
    hsp = clumpToHsp(clump, seq, gf->tileSize);
    }
}

void searchOne(bioSeq *seq, struct genoFind *gf, FILE *psl)
/* Search for seq on either strand in index. */
{
if (isProt)
    {
    searchOneProt(seq, gf, psl);
    }
else
    {
    searchOneStrand(seq, gf, psl, FALSE);
    reverseComplement(seq->dna, seq->size);
    searchOneStrand(seq, gf, psl, TRUE);
    reverseComplement(seq->dna, seq->size);
    }
}

void searchIndex(int fileCount, char *files[], struct genoFind *gf, char *pslOut)
/* Search all sequences in all files against genoFind index. */
{
int i;
char *fileName;
bioSeq *seqList = NULL, *targetSeq;
int count = 0; 
unsigned long totalSize = 0;
FILE *psl = mustOpen(pslOut, "w");

if (!noHead)
    pslWriteHead(psl);
for (i=0; i<fileCount; ++i)
    {
    fileName = files[i];
    if (isNib(fileName))
        {
	FILE *f;
	int size;
	struct dnaSeq *seq;

	if (isProt)
	    errAbort("%s: Can't use .nib files with -prot option\n", fileName);
	nibOpenVerify(fileName, &f, &size);
	seq = nibLdPart(fileName, f, size, 0, size);
	freez(&seq->name);
	seq->name = cloneString(fileName);
	carefulClose(&f);
	searchOne(seq, gf, psl);
	freeDnaSeq(&seq);
	totalSize += size;
	count += 1;
	}
    else
        {
	static struct dnaSeq seq;
	struct lineFile *lf = lineFileOpen(fileName, TRUE);
	while (faSomeSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name, !isProt))
	    {
	    searchOne(&seq, gf, psl);
	    totalSize += seq.size;
	    count += 1;
	    }
	lineFileClose(&lf);
	}
    }
carefulClose(&psl);
printf("Searched %lu bases in %d sequences\n", totalSize, count);
}


void blat(char *dbFile, char *queryFile, char *pslOut)
/* blat - Standalone BLAT fast sequence search command line tool. */
{
char **dbFiles, **queryFiles;
int dbCount, queryCount;
struct dnaSeq *dbSeqList, *seq;
struct hash *dbHash = newHash(16);
struct genoFind *gf;

getFileArray(dbFile, &dbFiles, &dbCount);
getFileArray(queryFile, &queryFiles, &queryCount);
dbSeqList = getSeqList(dbCount, dbFiles, dbHash);
gf = gfIndexSeq(dbSeqList, minMatch, maxGap, tileSize, repMatch, ooc, isProt);
searchIndex(queryCount, queryFiles, gf, pslOut);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
tileSize = cgiOptionalInt("tileSize", tileSize);
if (cgiVarExists("prot"))
    {
    uglyf("is prot\n");
    isProt = TRUE;
    tileSize = 4;
    minMatch = 3;
    maxGap = 0;
    }
if (isProt)
    {
    if (tileSize < 2 || tileSize > 6)
	errAbort("protein tileSize must be between 2 and 6");
    }
else
    {
    if (tileSize < 6 || tileSize > 15)
	errAbort("DNA tileSize must be between 6 and 15");
    }
minMatch = cgiOptionalInt("minMatch", minMatch);
if (minMatch < 0)
    errAbort("minMatch must be at least 1");
minBases = cgiOptionalInt("minBases", minBases);
maxGap = cgiOptionalInt("maxGap", maxGap);
if (maxGap > 100)
    errAbort("maxGap must be less than 100");
if (cgiVarExists("repMatch"))
    repMatch = cgiInt("repMatch");
else
    {
    if (tileSize == 15)
        repMatch = 16;
    else if (tileSize == 14)
        repMatch = 64;
    else if (tileSize == 13)
        repMatch = 256;
    else if (tileSize == 12)
        repMatch = 1024;
    else if (tileSize == 11)
        repMatch = 4*1024;
    else if (tileSize == 10)
        repMatch = 16*1024;
    else if (tileSize == 9)
        repMatch = 64*1024;
    else if (tileSize == 8)
        repMatch = 256*1024;
    else if (tileSize == 7)
        repMatch = 1024*1024;
    else if (tileSize == 6)
        repMatch = 4*1024*1024;
    }
noHead = (cgiVarExists("noHead") || cgiVarExists("nohead"));
ooc = cgiOptionalString("ooc");

blat(argv[1], argv[2], argv[3]);
return 0;
}

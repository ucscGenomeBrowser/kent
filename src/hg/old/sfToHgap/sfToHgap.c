/* sfToHgap - Load database with alignments from an .sf file. */
#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "hgap.h"
#include "mrnaToContig.h"

int targetCmp(const void *va, const void *vb)
/* Compare two mrnaToContigs by target. */
{
const struct mrnaToContig *a = *((struct mrnaToContig **)va);
const struct mrnaToContig *b = *((struct mrnaToContig **)vb);
int diff = strcmp(a->tAcc, b->tAcc);
if (diff == 0)
    diff = a->tContig - b->tContig;
return diff;
}

struct mrnaToContig *mtcFromSfLine(struct lineFile *lf, char *line)
/* Make up an alignment based on line.  Don't fill in the blocks
 * part of alignment yet though or scores.  A line of an SF file 
 * looks like this:
 *  96.50% - AA000973:248-363 of 363 at AC012670:79222-79337 of 160093
 */
{
struct mrnaToContig *mtc;
char *words[64];
int wordCount;
char *parts[4];
int partCount;
char *s;
int contigIx = 0;

AllocVar(mtc);
wordCount = chopLine(line, words);
if (wordCount != 9)
    errAbort("Bad line %d of %s\n", lf->lineIx, lf->fileName);
mtc->id = hgNextId();
if (words[1][0] == '-')
    mtc->qOrientation = -1;
else
    mtc->qOrientation = 1;
partCount = chopString(words[2], ":-", parts, ArraySize(parts));
if (partCount != 3)
    errAbort("Bad query part count %d of %s\n", lf->lineIx, lf->fileName);
strncpy(mtc->qAcc,parts[0], sizeof(mtc->qAcc));
mtc->qStart = sqlUnsigned(parts[1]);
mtc->qEnd = sqlUnsigned(parts[2]);
mtc->qSize = sqlUnsigned(words[4]);
partCount = chopString(words[6], ":-", parts, ArraySize(parts));
if (partCount != 3)
    errAbort("Bad target parts %d of %s\n", lf->lineIx, lf->fileName);
s = strrchr(parts[0], '.');
if (s != NULL)
    {
    *s++ = 0;
    contigIx = sqlUnsigned(s)-1;
    }
strncpy(mtc->tAcc,parts[0], sizeof(mtc->tAcc));
mtc->tContig = contigIx;
mtc->tStart = sqlUnsigned(parts[1]);
mtc->tEnd = sqlUnsigned(parts[2]);
mtc->tSize = sqlUnsigned(words[8]);
return mtc;
}

struct mrnaToContig *loadSfFile(char *fileName, struct sqlConnection *conn)
/* Load up lines of Sf file.  Don't do detailed alignments though. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int lineSize;
char *line;
struct mrnaToContig *mtcList = NULL, *mtc;
int lineMaxMod = 10000;
int lineMod = lineMaxMod; 
int aliCount = 0;
long startTime = clock1000(), endTime;

while (lineFileNext(lf, &line, &lineSize))
    {
    mtc = mtcFromSfLine(lf, line);
    slAddHead(&mtcList, mtc);
    ++aliCount;
    if (--lineMod == 0)
	{
	printf("Alignment %d\n", aliCount);
	lineMod = lineMaxMod;
	}
    }
lineFileClose(&lf);
endTime = clock1000();
printf("Got %d alignments in %4.2f seconds\n", slCount(mtcList), 0.001*(endTime-startTime));
startTime = endTime;
slSort(&mtcList, targetCmp);
endTime = clock1000();
printf("Sorted in %4.2f seconds\n", 0.001*(endTime-startTime));
return mtcList;
}

int countMatches(DNA *a, DNA *b, int size)
/* Return number of non-N matching bases. */
{
DNA aa, bb;
int i;
int count = 0;

for (i=0; i<size; ++i)
    {
    aa = a[i];
    bb = b[i];
    if (aa == bb && aa != 'n')
	++count;
    }
return count;
}

int countMismatches(DNA *a, DNA *b, int size)
/* Return number of non-N mismatching bases. */
{
DNA aa, bb;
int i;
int count = 0;

for (i=0; i<size; ++i)
    {
    aa = a[i];
    bb = b[i];
    if (aa != bb && aa != 'n' && bb != 'n')
	++count;
    }
return count;
}

void sfToHgap(char *fileName, char *database)
/* Load up database with alignments in sf file. */
{
struct mrnaToContig *mtcList = NULL, *mtc, *mtcNext;
struct sqlConnection *conn;
char *lastTacc = "";
struct dnaSeq *tSeqList = NULL;
struct dnaSeq *qSeq, *tSeq;
long startTime, endTime;
int lineMaxMod = 50;
int lineMod = lineMaxMod;
int maxMod = 200;
int mod = maxMod;
int aliCount = 0;
int bacsLoaded = 0;
struct ffAli *ffLeft, *ff;
int blockCount;
struct mtcBlock *block;
int i;
char *tableName = "mrnaToContig";
FILE *f = hgCreateTabFile(tableName);


hgSetDb(database);
conn = hgStartUpdate();
mtcList = loadSfFile(fileName, conn);

startTime = clock1000();
for (mtc = mtcList; mtc != NULL; mtc = mtcNext)
    {
    DNA *qDna, *tDna;
    int matchCount = 0;
    int mismatchCount = 0;
    int nCount = 0;

    mtcNext = mtc->next;
    ++aliCount;
    if (--mod == 0)
	{
	printf(".");
	fflush(stdout);
	mod = maxMod;
	if (--lineMod == 0)
	    {
	    printf(" %d\n", aliCount);
	    lineMod = lineMaxMod;
	    }
	}
    if (!sameString(lastTacc, mtc->tAcc))
	{
	++bacsLoaded;
	freeDnaSeqList(&tSeqList);
	tSeqList = hgBacContigSeq(mtc->tAcc);
	lastTacc = mtc->tAcc;
	}
    tSeq = slElementFromIx(tSeqList, mtc->tContig);
    if (tSeq == NULL)
	errAbort("Not %d contigs in %s - database out of sync?\n", mtc->tContig+1, mtc->tAcc);
    qSeq = hgRnaSeq(mtc->qAcc);
    if (mtc->qOrientation < 0)
	reverseComplement(qSeq->dna, qSeq->size);
    qDna = qSeq->dna;
    tDna = tSeq->dna;
    ffLeft = ffFind(qDna + mtc->qStart, qDna + mtc->qEnd,
    		   tDna + mtc->tStart, tDna + mtc->tEnd,
		   ffCdna);
    if (ffLeft != NULL)
	{
	blockCount = mtc->blockCount = ffAliCount(ffLeft);
	mtc->blocks = AllocArray(block, blockCount);
	for (i=0, ff=ffLeft; i<blockCount; ++i, ff=ff->right)
	    {
	    int size = block->size = ff->nEnd - ff->nStart;
	    block->qStart = ff->nStart - qDna;
	    block->tStart = ff->hStart - tDna;
	    ++block;
	    matchCount += countMatches(ff->nStart, ff->hStart, size);
	    mismatchCount += countMismatches(ff->nStart, ff->hStart, size);
	    }
	mtc->matchCount = matchCount;
	mtc->mismatchCount = mismatchCount;
	mrnaToContigTabOut(mtc, f);
	mrnaToContigFree(&mtc);
	ffFreeAli(&ffLeft);
	}
    else
	{
	warn("Couldn't realign %s:%d-%d to %s.%d:%d-%d",
		mtc->qAcc, mtc->qStart, mtc->qEnd,
		mtc->tAcc, mtc->tContig, mtc->tStart, mtc->tEnd);
	}
    freeDnaSeq(&qSeq);
    }
freeDnaSeqList(&tSeqList);
endTime = clock1000();
printf("Did %d alignments in %4.2f seconds loading %d bacs\n", aliCount, 0.001*(endTime-startTime),
	bacsLoaded);
startTime = endTime;
fclose(f);
hgLoadTabFile(conn, tableName);
hgEndUpdate(&conn, "sfToHgap %s (%d alignments)", fileName, aliCount);
endTime = clock1000();
printf("Updated database in %4.2 seconds\n", 0.001*(endTime-startTime));
}

void usage()
/* Explain usage and exit. */
{
errAbort(
    "sfToHgap - Loads database with alignments from a .sf file\n"
    "usage:\n"
    "   sfToHgap file.sf database");
}

int main(int argc, char *argv[])
/* Check command line. */
{
if (argc != 3)
    usage();
sfToHgap(argv[1], argv[2]);
}

/* waba - Wobble Aware Bulk Aligner.  This is a three pass
 * alignment algorithm for nucleotide sequences that is
 * intended primarily for genomic vs. genomic alignments.
 * It's described in the Genome Research article by WJ Kent
 * and AM Zahler.
 *
 * This file contains a driver for the algorithm that can
 * run either a single pass or all three passes at once.
 * It's primarily intended for batch jobs.  See dynAlign
 * for a cgi interface for smaller jobs.
 */

#include "common.h"
#include "portable.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "nt4.h"
#include "crudeali.h"
#include "xenalign.h"
#include "wabaCrude.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
 "waba - a program for doing genomic vs. genomic DNA alignments\n"
 "usage:\n"
 "   waba 1 small.lst big.lst output.1\n"
 "Runs first pass alignment between all the fasta files in small.lst\n"
 "vs. all the fasta files in big.lst writing output to output.1\n"
 "   waba 2 input.1 output.2\n"
 "Runs second pass alignment taking results from first pass (in \n"
 "input.1 and using them to create output.2\n"
 "   waba 3 input.2 output.st\n"
 "Runs third pass alignment on second pass results to get final output\n"
 "\n"
 "The results of all three passes are text files.  You can also run\n"
 "all three passes together by the command\n"
 "   waba all small.lst big.lst output.wab\n"
 "To view a .wab file in a more human readable form try:\n"
 "   waba view file.wab\n"
 );
}


void doCrude(char *qFile, struct dnaSeq *qSeq, struct nt4Seq **nt4s, int nt4Count, FILE *out)
/* Do crude alignment of qSeq against nt4s putting results in f */
{
struct crudeAli *caList, *ca;
int i;
int lqSize;
int qMaxSize = 2000;
int tMaxSize = 5000;
int qSize = qSeq->size;
int lastQsection = qSize - qMaxSize/2;
int tMin, tMax, tMid, tSize;
DNA *query = qSeq->dna;
int qStart,qEnd;

for (i = 0; i<lastQsection || i == 0; i += qMaxSize/2)
    {
    lqSize = qSize - i;
    if (lqSize > qMaxSize)
        lqSize = qMaxSize;
    caList = crudeAliFind(query+i, lqSize, nt4s, nt4Count, 8, 45);
    qStart = i;
    qEnd = i+lqSize;
    for (ca = caList; ca != NULL; ca = ca->next)
        {
	char *targetCompositeName;
	char targetFile[512];
	char targetName[256];
	char *s;
	int tNameSize;

	targetCompositeName = nt4s[ca->chromIx]->name;
	s = strchr(targetCompositeName, '@');
	tNameSize = s - targetCompositeName;
	memcpy(targetName, targetCompositeName, tNameSize);
	targetName[tNameSize] = 0;
	strcpy(targetFile, s+1);

	fprintf(out, "%d\t%s\t%s\t%d\t%d\t%d\t%s\t%s\t%d\t%d\n",
		ca->score, qFile, qSeq->name, qStart, qEnd,
		(ca->strand == '-' ? -1 : 1), 
		targetFile, targetName, ca->start, ca->end);
        }
    slFreeList(&caList);
    printf(".");
    fflush(stdout);
    }
}

void firstPass(char *aList, char *bList, char *outName)
/* Do first pass - find areas of homology between a and b,
 * save to outName. */
{
char *aNameBuf, **aNames;
char *bNameBuf, **bNames;
int aCount, bCount;
struct nt4Seq **bNts, *bNt, *bNtList = NULL;
int bNtCount;
int i;
FILE *out = mustOpen(outName, "w");

/* Read in fa file lists . */
readAllWords(aList, &aNames, &aCount, &aNameBuf);
readAllWords(bList, &bNames, &bCount, &bNameBuf);

/* Convert second list to nt4 (packed) format in memory. */
printf("Loading and packing dna in %s\n", bList);
for (i=0; i<bCount; ++i)
    {
    char *bName = bNames[i];
    struct dnaSeq *seqList, *seq;

    seqList = faReadAllDna(bName);
    for (seq = seqList; seq != NULL; seq = seq->next)
	{
	char uniqName[512];
	sprintf(uniqName, "%s@%s", seq->name, bName);
	bNt = newNt4(seq->dna, seq->size, uniqName);
	slAddHead(&bNtList, bNt);
	}
    freeDnaSeqList(&seqList);
    }
slReverse(&bNtList);
bNtCount = slCount(bNtList);
AllocArray(bNts, bNtCount);
for (i=0, bNt=bNtList; i<bNtCount; ++i, bNt=bNt->next)
    bNts[i] = bNt;
printf("Loaded %d contigs from %d files\n", bNtCount, bCount);

/* Align elements of A list one at a time against B list. */
for (i=0; i<aCount; ++i)
    {
    char *aName = aNames[i];
    struct dnaSeq *seqList, *seq;

    printf("Aligning %s against %s\n", aName, bList);
    seqList = faReadAllDna(aName);
    for (seq = seqList; seq != NULL; seq = seq->next)
	{
	doCrude(aName, seq, bNts, bNtCount, out);
	}
    printf("\n");
    freeDnaSeqList(&seqList);
    }


/* Cleanup time. */
for (i=0; i<bNtCount; ++i)
    freeNt4(&bNts[i]);
freeMem(bNts);
freeMem(aNames);
freeMem(bNames);
freeMem(aNameBuf);
freeMem(bNameBuf);
fclose(out);
}

int wcCmpQposScore(const void *va, const void *vb)
/* Compare two wabaCrude's by query, qStart, score (descending). */
{
const struct wabaCrude *a = *((struct wabaCrude **)va);
const struct wabaCrude *b = *((struct wabaCrude **)vb);
int diff;
diff = strcmp(a->qFile, b->qFile);
if (diff == 0)
    diff = strcmp(a->qSeq, b->qSeq);
if (diff == 0)
    diff = a->qStart, b->qStart;
if (diff == 0)
    diff = b->score - a->score;
return diff;
}

struct dnaSeq *findSeq(struct dnaSeq *list, char *name)
/* Find sequence of given name in list.  Abort if not found. */
{
struct dnaSeq *el;

for (el = list; el != NULL; el = el->next)
    {
    if (sameString(name, el->name))
	return el;
    }
errAbort("Couldn't find %s", name);
}

void htvFreeSeq(void *val)
/* Free dnaSeq list in hash table */
{
struct dnaSeq *seqList = val;
freeDnaSeqList(&seqList);
}

void secondPass(char *inName, char *outName)
/* Do second pass - pair HMM between homologous regions specified in
 * input. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
char *line;
int lineSize;
char *words[16];
int wordCount;
struct wabaCrude *wcList = NULL, *wc;
char qFileName[512];
struct dnaSeq *qSeqList = NULL, *seq;
struct hash *tFileHash = newHash(8);
struct hash *qSeqHash = NULL;
FILE *out = mustOpen(outName, "w");
FILE *dynFile;

printf("Second pass (HMM) input %s output %s\n", inName, outName);

/* Load up alignments from file and sort. */
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount != 10)
	errAbort("line %d of %s doesn't look like a waba first pass file",
	         lf->lineIx, lf->fileName);
    wc = wabaCrudeLoad(words);
    slAddHead(&wcList, wc);
    }
lineFileClose(&lf);
slSort(&wcList, wcCmpQposScore);


/* Go through alignments one by one, loading DNA as need be.  */
qFileName[0] = 0;
for (wc = wcList; wc != NULL; wc = wc->next)
    {
    struct hashEl *hel;
    struct dnaSeq *tSeqList, *tSeq, *qSeq;
    int qSize;
    DNA *qStart;
    int tMaxSize = 5000;
    int tMin, tMax, tMid, tSize;
    int score;

    /* Get target sequence. */
    hel = hashLookup(tFileHash, wc->tFile);
    if (hel == NULL)
	{
	printf("Loading %s\n", wc->tFile);
	tSeqList = faReadAllDna(wc->tFile);
	hel = hashAdd(tFileHash, wc->tFile, tSeqList);
	}
    else
	{
	tSeqList = hel->val;
	}
    tSeq = findSeq(tSeqList, wc->tSeq);

    /* Get query sequence. */
    if (!sameString(qFileName, wc->qFile))
	{
	strcpy(qFileName, wc->qFile);
	printf("Loading %s\n", wc->qFile);
	freeDnaSeqList(&qSeqList);
	qSeqList = faReadAllDna(wc->qFile);
	freeHash(&qSeqHash);
	qSeqHash = newHash(0);
	for (qSeq = qSeqList; qSeq != NULL; qSeq = qSeq->next)
	    hashAddUnique(qSeqHash, qSeq->name, qSeq);
	}
    qSeq = hashMustFindVal(qSeqHash, wc->qSeq);

    /* Do fine alignment. */
    qSize = wc->qEnd - wc->qStart;
    qStart = qSeq->dna + wc->qStart;
    if (wc->strand < 0)
	reverseComplement(qStart, qSize);

    tMid = (wc->tStart + wc->tEnd)/2;
    tMin = tMid-tMaxSize/2;
    tMax = tMin + tMaxSize;
    if (tMin < 0)
	tMin = 0;
    if (tMax > tSeq->size)
	tMax = tSeq->size;


    printf("Aligning %s %s:%d-%d %c to %s.%s:%d-%d +\n",
	wc->qFile, qSeq->name, wc->qStart, wc->qEnd, 
	(wc->strand < 0 ? '-' : '+'),
	wc->tFile, tSeq->name, tMin, tMax);

    fprintf(out, "Aligning %s %s:%d-%d %c to %s.%s:%d-%d +\n",
	wc->qFile, qSeq->name, wc->qStart, wc->qEnd, 
	(wc->strand < 0 ? '-' : '+'),
	wc->tFile, tSeq->name, tMin, tMax);

    score = xenAlignSmall(qStart, qSize, tSeq->dna + tMin, 
    	tMax-tMin, out, FALSE);        
    fprintf(out, "best score %d\n", score);

    if (wc->strand < 0)
	reverseComplement(qStart, qSize);
    }

freeDnaSeqList(&qSeqList);
hashTraverseVals(tFileHash, htvFreeSeq);
wabaCrudeFreeList(&wcList);
freeHash(&tFileHash);
fclose(out);
}

void thirdPass(char *inName, char *outName)
/* Do third pass - stitching together overlapping HMM alignments. */
{
FILE *out = mustOpen(outName, "w");
printf("Third pass (stitcher) input %s output %s\n", inName, outName);
xenStitch(inName, out,  150000, TRUE);
fclose(out);
}

void viewWaba(char *wabName)
/* Show human readable waba alignment. */
{
struct lineFile *lf = lineFileOpen(wabName, TRUE);
int lineSize;
char *line;
char *qSym;
char *tSym;
char *hSym;
int symCount;
int wordCount, partCount;
char *words[16], *parts[4];
int qStart, qEnd, tStart, tEnd;
char strand;

while (lineFileNext(lf, &line, &lineSize))
    {
    printf("%s\n", line);
    wordCount = chopLine(line, words);
    if (wordCount != 10)
        errAbort("Funny info line %d of %s\n", lf->lineIx, lf->fileName);
    partCount = chopString(words[6], ":-", parts, ArraySize(parts));
    if (partCount != 3)
        errAbort("Bad query range line %d of %s\n", lf->lineIx, lf->fileName);
    qStart = atoi(parts[1]);
    qEnd = atoi(parts[2]);
    strand = words[7][0];
    partCount = chopString(words[8], ":-", parts, ArraySize(parts));
    if (partCount != 3)
        errAbort("Bad target range line %d of %s\n", lf->lineIx, lf->fileName);
    tStart = atoi(parts[1]);
    tEnd = atoi(parts[2]);

    if (!lineFileNext(lf, &line, &lineSize))
        errAbort("Unexpected EOF.");
    symCount = strlen(line);
    qSym = cloneString(line);
    if (!lineFileNext(lf, &line, &lineSize))
        errAbort("Unexpected EOF.");
    tSym = cloneString(line);
    if (!lineFileNext(lf, &line, &lineSize))
        errAbort("Unexpected EOF.");
    hSym = cloneString(line);
    if (strand == '+')
	xenShowAli(qSym, tSym, hSym, symCount, stdout, qStart, tStart, '+', '+', 60);
    else
	xenShowAli(qSym, tSym, hSym, symCount, stdout, qEnd, tStart, '-', '+', 60);
    freeMem(hSym);
    freeMem(tSym);
    freeMem(qSym);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line and call passes. */
{
char *command;

if (argc < 3)
    usage();
command = argv[1];
if (sameWord(command, "1"))
    {
    if (argc != 5)
	usage();
    firstPass(argv[2], argv[3], argv[4]);
    }
else if (sameWord(command, "2"))
    {
    if (argc != 4)
	usage();
    secondPass(argv[2], argv[3]);
    }
else if (sameWord(command, "3"))
    {
    if (argc != 4)
	usage();
    thirdPass(argv[2], argv[3]);
    }
else if (sameWord(command, "all"))
    {
    struct tempName tn1,tn2;
    if (argc != 5)
	usage();
    makeTempName(&tn1, "waba", ".1");
    makeTempName(&tn2, "waba", ".2");
    firstPass(argv[2], argv[3], tn1.forCgi);
    secondPass(tn1.forCgi,tn2.forCgi);
    thirdPass(tn2.forCgi,argv[4]);
    remove(tn1.forCgi);
    remove(tn2.forCgi);
    }
else if (sameWord(command, "view"))
    {
    if (argc != 3)
        usage();
    viewWaba(argv[2]);
    }
else
    {
    usage();
    }
}


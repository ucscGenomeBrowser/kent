/* g2gSeqOverlap - make a big .fa file with overlap sequence. */
#include "common.h"
#include "hCommon.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "psl.h"


char *faDirs[] = 
    {
    "/projects/cc/hg/gs.2/fin/fa",
    "/projects/cc/hg/gs.2/draft/fa",
    "/projects/cc/hg/gs.2/predraft/fa",
    };

int faDirCount = 3;

void usage()
/* Describe usage and exit. */
{
errAbort(
 "g2gSeqOverlap - make a big .fa file with overlap sequence.\n"
 "usage:\n"
 "   g2gSeqOverlap pairFile g2g.psl output.fa");
}

struct seqOver
/* Info on (overlap of) single sequence. */
    {
    char *name;			/* Sequence name. */
    int overlap; 		/* Number of overlapping bases. */
    struct psl *pslList;	/* Alignments with this as query. */ 
    };

struct seqPair
/* Info on pair of sequences. */
    {
    struct seqPair *next;
    char *pairName;	  	/* name in form a&b */
    struct seqOver a;		/* Describe alphabetically first seq. */
    struct seqOver b;		/* Describe alphabetically second seq. */
    char *aName;	  /* Alphabetically first sequence. */
    char *bName;	  /* Alphabetically second sequence. */
    };

char *makePairName(char *a, char *b, boolean *retFirstA)
/* Return name in form a&b or b&a (alphabetically sorted) */
{
static char buf[256];
if (strcmp(a, b) <= 0)
    {
    sprintf(buf, "%s%s", a, b);
    *retFirstA = TRUE;
    }
else
    {
    sprintf(buf, "%s%s", b, a);
    *retFirstA = FALSE;
    }
return buf;
}

struct seqPair *readPairList(char *pairFileName, struct hash *cloneHash, struct hash *pairHash)
/* Read in an overlapping pair file. */
{
char *pairName;
char *a, *b;
struct lineFile *lf = lineFileOpen(pairFileName, TRUE);
char *line, *words[4];
int lineSize, wordCount;
struct hashEl *hel;
int overlap;
struct seqPair *pair, *pairList = NULL;
boolean firstA;

while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount != 3)
	errAbort("Expecting three words line %d of %s", lf->lineIx, lf->fileName);
    if (!isdigit(words[1][0]))
	errAbort("Expecting number in second field line %d of %s", lf->lineIx, lf->fileName);
    a = hashStoreName(cloneHash, words[0]);
    overlap = atoi(words[1]);
    b = hashStoreName(cloneHash, words[2]);
    pairName = makePairName(a, b, &firstA);
    if ((hel = hashLookup(pairHash, pairName)) == NULL)
	{
	AllocVar(pair);
	hel = hashAdd(pairHash, pairName, pair);
	pair->pairName = pairName;
	if (firstA)
	    {
	    pair->a.name = a;
	    pair->b.name = b;
	    }
	else
	    {
	    pair->a.name = b;
	    pair->b.name = a;
	    }
	slAddHead(&pairList, pair);
	}
    else
	pair = hel->val;
    if (firstA)
	pair->a.overlap = overlap;
    else
	pair->b.overlap = overlap;
    }
slReverse(&pairList);
lineFileClose(&lf);
return pairList;
}

struct dnaSeq *readHashDna(char *cloneName, struct hash *hash)
/* Read in dna sequence from fa file and add to hash. */
{
int i;
char pathName[512];

for (i=0; i<faDirCount; ++i)
    {
    sprintf(pathName, "%s/%s.fa", faDirs[i], cloneName);
    if (fileExists(pathName))
	{
	struct dnaSeq *seqList = NULL, *seq;
	seqList = faReadAllDna(pathName);
	for (seq = seqList; seq != NULL; seq = seq->next)
	    hashAdd(hash, seq->name, seq);
	return seqList;
	}
    }
errAbort("Couldn't find clone %s", cloneName);
return NULL;
}

void writeOverlaps(FILE *f, struct seqOver *so)
/* Write out info on overlapping part. */
{
struct hash *fragHash = newHash(0);
struct dnaSeq *qSeqList, *qSeq;
char *queryClone = so->name;
struct psl *psl;
char qName[256];
char tName[256];
char faHead[512];

qSeqList = readHashDna(queryClone, fragHash);
slSort(&so->pslList, pslCmpQuery);
for (psl = so->pslList; psl != NULL; psl = psl->next)
    {
    qSeq = hashMustFindVal(fragHash, psl->qName);
    sprintf(qName, "%s.%d.%d.%d", psl->qName, psl->qStart, psl->qEnd, psl->qSize);
    sprintf(tName, "%s.%d.%d.%d", psl->tName, psl->tStart, psl->tEnd, psl->tSize);
    sprintf(faHead, "%s %s", qName, tName);
    faWriteNext(f, faHead, qSeq->dna + psl->qStart, psl->qEnd - psl->qStart);
    }
freeHash(&fragHash);
freeDnaSeqList(&qSeqList);
}

void fillInPsls(char *pslName, struct hash *pairHash)
/* Read in psl file and save overlaps between indicated pairs
 * in hash. */
{
struct lineFile *lf = pslFileOpen(pslName);
struct psl *psl;
char *pairName;
struct seqPair *pair;
struct seqOver *so;
boolean firstA;
char queryClone[128], targetClone[128];
struct hashEl *hel;

while ((psl = pslNext(lf)) != NULL)
    {
    fragToCloneName(psl->qName, queryClone);
    fragToCloneName(psl->tName, targetClone);
    pairName = makePairName(queryClone, targetClone, &firstA);
    if ((pair = hashFindVal(pairHash, pairName)) != NULL)
	{
	so = (firstA ? &pair->a : &pair->b);
	slAddHead(&so->pslList, psl);
	}
    else
	{
	pslFree(&psl);
	}
    }
}

void g2gSeqOverlap(char *pairFileName, char *g2gPsl, char *outName)
/* g2gSeqOverlap - make a big .fa file with overlap sequence. */
{
boolean firstA;
FILE *f = mustOpen(outName, "w");
struct hash *cloneHash = newHash(16);
struct hash *pairHash = newHash(17);
struct seqPair *pair, *pairList;
int dotty = 0;

printf("Reading %s. ", pairFileName);
fflush(stdout);
pairList = readPairList(pairFileName, cloneHash, pairHash);
printf("Got %d pairs\n", slCount(pairList));
printf("Processing %s\n", g2gPsl);
fillInPsls(g2gPsl, pairHash);
printf("Writing overlaps to %s\n", outName);
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    if ((++dotty % 100) == 0)
	{
	printf(".");
	fflush(stdout);
	}
    if (pair->a.overlap >= pair->b.overlap)
	writeOverlaps(f, &pair->a);
    else
	writeOverlaps(f, &pair->b);
    }
printf("\n");
fclose(f);
freeHash(&cloneHash);
freeHash(&pairHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
g2gSeqOverlap(argv[1], argv[2], argv[3]);
return 0;
}




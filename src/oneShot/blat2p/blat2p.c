/* blat2p - blat two proteins. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fa.h"
#include "axt.h"
#include "genoFind.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blat2p - blat two proteins\n"
  "usage:\n"
  "   blat2p a.fa b.fa\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static void countBestMatch(char *chromName, int chromSize, int chromOffset,
	struct ffAli *ali, 
	struct dnaSeq *tSeq, struct hash *t3Hash, struct dnaSeq *qSeq, 
	boolean qIsRc, boolean tIsRc, 
	enum ffStringency stringency, int minMatch, struct gfOutput *out)
/* Save alignment to axtBundle. */
{
int *pCount = out->data;
int i, blockSize, matches = 0;
char *n, *h;

for (; ali != NULL; ali = ali->right)
    {
    n = ali->nStart;
    h = ali->hStart;
    blockSize = ali->nEnd - n;
    for (i=0; i<blockSize; ++i)
        {
	if (n[i] == h[i])
	    ++matches;
	}
    }
if (matches > *pCount)
    *pCount = matches;
}


struct gfOutput *gfOutCountMatch(int *pCount)
/* Get out type that just sets *pCount. */
{
struct gfOutput *out;
AllocVar(out);
out->minGood = 0;
out->qIsProt = TRUE;
out->tIsProt = TRUE;
out->out = countBestMatch;
out->data = pCount;
return out;
}

int score2p(aaSeq *aSeq, aaSeq *bSeq)
/* Return a list of alignments between a and b.
 * May be NULL. */
{
int *pCount = needMem(sizeof(int)), count;
int hitCount;
struct lm *lm = lmInit(0);
struct gfOutput *gvo = gfOutCountMatch(pCount);
struct genoFind *gf = gfIndexSeq(aSeq, 1, 1, 4, 10, NULL, TRUE, FALSE, FALSE);
struct gfClump *clump, *clumpList = gfFindClumps(gf, bSeq, lm, &hitCount);
gfAlignAaClumps(gf, clumpList, bSeq, FALSE, 0, gvo);
gfClumpFreeList(&clumpList);
lmCleanup(&lm);
count = *pCount;
gfOutputFree(&gvo);
return count;
}

void blat2p(char *a, char *b)
/* blat2p - blat two proteins. */
{
aaSeq *aSeq = faReadAa(a);
aaSeq *bSeq = faReadAa(b);
int count = score2p(aSeq, bSeq);
printf("%s and %s match at %d letters\n", a, b, count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
blat2p(argv[1], argv[2]);
return 0;
}

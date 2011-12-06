/* pslCrudeCmp - Crudely compare two psl files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"


/* output files */
FILE *only1Fh = NULL;
FILE *only2Fh = NULL;
FILE *better1Fh = NULL;
FILE *better2Fh = NULL;
FILE *sameFh = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslCrudeCmp - Crudely compare two psl files\n"
  "usage:\n"
  "   pslCrudeCmp 1.psl 2.psl\n"
  "options:\n"
  "   -only1=file - Write PSLs occuring only in the 1.psl to this file.\n"
  "   -only2=file - Write PSLs occuring only in the 2.psl to this file.\n"
  "   -better1=file - Write PSLs that are better in 1.psl to this file.\n"
  "   -better2=file - Write PSLs that are better in 2.psl to this file.\n"
  "   -same=file - Write PSLs that are the same in both files to this file.\n"
  );
}

int scorePsl(struct psl *psl)
/* Return a score for .psl */
{
int score = 1*psl->match + 1*psl->repMatch - 2*psl->misMatch;
int tGap, qGap;
int i, count = psl->blockCount;

for (i=1; i<count; ++i)
    {
    qGap = psl->qStarts[i] - psl->qStarts[i-1] - psl->blockSizes[i-1];
    tGap = psl->tStarts[i] - psl->tStarts[i-1] - psl->blockSizes[i-1];
    if (qGap > 0 && tGap == 0)
       score -= 3+2*log(qGap);
    else if (tGap > 0 && qGap == 0)
       score -= 1+log(tGap);
    else if (qGap > 0 && tGap > 0)
       score -= 3+2*log(max(qGap, tGap));
    }
return score;
}

boolean pslSame(struct psl *a, struct psl *b)
/* Return TRUE if psls are the same. */
{
if (a->blockCount != b->blockCount)
    return FALSE;
else
   {
   int i;
   for (i=0; i<a->blockCount; ++i)
       {
       if (a->blockSizes[i] != b->blockSizes[i] ||
	 a->qStarts[i] != b->qStarts[i] ||
	 a->tStarts[i] != b->tStarts[i])
	    {
	    return FALSE;
	    }
       }
   }
return TRUE;
}

struct queryList
/* List derived from psl's with common query sequence. */
    {
    struct queryList *next;
    char *qName;	/* query name, not allocated here. */
    int score;		/* Score of best psl with this query. */
    struct psl *psl;
    };

void loadPsl(char *fileName, struct queryList **retList, struct hash **retHash)
/* Load up psl file into list/hash. */
{
struct hash *hash = newHash(16);
struct queryList *list = NULL, *el;
struct psl *psl;
struct lineFile *lf = pslFileOpen(fileName);
int score;

while ((psl = pslNext(lf)) != NULL)
    {
    if ((el = hashFindVal(hash, psl->qName)) == NULL)
        {
	AllocVar(el);
	el->score = -10000;
	slAddHead(&list, el);
	hashAddSaveName(hash, psl->qName, el, &el->qName);
	}
    score = scorePsl(psl);
    if (score > el->score) 
	{
    	el->score = score;
	pslFree(&el->psl);
	el->psl = psl;
	}
    else
	pslFree(&psl);
    }
lineFileClose(&lf);
slReverse(&list);
*retList = list;
*retHash = hash;
}

void crossCompare(struct queryList *list, char *listName, struct hash *hash, char *hashName,
                  FILE *onlyFh, FILE *betterFh)
/* Report things in list that aren't on hash, or are better than in hash. */
{
int uniqCount = 0, betterCount = 0;
struct queryList *q, *h;
char *qName;
int count = 0;

verbose(2, "Comparing %s and %s\n", listName, hashName);
for (q = list; q != NULL; q = q->next)
    {
    ++count;
    qName = q->qName;
    if ((h = hashFindVal(hash, qName)) == NULL)
        {
	++uniqCount;
        if (onlyFh != NULL)
            pslTabOut(q->psl, onlyFh);
	verbose(2, " %s only in %s\n", qName, listName);
	}
    else if (!pslSame(q->psl, h->psl) && q->score > h->score)
        {
	++betterCount;
        if (betterFh != NULL)
            pslTabOut(q->psl, betterFh);
	verbose(2, " %s better in %s\n", qName, listName);
	}
    }
printf("%d total in %s\n", count, listName);
printf("%d only in %s (%f%%)\n", uniqCount, listName, 100.0*uniqCount/count);
printf("%d better in %s (%f%%)\n", betterCount, listName, 100.0*betterCount/count);
}

void findIdentities(struct queryList *list, struct hash *aHash, struct hash *bHash)
/* Count identical psls. */
{
int total=0, same=0;
struct queryList *aEl, *bEl;
struct psl *a, *b;
struct queryList *el;
for (el = list; el != NULL; el = el->next)
    {
    char *name = el->qName;
    aEl = hashFindVal(aHash,name);
    bEl = hashFindVal(bHash,name);
    if (aEl != NULL && bEl != NULL)
        {
	++total;
	a = aEl->psl;
	b = bEl->psl;
	if (pslSame(a, b))
            {
	    ++same;
            if (sameFh != NULL)
                pslTabOut(b, sameFh);
            }
	else 
	    verbose(2, " %s is different in two files.\n", name);
	}
    }
printf("%d of %d (%4.2f%%) are the same in both files\n", same, total, 100.0 * same / total);
}

void pslCrudeCmp(char *aFile, char *bFile)
/* pslCrudeCmp - Crudely compare two psl files. */
{
struct queryList *aList, *bList, *q;
struct hash *aHash, *bHash;

loadPsl(aFile, &aList, &aHash);
loadPsl(bFile, &bList, &bHash);
crossCompare(aList, aFile, bHash, bFile, only1Fh, better1Fh);
printf("\n");
crossCompare(bList, bFile, aHash, aFile, only2Fh, better2Fh);
printf("\n");
findIdentities(aList, aHash, bHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
if (optionExists("only1"))
    only1Fh = mustOpen(optionVal("only1", NULL), "w");
if (optionExists("only2"))
    only2Fh = mustOpen(optionVal("only2", NULL), "w");
if (optionExists("better1"))
    better1Fh = mustOpen(optionVal("better1", NULL), "w");
if (optionExists("better2"))
    better2Fh = mustOpen(optionVal("better2", NULL), "w");
if (optionExists("same"))
    sameFh = mustOpen(optionVal("same", NULL), "w");
pslCrudeCmp(argv[1], argv[2]);
return 0;
}

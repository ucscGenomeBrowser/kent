/* pslCrudeCmp - Crudely compare two psl files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "psl.h"

boolean verbose = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslCrudeCmp - Crudely compare two psl files\n"
  "usage:\n"
  "   pslCrudeCmp a.psl b.psl\n"
  "options:\n"
  "   -verbose\n"
  );
}

int scorePsl(struct psl *psl)
/* Return a score for .psl */
{
return psl->match + psl->repMatch - psl->misMatch - psl->qNumInsert - psl->tNumInsert - log(psl->qBaseInsert + psl->tBaseInsert);
}

struct queryList
/* List derived from psl's with common query sequence. */
    {
    struct queryList *next;
    char *qName;	/* query name, not allocated here. */
    int score;		/* Score of best psl with this query. */
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
	slAddHead(&list, el);
	hashAddSaveName(hash, psl->qName, el, &el->qName);
	}
    score = scorePsl(psl);
    if (score > el->score) el->score = score;
    pslFree(&psl);
    }
lineFileClose(&lf);
slReverse(&list);
*retList = list;
*retHash = hash;
}

void crossCompare(struct queryList *list, char *listName, struct hash *hash, char *hashName)
/* Report things in list that aren't on hash, or are better than in hash. */
{
int uniqCount = 0, betterCount = 0;
struct queryList *q, *h;
char *qName;
int count = 0;

if (verbose)
    printf("Comparing %s and %s\n", listName, hashName);
for (q = list; q != NULL; q = q->next)
    {
    ++count;
    qName = q->qName;
    if ((h = hashFindVal(hash, qName)) == NULL)
        {
	++uniqCount;
	if (verbose)
	    printf(" %s only in %s\n", qName, listName);
	}
    else if (q->score > h->score)
        {
	++betterCount;
	if (verbose)
	    printf(" %s better in %s\n", qName, listName);
	}
    }
printf("%d total in %s\n", count, listName);
printf("%d only in %s (%f%%)\n", uniqCount, listName, 100.0*uniqCount/count);
printf("%d better in %s (%f%%)\n", betterCount, listName, 100.0*betterCount/count);
}

void pslCrudeCmp(char *aFile, char *bFile)
/* pslCrudeCmp - Crudely compare two psl files. */
{
struct queryList *aList, *bList, *q;
struct hash *aHash, *bHash;

loadPsl(aFile, &aList, &aHash);
loadPsl(bFile, &bList, &bHash);
crossCompare(aList, aFile, bHash, bFile);
printf("\n");
crossCompare(bList, bFile, aHash, aFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
verbose = cgiBoolean("verbose");
pslCrudeCmp(argv[1], argv[2]);
return 0;
}

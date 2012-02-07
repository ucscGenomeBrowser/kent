/* blastRecipBest - Pick out just the reciprocal best alignments.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "blastTab.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "blastRecipBest - Pick out just the reciprocal best alignments.\n"
  "usage:\n"
  "   blastRecipBest inA.blastTab inB.blastTab outA.blastTab outB.blastTab\n"
  "where:\n"
  "   inA.blastTab is blastall output in the -m 8 format for species A containing\n"
  "                the best alignment for each protein in A vs the B protein set.\n"
  "   inB.blastTab is a similar file containing the best alignment for each B\n"
  "   outA.blastTab is the reciprocal best subset of inA\n"
  "   outB.blastTab is the reciprocal best sebset of inB\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *blastTabLoadBestQuery(char *fileName)
/* Load all blastTabs from file, discarding ones that are not best for a given
 * query. */
{
struct blastTab *bt, *btList = blastTabLoadAll(fileName);
struct hash *hash = hashNew(18);
for (bt = btList; bt != NULL; bt = bt->next)
    {
    struct hashEl *hel = hashLookup(hash, bt->query);
    if (hel == NULL)
        hashAdd(hash, bt->query, bt);
    else
        {
	struct blastTab *oldBt = hel->val;
	if (bt->bitScore > oldBt->bitScore)
	    hel->val = bt;
	}
    }
return hash;
}

struct blastTab *recipBest(struct hash *aHash, struct hash *bHash)
/* Create a list of the elements from aHash that are reciprocal best. */
{
struct blastTab *a, *b, *aList = NULL;
struct hashCookie cookie = hashFirst(aHash);
struct hashEl *el;
while ((el = hashNext(&cookie)) != NULL)
    {
    a = el->val;
    b = hashFindVal(bHash, a->target);
    if (b != NULL)
        {
	if (sameString(b->target, a->query))
	    {
	    slAddHead(&aList, a);
	    }
	}
    }
return aList;
}

void blastTabSaveAll(struct blastTab *list, char *fileName)
/* Save all blastTabs in list to file. */
{
FILE *f = mustOpen(fileName, "w");
struct blastTab *el;
for (el = list; el != NULL; el = el->next)
    blastTabTabOut(el, f);
carefulClose(&f);
}

void blastRecipBest(char *aInFile, char *bInFile, char *aOutFile, char *bOutFile)
/* blastRecipBest - Pick out just the reciprocal best alignments.. */
{
struct hash *aHash = blastTabLoadBestQuery(aInFile);
struct hash *bHash = blastTabLoadBestQuery(bInFile);
struct blastTab *aList = recipBest(aHash, bHash);
struct blastTab *bList = recipBest(bHash, aHash);
blastTabSaveAll(aList, aOutFile);
blastTabSaveAll(bList, bOutFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
blastRecipBest(argv[1], argv[2], argv[3], argv[4]);
return 0;
}

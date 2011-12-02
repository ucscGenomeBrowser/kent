/* testSearch - Set up a search program that does free text indexing and retrieval.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "trix.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "testSearch - Set up a search program that does free text indexing and retrieval.\n"
  "usage:\n"
  "   testSearch file word(s)\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void dumpTsList(struct trixSearchResult *tsList)
/* Dump out contents of tsList to stdout. */
{
struct trixSearchResult *ts;
int maxIx=40, ix=0;
printf("%d match:", slCount(tsList));
for (ts = tsList, ix=0; ts != NULL && ix<maxIx; ts = ts->next, ++ix)
    printf(" %s,%d,%d,%d", ts->itemId, 
    	ts->unorderedSpan, ts->orderedSpan, ts->wordPos);
if (ts != NULL)
    printf(" ...");
printf("\n");
}

void testSearch(char *inFile, int wordCount, char *words[])
/* testSearch - Set up a search program that does free text indexing and retrieval.. */
{
struct trix *trix;
char ixFile[PATH_LEN];
struct trixSearchResult *tsList, *ts;
int i;

safef(ixFile, sizeof(ixFile), "%s.ix", inFile);
for (i=0; i<wordCount; ++i)
    tolowers(words[i]);
trix = trixOpen(ixFile);
tsList = trixSearch(trix, wordCount, words);
dumpTsList(tsList);
trixSearchResultFreeList(&tsList);
trixClose(&trix);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
testSearch(argv[1], argc-2, argv+2);
return 0;
}

/* trixSearch - search trix free text index from command line. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "trix.h"


int maxReturn = 20;
boolean full = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trixSearch - search trix free text index from command line.\n"
  "usage:\n"
  "   trixSearch file word(s)\n"
  "options:\n"
  "   -maxReturn=%d - maximum number of matches to return\n"
  "   -full - return additional search information rather than just ordered list of matching IDs\n"
  , maxReturn
  );
}

static struct optionSpec options[] = {
   {"maxReturn", OPTION_INT},
   {"full", OPTION_BOOLEAN},
   {NULL, 0},
};

void dumpTsList(struct trixSearchResult *tsList)
/* Dump out contents of tsList to stdout. */
{
struct trixSearchResult *ts;
int matchCount = slCount(tsList);
int showCount = min(matchCount, maxReturn);
int ix=0;

if (full)
    {
    printf("#showing %d of %d matches\n", showCount, matchCount);
    printf("#identifier\tspan\tordered\tpos\n");
    }
for (ts = tsList, ix=0; ts != NULL && ix<maxReturn; ts = ts->next, ++ix)
    {
    if (full)
	{
	printf("%s\t%d\t", ts->itemId, ts->unorderedSpan);
	if (ts->orderedSpan == BIGNUM)
	    printf("n/a");
	else
	    printf("%d", ts->orderedSpan);
	printf("\t%d\n", ts->wordPos);
	}
    else
        printf("%s\n", ts->itemId);
    }
}

void trixSearchCommand(char *inFile, int wordCount, char *words[])
/* trixSearchCommand - search trix free text index from command line. */
{
struct trix *trix;
char ixFile[PATH_LEN];
struct trixSearchResult *tsList;
int i;

safef(ixFile, sizeof(ixFile), "%s.ix", inFile);
for (i=0; i<wordCount; ++i)
    tolowers(words[i]);
trix = trixOpen(ixFile);
tsList = trixSearch(trix, wordCount, words, tsmExpand);
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
maxReturn = optionInt("maxReturn", maxReturn);
full = optionExists("full");
trixSearchCommand(argv[1], argc-2, argv+2);
return 0;
}

/* trixSearch - search trix free text index for text from a file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "trix.h"


int maxReturn = 20;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trixMultiSearch - search trix free text index from index for text from a file.\n"
  "usage:\n"
  "   trixMultiSearch file searchFile hitFile\n"
  "\n"
  "Search a trix index on file for words from each line in the search file.  Each\n"
  "line in the search file is split into words and a search is done for records containing\n"
  "the words. This is repeated for each line in the search file as independent\n"
  "queries.  Output are the ids of hits.\n"
  "\n"
  "options:\n"
  "   -maxReturn=%d - maximum number of matches to return\n"
  , maxReturn
  );
}

static struct optionSpec options[] = {
   {"maxReturn", OPTION_INT},
   {NULL, 0},
};

void dumpTsList(struct trixSearchResult *tsList,
                FILE* hitsFh)
/* Dump out contents of tsList to file. */
{
struct trixSearchResult *ts;
int ix=0;

for (ts = tsList, ix=0; ts != NULL && ix<maxReturn; ts = ts->next, ++ix)
    {
    fprintf(hitsFh, "%s\n", ts->itemId);
    }
}

void trixSearchOne(struct trix *trix, FILE *hitFh, int wordCount, char *words[])
/* search for one set of words */
{

for (int i=0; i<wordCount; ++i)
    tolowers(words[i]);
struct trixSearchResult *tsList = trixSearch(trix, wordCount, words, tsmExpand);
dumpTsList(tsList, hitFh);
trixSearchResultFreeList(&tsList);
}

void trixMultiSearch(char *file, char *searchFile, char *hitFile)
/* trixMultiSearch - search trix free text index from index for text from a file. */
{
static const int MAX_WORDS = 128;
char ixFile[PATH_LEN];
safef(ixFile, sizeof(ixFile), "%s.ix", file);
struct trix *trix = trixOpen(ixFile);
struct lineFile *searchLf = lineFileOpen(searchFile, TRUE);
FILE *hitFh = mustOpen(hitFile, "w");

char *words[MAX_WORDS];
int wordCount;
while ((wordCount = lineFileChopNext(searchLf, words, MAX_WORDS)) > 0)
    trixSearchOne(trix, hitFh, wordCount, words);

carefulClose(&hitFh);
lineFileClose(&searchLf);
trixClose(&trix);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
maxReturn = optionInt("maxReturn", maxReturn);
trixMultiSearch(argv[1], argv[2], argv[3]);
return 0;
}

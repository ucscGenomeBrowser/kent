/* wordUse - Make dictionary of all words and count usages. */
#include "common.h"
#include "linefile.h"
#include "hash.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "wordUse - Make dictionary of all words and count usages\n"
  "usage:\n"
  "   wordUse file\n"
  );
}

struct wordCount
/* Count number of uses of word. */
    {
    struct wordCount *next;	/* Next in list. */
    char *word;			/* Allocated in hash. */
    int count;			/* Number of times used. */
    };

int wordCountCmpCount(const void *va, const void *vb)
/* Compare two wordCounts in decreasing order of count. */
{
const struct wordCount *a = *((struct wordCount **)va);
const struct wordCount *b = *((struct wordCount **)vb);
return b->count - a->count;
}

void wordUse(char *fileName)
/* wordUse - Make dictionary of all words and count usages, report top ten. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
struct hash *hash = newHash(18);
struct wordCount *wcList = NULL, *wc;
int i;
while (lineFileNext(lf, &line, NULL))
    {
    while ((word = nextWord(&line)) != NULL)
	{
	wc = hashFindVal(hash, word);
	if (wc == NULL)
	    {
	    AllocVar(wc);
	    hashAddSaveName(hash, word, wc, &wc->word);
	    slAddHead(&wcList, wc);
	    }
	wc->count += 1;
	}
    }
slSort(&wcList, wordCountCmpCount);
for (i=0, wc=wcList; i<10 && wc != NULL; wc = wc->next, ++i)
    printf("%s\t%d\n", wc->word, wc->count);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
wordUse(argv[1]);
return 0;
}

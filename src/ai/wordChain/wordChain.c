/* wordChain - Create Markov chain of words. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"

int maxChainSize = 3;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wordChain - Create Markov chain of words\n"
  "usage:\n"
  "   wordChain in.txt\n"
  "options:\n"
  "   -size=N - Set max chain size, default %d\n"
  "   -chain=fileName - Write out word chain to file\n"
  "   -nonsense=fileName - Write out predicted nonsense to file\n"
  , maxChainSize
  );
}

static struct optionSpec options[] = {
   {"size", OPTION_INT},
   {"nonsense", OPTION_STRING},
   {"chain", OPTION_STRING},
   {NULL, 0},
};

struct wordTree
/* A tree of words. */
    {
    struct wordTree *next;	/* Next in somebody's following list. */
    struct wordTree *following;	/* List of words that we've seen follow us. */
    char *word;			/* The word itself including comma, period etc. */
    int useCount;		/* Number of times word used. */
    };

struct wordTree *wordTreeNew(char *word)
/* Create and return new wordTree element. */
{
struct wordTree *wt;
AllocVar(wt);
wt->word = cloneString(word);
return wt;
}

int wordTreeCmpWord(const void *va, const void *vb)
/* Compare two wordTree. */
{
const struct wordTree *a = *((struct wordTree **)va);
const struct wordTree *b = *((struct wordTree **)vb);
return strcmp(a->word, b->word);
}


struct wordTree *wordTreeFind(struct wordTree *list, char *word)
/* Return matching element in list or NULL if not found. */
{
struct wordTree *wt;
for (wt = list; wt != NULL; wt = wt->next)
    {
    if (sameString(wt->word, word))
        return wt;
    }
return NULL;
}

struct wordTree *wordTreeAddFollowing(struct wordTree *wt, char *word)
/* Make word follow wt in tree.  If word already exists among followers
 * return it and bump use count.  Otherwise create new one. */
{
struct wordTree *w = wordTreeFind(wt->following, word);
if (w == NULL)
    {
    w = wordTreeNew(word);
    slAddHead(&wt->following, w);
    }
w->useCount += 1;
return w;
}

void addChainToTree(struct wordTree *wt, struct dlList *chain)
/* Add chain of words to tree. */
{
struct dlNode *node;
wt->useCount += 1;
for (node = chain->head; !dlEnd(node); node = node->next)
    {
    char *word = node->val;
    wt = wordTreeAddFollowing(wt, word);
    }
}

void wordTreeDump(int level, struct wordTree *wt, FILE *f)
/* Write out wordTree to file. */
{
static char *words[64];
int i;
assert(level < ArraySize(words));
words[level] = wt->word;
for (i=1; i<=level; ++i)
    fprintf(f, "%s ", words[i]);
fprintf(f, "%d %d\n", wt->useCount, slCount(wt->following));
for (wt = wt->following; wt != NULL; wt = wt->next)
    wordTreeDump(level+1, wt, f);
}

void wordTreeSort(struct wordTree *wt)
/* Recursively sort tree. */
{
slSort(&wt->following, wordTreeCmpWord);
for (wt = wt->following; wt != NULL; wt = wt->next)
    wordTreeSort(wt);
}

char *pickRandomWord(struct wordTree *list)
/* Pick word from list randomly, but so that words more
 * commonly seen are picked more often. */
{
int totalUses = 0, curUses = 0, useThreshold;
struct wordTree *wt;
for (wt = list; wt != NULL; wt = wt->next)
    totalUses += wt->useCount;
useThreshold = rand()%totalUses + 1;
for (wt = list; wt != NULL; wt = wt->next)
    {
    curUses += wt->useCount;
    if (curUses >= useThreshold)
	break;
    }
assert(wt != NULL);
return wt->word;
}

char *predictNext(struct wordTree *wt, struct dlList *recent)
/* Predict next word given list of recent words and wordTree. */
{
struct dlNode *node;
for (node = recent->head; !dlEnd(node); node = node->next)
    {
    char *word = node->val;
    wt = wordTreeFind(wt->following, word);
    if (wt == NULL)
        errAbort("%s isn't a follower of %s\n", word, wt->word);
    }
if (wt->following == NULL)
    return NULL;
else
    return pickRandomWord(wt->following);
}

static void wordTreeMakeNonsense(struct wordTree *wt, int maxSize, char *firstWord, 
	int wordCount, FILE *f)
/* Go spew out a bunch of words according to probabilities in tree. */
{
struct dlList *ll = dlListNew();
int listSize = 0;
int i;

for (;;)
    {
    struct dlNode *node;
    char *word;

    /* Get next predicted word. */
    if (listSize == 0)
        {
	AllocVar(node);
	++listSize;
	word = firstWord;
	}
    else if (listSize >= maxSize)
	{
	node = dlPopHead(ll);
	word = predictNext(wt, ll);
	}
    else
	{
	word = predictNext(wt, ll);
	AllocVar(node);
	++listSize;
	}
    node->val = word;
    dlAddTail(ll, node);

    if (word == NULL)
         break;

    /* Output last word in list. */
	{
	node = ll->tail;
	word = node->val;
	fprintf(f, "%s", word);
	if (word[strlen(word)-1] == '.')
	    fprintf(f, "\n");
	else
	    fprintf(f, " ");
	}
    }
dlListFree(&ll);
}

void wordChain(char *inFile, int maxSize)
/* wordChain - Create Markov chain of words. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f;
char *line, *word, *firstWord = NULL;
struct dlList *ll = dlListNew();
struct dlNode *node;
int llSize = 0;
struct wordTree *wt = wordTreeNew("");
int wordCount = 0;

while (lineFileNext(lf, &line, NULL))
    {
    while ((word = nextWord(&line)) != NULL)
	{
	if (wordCount == 0)
	    firstWord = cloneString(word);

	if (llSize < maxSize)
	    {
	    dlAddValTail(ll, cloneString(word));
	    ++llSize;
	    if (llSize == maxSize)
		addChainToTree(wt, ll);
	    }
	else
	    {
	    node = dlPopHead(ll);
	    freeMem(node->val);
	    node->val = cloneString(word);
	    dlAddTail(ll, node);
	    addChainToTree(wt, ll);
	    }
	++wordCount;
	}
    }
if (llSize < maxSize)
    addChainToTree(wt, ll);
while ((node = dlPopHead(ll)) != NULL)
    {
    addChainToTree(wt, ll);
    freeMem(node->val);
    freeMem(node);
    }
dlListFree(&ll);
lineFileClose(&lf);
wordTreeSort(wt);

if (optionExists("chain"))
    {
    char *fileName = optionVal("chain", NULL);
    f = mustOpen(fileName, "w");
    wordTreeDump(0, wt, f);
    carefulClose(&f);
    }

if (optionExists("nonsense"))
    {
    char *fileName = optionVal("nonsense", NULL);
    FILE *f = mustOpen(fileName, "w");
    wordTreeMakeNonsense(wt, maxSize, firstWord, wordCount, f);
    carefulClose(&f);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
maxChainSize = optionInt("size", maxChainSize);
wordChain(argv[1], maxChainSize);
return 0;
}

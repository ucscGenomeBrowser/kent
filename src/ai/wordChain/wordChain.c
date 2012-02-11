/* wordChain - Create Markov chain of words. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "dlist.h"
#include "rbTree.h"

int maxChainSize = 3;
int maxNonsenseSize = 10000;
int minUse = 1;
boolean lower = FALSE;
boolean unpunc = FALSE;
boolean fullOnly = FALSE;

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
  "   -maxNonsenseSize=N - Keep nonsense output to this many words.\n"
  "   -lower - Lowercase all words\n"
  "   -unpunc - Strip punctuation\n"
  "   -fullOnly - Only output chains of size\n"
  "   -minUse=N - Set minimum use in output chain, default %d\n"
  , maxChainSize, minUse
  );
}

static struct optionSpec options[] = {
   {"size", OPTION_INT},
   {"minUse", OPTION_INT},
   {"nonsense", OPTION_STRING},
   {"chain", OPTION_STRING},
   {"lower", OPTION_BOOLEAN},
   {"unpunc", OPTION_BOOLEAN},
   {"fullOnly", OPTION_BOOLEAN},
   {"maxNonsenseSize", OPTION_INT},
   {NULL, 0},
};

struct wordTree
/* A tree of words. */
    {
    struct rbTree *following;	/* Binary tree of words that follow us. */
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

int wordTreeCmpWord(void *va, void *vb)
/* Compare two wordTree. */
{
struct wordTree *a = va, *b = vb;
return strcmp(a->word, b->word);
}


struct wordTree *wordTreeAddFollowing(struct wordTree *wt, char *word, 
	struct lm *lm, struct rbTreeNode **stack)
/* Make word follow wt in tree.  If word already exists among followers
 * return it and bump use count.  Otherwise create new one. */
{
struct wordTree *w;
if (wt->following == NULL)
    {
    wt->following = rbTreeNewDetailed(wordTreeCmpWord, lm, stack);
    w = NULL;
    }
else
    {
    struct wordTree key;
    key.word = word;
    w = rbTreeFind(wt->following, &key);
    }
if (w == NULL)
    {
    w = wordTreeNew(word);
    rbTreeAdd(wt->following, w);
    }
w->useCount += 1;
return w;
}

void addChainToTree(struct wordTree *wt, struct dlList *chain, 
	struct lm *lm, struct rbTreeNode **stack)
/* Add chain of words to tree. */
{
struct dlNode *node;
wt->useCount += 1;
for (node = chain->head; !dlEnd(node); node = node->next)
    {
    char *word = node->val;
    verbose(2, "  %s\n", word);
    wt = wordTreeAddFollowing(wt, word, lm, stack);
    }
}

void wordTreeDump(int level, struct wordTree *wt, FILE *f)
/* Write out wordTree to file. */
{
static char *words[64];
struct slRef *list, *ref;
int i;
assert(level < ArraySize(words));

words[level] = wt->word;
if (wt->useCount >= minUse)
    {
    if (!fullOnly || level == maxChainSize)
	{
	fprintf(f, "%d\t", wt->useCount);
	for (i=1; i<=level; ++i)
	    fprintf(f, "%s ", words[i]);
	fprintf(f, "\n");
	}
    }
if (wt->following != NULL)
    {
    list = rbTreeItems(wt->following);
    for (ref = list; ref != NULL; ref = ref->next)
        wordTreeDump(level+1, ref->val, f);
    slFreeList(&list);
    }
}

int totalUses = 0;
int curUses = 0;
int useThreshold = 0;
char *pickedWord;

void addUse(void *v)
/* Add up to total uses. */
{
struct wordTree *wt = v;
totalUses += wt->useCount;
}

void pickIfInThreshold(void *v)
/* See if inside threshold, and if so store it in pickedWord. */
{
struct wordTree *wt = v;
int top = curUses + wt->useCount;
if (curUses <= useThreshold && useThreshold < top)
    pickedWord = wt->word;
curUses = top;
}

char *pickRandomWord(struct rbTree *rbTree)
/* Pick word from list randomly, but so that words more
 * commonly seen are picked more often. */
{
pickedWord = NULL;
curUses = 0;
totalUses = 0;
rbTreeTraverse(rbTree, addUse);
useThreshold = rand() % totalUses;
rbTreeTraverse(rbTree, pickIfInThreshold);
assert(pickedWord != NULL);
return pickedWord;
}

char *predictNext(struct wordTree *wt, struct dlList *recent)
/* Predict next word given list of recent words and wordTree. */
{
struct dlNode *node;
for (node = recent->head; !dlEnd(node); node = node->next)
    {
    char *word = node->val;
    struct wordTree key;
    key.word = word;
    wt = rbTreeFind(wt->following, &key);
    if (wt == NULL)
        errAbort("%s isn't a follower of %s\n", word, wt->word);
    }
char *result = NULL;
if (wt->following != NULL)
    result = pickRandomWord(wt->following);
return result;
}

static void wordTreeMakeNonsense(struct wordTree *wt, int maxSize, char *firstWord, 
	int maxOutputWords, FILE *f)
/* Go spew out a bunch of words according to probabilities in tree. */
{
struct dlList *ll = dlListNew();
int listSize = 0;
int outputWords = 0;

for (;;)
    {
    if (++outputWords > maxOutputWords)
        break;
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
struct lm *lm = lmInit(0);
struct rbTreeNode **stack;

lmAllocArray(lm, stack, 256);
while (lineFileNext(lf, &line, NULL))
    {
    if (lower)
        tolowers(line);
    while ((word = nextWord(&line)) != NULL)
	{
	if (unpunc)
	    {
	    stripChar(word, ',');
	    stripChar(word, '.');
	    stripChar(word, ';');
	    stripChar(word, '-');
	    stripChar(word, '"');
	    stripChar(word, '?');
	    stripChar(word, '!');
	    stripChar(word, '(');
	    stripChar(word, ')');
	    if (word[0] == 0)
	         continue;
	    }
	verbose(2, "%s\n", word);
	if (wordCount == 0)
	    firstWord = cloneString(word);

	if (llSize < maxSize)
	    {
	    dlAddValTail(ll, cloneString(word));
	    ++llSize;
	    if (llSize == maxSize)
		addChainToTree(wt, ll, lm, stack);
	    }
	else
	    {
	    node = dlPopHead(ll);
	    freeMem(node->val);
	    node->val = cloneString(word);
	    dlAddTail(ll, node);
	    addChainToTree(wt, ll, lm, stack);
	    }
	++wordCount;
	}
    }
if (llSize < maxSize)
    addChainToTree(wt, ll, lm, stack);
while ((node = dlPopHead(ll)) != NULL)
    {
    addChainToTree(wt, ll, lm, stack);
    freeMem(node->val);
    freeMem(node);
    }
dlListFree(&ll);
lineFileClose(&lf);

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
    int maxSize = min(wordCount, maxNonsenseSize);
    wordTreeMakeNonsense(wt, maxChainSize, firstWord, maxSize, f);
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
minUse = optionInt("minUse", minUse);
maxNonsenseSize = optionInt("maxNonsenseSize", maxNonsenseSize);
lower = optionExists("lower");
unpunc = optionExists("unpunc");
fullOnly = optionExists("fullOnly");
wordChain(argv[1], maxChainSize);
return 0;
}

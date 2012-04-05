/* wordChain - Create Markov chain of words. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "dlist.h"
#include "rbTree.h"

/* Global vars - all of which can be set by command line options. */
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
  "alphaChain - create a linear projection of alpha satellite arrays using the probablistic model\n"
  "of HuRef satellite graphs\n"
  "usage:\n"
  "   alphaChain alphaMonFile.fa significant_output.txt\n"
  "options:\n"
  "   -size=N - Set max chain size, default %d\n"
  "   -chain=fileName - Write out word chain to file\n"
  "   -maxNonsenseSize=N - Keep nonsense output to this many words.\n"
  "   -fullOnly - Only output chains of size\n"
  "   -minUse=N - Set minimum use in output chain, default %d\n"
  , maxChainSize, minUse
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"size", OPTION_INT},
   {"minUse", OPTION_INT},
   {"chain", OPTION_STRING},
   {"fullOnly", OPTION_BOOLEAN},
   {"maxNonsenseSize", OPTION_INT},
   {NULL, 0},
};

/* The wordTree structure below is the central data structure for this program.  It is
 * used to build up a tree that contains all observed N-word-long sequences observed in
 * the text, where N corresponds to the "size" command line option which defaults to 3,
 * an option that in turn is stored in the maxChainSize variable.  At this chain size the
 * text 
 *     this is the black dog and the black cat
 * would have the chains 
 *     this is the 
 *     is the black
 *     the black dog
 *     black dog and
 *     dog and the
 *     and the black
 *     the black cat
 * and turn into the tree
 *     this
 *        is
 *           the
 *     is
 *        the
 *           black
 *     the
 *        black
 *           dog
 *           cat
 *     black
 *        dog
 *           and
 *     dog
 *        and
 *           the
 *     and
 *        the
 *           black
 * Note how the tree is able to compress the two chains "the black dog" and "the black cat."
 *
 * A node in the tree can have as many children as it needs to at each node.  The depth of
 * the tree is the same as the chain size, by default 3. At each node in the tree you get
 * a word, and a list of all words that are observed in the text to follow that word.
 *
 * There are special cases in the code so that the first and last words in the text get included 
 * as much as possible in the tree. 
 *
 * Once the program has build up the wordTree, it can output it in a couple of fashions. */

struct wordTree
/* A node in a tree of words.  The head of the tree is a node with word value the empty string. */
    {
    struct rbTree *following;	/* Contains words (as struct wordTree) that follow us. */
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
struct wordTree *w;   /* Points to following element if any */
if (wt->following == NULL)
    {
    /* Allocate new if you've never seen it before. */
    wt->following = rbTreeNewDetailed(wordTreeCmpWord, lm, stack);
    w = NULL;
    }
else
    {
    /* Find word in existing tree */
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

char *predictNextFromAllPredecessors(struct wordTree *wt, struct dlNode *list)
/* Predict next word given tree and recently used word list.  If tree doesn't
 * have statistics for what comes next given the words in list, then it returns
 * NULL. */
{
struct dlNode *node;
for (node = list; !dlEnd(node); node = node->next)
    {
    char *word = node->val;
    struct wordTree key;
    key.word = word;
    wt = rbTreeFind(wt->following, &key);
    if (wt == NULL || wt->following == NULL)
        break;
    }
char *result = NULL;
if (wt != NULL && wt->following != NULL)
    result = pickRandomWord(wt->following);
return result;
}

char *predictNext(struct wordTree *wt, struct dlList *recent)
/* Predict next word given tree and recently used word list.  Will use all words in
 * recent list if can,  but if there is not data in tree, will back off, and use
 * progressively less previous words until ultimately it just picks a random
 * word. */
{
struct dlNode *node;
for (node = recent->head; !dlEnd(node); node = node->next)
    {
    char *result = predictNextFromAllPredecessors(wt, node);
    if (result != NULL)
        return result;
    }
return pickRandomWord(wt->following); 
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

    fprintf(f, "%s\n", word);
    }
dlListFree(&ll);
}

struct wordTree *wordTreeForChainsInFile(char *fileName, int chainSize, struct lm *lm)
/* Return a wordTree of all chains-of-words of length chainSize seen in file. 
 * Allocate the structure in local memory pool lm. */ 
{
/* Stuff for processing file a line at a time. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;

/* We'll build up the tree starting with an empty root node. */
struct wordTree *wt = wordTreeNew("");	

/* Save time/space by sharing stack between all "following" rbTrees. */
struct rbTreeNode **stack;	
lmAllocArray(lm, stack, 256);

/* Loop through each line of input file, lowercasing the whole line, and then
 * looping through each word of line, stripping out special chars, and finally
 * processing each word. */
while (lineFileNext(lf, &line, NULL))
    {
      /* KEH NOTES: change 3/14/12: before process beginning and end of a file, now happens at the beginning and end of each line */
      /* We'll keep a chain of three or so words in a doubly linked list. */
      struct dlNode *node;
      struct dlList *chain = dlListNew();
      int curSize = 0;
      int wordCount = 0;

      /* skipping the first word which is the read id */
      word = nextWord(&line);

    while ((word = nextWord(&line)) != NULL)
	{
	/* We come to this point in the code for each word in the file. 
	 * Here we want to maintain a chain of sequential words up to
	 * chainSize long.  We do this with a doubly-linked list structure.
	 * For the first few words in the file we'll just build up the list,
	 * only adding it to the tree when we finally do get to the desired
	 * chain size.  Once past the initial section of the file we'll be
	 * getting rid of the first link in the chain as well as adding a new
	 * last link in the chain with each new word we see. */



	if (curSize < chainSize)
	    {
	    dlAddValTail(chain, cloneString(word));
	    ++curSize;
	    if (curSize == chainSize)
		addChainToTree(wt, chain, lm, stack);
	    }
	else
	    {
	    /* Reuse doubly-linked-list node, but give it a new value, as we move
	     * it from head to tail of list. */
	    node = dlPopHead(chain);
	    freeMem(node->val);
	    node->val = cloneString(word);
	    dlAddTail(chain, node);
	    addChainToTree(wt, chain, lm, stack);
	    }
	++wordCount;
	}
    /* Handle last few words in line, where can't make a chain of full size.  Also handles       
     * lines that have fewer than chain size words. */
    if (curSize < chainSize)
      addChainToTree(wt, chain, lm, stack);
    while ((node = dlPopHead(chain)) != NULL)
      {
	addChainToTree(wt, chain, lm, stack);
	freeMem(node->val);
	freeMem(node);
      }
    dlListFree(&chain);
    }

lineFileClose(&lf);
return wt;
}

void alphaChain(char *inFile, char *outFile)
/* alphaChain - Create Markov chain of words and optionally output chain in two formats. */
{
struct lm *lm = lmInit(0);
struct wordTree *wt = wordTreeForChainsInFile(inFile, maxChainSize, lm);

if (optionExists("chain"))
    {
    char *fileName = optionVal("chain", NULL);
    FILE *f = mustOpen(fileName, "w");
    wordTreeDump(0, wt, f);
    carefulClose(&f);
    }


 FILE *f = mustOpen(outFile, "w");
 int maxSize = min(wt->useCount, maxNonsenseSize);

 /* KEH NOTES: controls how many words we emit */

 wordTreeMakeNonsense(wt, maxChainSize, pickRandomWord(wt->following), maxSize, f);
 carefulClose(&f);
    

lmCleanup(&lm);	// Not really needed since we're just going to exit.
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
maxChainSize = optionInt("size", maxChainSize);
minUse = optionInt("minUse", minUse);
maxNonsenseSize = optionInt("maxNonsenseSize", maxNonsenseSize);
fullOnly = optionExists("fullOnly");
alphaChain(argv[1], argv[2]);
return 0;
}

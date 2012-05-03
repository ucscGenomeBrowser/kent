/* alphaChain - Predicts faux centromere sequences using a probablistic model. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"

/* Global vars - all of which can be set by command line options. */
int maxChainSize = 3;
int outSize = 10000;
int minUse = 1;
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
  "   -outSize=N - Output this many words.\n"
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
   {"outSize", OPTION_INT},
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
 * Once the program has build up the wordTree, it can output it in a couple of fashions. */

struct wordTree
/* A node in a tree of words.  The head of the tree is a node with word value the empty string. */
    {
    struct wordTree *next;	/* Next sibling */
    struct wordTree *children;	/* Children in list. */
    struct wordTree *parent;    /* Parent of this node or NULL for root. */
    char *word;			/* The word itself including comma, period etc. */
    int useCount;		/* Number of times word used in input. */
    int outTarget;              /* Number of times want to output word. */
    int outCount;	/* Number of times output. */
    double normVal;             /* value to place the normalization value */    
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
/* Compare two wordTree for slSort. */
{
const struct wordTree *a = *((struct wordTree **)va);
const struct wordTree *b = *((struct wordTree **)vb);
return strcmp(a->word, b->word);
}

struct wordTree *wordTreeFindInList(struct wordTree *list, char *word)
/* Return wordTree element in list that has given word, or NULL if none. */
{
struct wordTree *wt;
for (wt = list; wt != NULL; wt = wt->next)
    if (sameString(wt->word, word))
        break;
return wt;
}

struct wordTree *wordTreeAddFollowing(struct wordTree *wt, char *word)
/* Make word follow wt in tree.  If word already exists among followers
 * return it and bump use count.  Otherwise create new one. */
{
struct wordTree *child = wordTreeFindInList(wt->children, word);
if (child == NULL)
    {
    child = wordTreeNew(word);
    child->parent = wt;
    slAddHead(&wt->children, child);
    }
child->useCount += 1;
return child;
}

int wordTreeSumUseCounts(struct wordTree *list)
/* Sum up useCounts in list */
{
int total = 0;
struct wordTree *wt;
for (wt = list; wt != NULL; wt = wt->next)
    total += wt->useCount;
return total;
}

int wordTreeChildrenUseCount(struct wordTree *wt)
/* Return sum of useCounts of all children */
{
return wordTreeSumUseCounts(wt->children);
}

int wordTreeSumOutTargets(struct wordTree *list)
/* Sum up useCounts in list */
{
int total = 0;
struct wordTree *wt;
for (wt = list; wt != NULL; wt = wt->next)
    total += wt->outTarget;
return total;
}

void addChainToTree(struct wordTree *wt, struct dlList *chain)
/* Add chain of words to tree. */
{
struct dlNode *node;
wt->useCount += 1;
for (node = chain->head; !dlEnd(node); node = node->next)
    {
    char *word = node->val;
    verbose(2, "  %s\n", word);
    wt = wordTreeAddFollowing(wt, word);
    }
}

void wordTreeNormalize(struct wordTree *wt, double outTarget, double normVal)
/* Recursively set wt->normVal  and wt->outTarget so each branch gets its share */
{
wt->normVal = normVal;
wt->outTarget = outTarget;
int totalChildUses = wordTreeChildrenUseCount(wt);
struct wordTree *child;
for (child = wt->children; child != NULL; child = child->next)
    {
    double childRatio = (double)child->useCount / totalChildUses;
    wordTreeNormalize(child, childRatio*outTarget, childRatio*normVal);
    }
}

void wordTreeDump(int level, struct wordTree *wt, FILE *f)
/* Write out wordTree to file. */
{
static char *words[64];
int i;
assert(level < ArraySize(words));

words[level] = wt->word;
if (wt->useCount >= minUse)
    {
    if (!fullOnly || level == maxChainSize)
	{
	fprintf(f, "%d\t%d\t%d\t%d\t%f\t", level, wt->useCount, wt->outTarget, wt->outCount, wt->normVal);
	
	for (i=1; i<=level; ++i)
            {
            spaceOut(f, level*2);
	    fprintf(f, "%s ", words[i]);
            }
	fprintf(f, "\n");
	}
    }
struct wordTree *child;
for (child = wt->children; child != NULL; child = child->next)
    wordTreeDump(level+1, child, f);
}

int totUseZeroCount;  // debugging aid

struct wordTree *pickRandomOnOutTarget(struct wordTree *list)
/* Pick word from list randomly, but so that words more
 * commonly seen are picked more often. */
{
struct wordTree *picked = NULL;

/* Figure out total number of outputs left, and a random number between 0 and that total. */
int total = wordTreeSumOutTargets(list);
if (total > 0)
    {
    int threshold = rand() % total; 

    /* Loop through list returning selection corresponding to random threshold. */
    int binStart = 0;
    struct wordTree *wt;
    for (wt = list; wt != NULL; wt = wt->next)
	{
	int size = wt->outTarget;
	int binEnd = binStart + size;
	if (threshold < binEnd)
	    {
	    picked = wt;
	    break;
	    }
	binStart = binEnd;
	}
    }

/* If did not find anything, that's ok. It can happen on legitimate input due to unevenness
 * of read coverage.  In this case we just return an arbitrary element. */
if (picked == NULL)
    {
    picked = list;
    totUseZeroCount += 1;
    }
return picked;
}

struct wordTree *predictNextFromAllPredecessors(struct wordTree *wt, struct dlNode *list)
/* Predict next word given tree and recently used word list.  If tree doesn't
 * have statistics for what comes next given the words in list, then it returns
 * NULL. */
{
struct dlNode *node;
for (node = list; !dlEnd(node); node = node->next)
    {
    char *word = node->val;
    wt = wordTreeFindInList(wt->children, word);
    if (wt == NULL || wt->children == NULL)
        break;
    }
struct wordTree *result = NULL;
if (wt != NULL && wt->children != NULL)
    result = pickRandomOnOutTarget(wt->children);
return result;
}

struct wordTree *predictNext(struct wordTree *wt, struct dlList *recent)
/* Predict next word given tree and recently used word list.  Will use all words in
 * recent list if can,  but if there is not data in tree, will back off, and use
 * progressively less previous words until ultimately it just picks a random
 * word. */
{
struct dlNode *node;
for (node = recent->head; !dlEnd(node); node = node->next)
    {
    struct wordTree *result = predictNextFromAllPredecessors(wt, node);
    if (result != NULL)
        return result;
    }
return pickRandomOnOutTarget(wt->children); 
}

void decrementOutputCounts(struct wordTree *wt)
/* Decrement output count of self and parents. */
{
while (wt != NULL)
    {
    wt->outTarget -= 1;
    wt->outCount += 1;
    wt = wt->parent;
    }
}

static void wordTreeGenerateFaux(struct wordTree *wt, int maxSize, struct wordTree *firstWord, 
	int maxOutputWords, char *fileName)
/* Go spew out a bunch of words according to probabilities in tree. */
{
FILE *f = mustOpen(fileName, "w");
struct dlList *ll = dlListNew();
int listSize = 0;
int outputWords = 0;

for (;;)
    {
    if (++outputWords > maxOutputWords)
        break;
    struct dlNode *node;
    struct wordTree *picked;

    /* Get next predicted word. */
    if (listSize == 0)
        {
	AllocVar(node);
	++listSize;
	picked = firstWord;
	}
    else if (listSize >= maxSize)
	{
	node = dlPopHead(ll);
	picked = predictNext(wt, ll);
//         decrementOutputCounts(picked);   // ugly placement?
	}
    else
	{
	picked = predictNext(wt, ll);
	AllocVar(node);
	++listSize;
	}

    if (picked == NULL)
         break;


    /* Add word from whatever level we fetched back to our chain of up to maxChainSize. */
    node->val = picked->word;
    dlAddTail(ll, node);

    fprintf(f, "%s\n", picked->word);

    decrementOutputCounts(picked);
    }
dlListFree(&ll);
carefulClose(&f);
}

static void wordTreeSort(struct wordTree *wt)
/* Sort all children lists in tree. */
{
slSort(&wt->children, wordTreeCmpWord);
struct wordTree *child;
for (child = wt->children; child != NULL; child = child->next)
    wordTreeSort(child);
}

struct wordTree *wordTreeForChainsInFile(char *fileName, int chainSize)
/* Return a wordTree of all chains-of-words of length chainSize seen in file. 
 * Allocate the structure in local memory pool lm. */ 
{
/* Stuff for processing file a line at a time. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;

/* We'll build up the tree starting with an empty root node. */
struct wordTree *wt = wordTreeNew("");	

/* Loop through each line of file, treating it as a separate read. There's 
 * special cases at the beginning and end of line, and for short lines.  In the
 * main case we'll be maintaining a chain (doubly linked list) of maxChainSize words, 
 * popping off one word from the start, and adding one word to the end for each
 * new word we encounter. This list is added to the tree each iteration. */
while (lineFileNext(lf, &line, NULL))
    {
    /* We'll keep a chain of three or so words in a doubly linked list. */
    struct dlNode *node;
    struct dlList *chain = dlListNew();
    int curSize = 0;
    int wordCount = 0;

    /* skipping the first word which is the read id */
    word = nextWord(&line);

    while ((word = nextWord(&line)) != NULL)
	{
	 /* For the first few words in the file after ID, we'll just build up the chain,
	 * only adding it to the tree when we finally do get to the desired
	 * chain size.  Once past the initial section of the file we'll be
	 * getting rid of the first link in the chain as well as adding a new
	 * last link in the chain with each new word we see. */
	if (curSize < chainSize)
	    {
	    dlAddValTail(chain, cloneString(word));
	    ++curSize;
	    if (curSize == chainSize)
		addChainToTree(wt, chain);
	    }
	else
	    {
	    /* Reuse doubly-linked-list node, but give it a new value, as we move
	     * it from head to tail of list. */
	    node = dlPopHead(chain);
	    freeMem(node->val);
	    node->val = cloneString(word);
	    dlAddTail(chain, node);
	    addChainToTree(wt, chain);
	    }
	++wordCount;
	}
    /* Handle last few words in line, where can't make a chain of full size.  Also handles       
    * lines that have fewer than chain size words. */
    if (curSize < chainSize)
 	addChainToTree(wt, chain);
    while ((node = dlPopHead(chain)) != NULL)
	{
	if (!dlEmpty(chain))
	    addChainToTree(wt, chain);
	freeMem(node->val);
	freeMem(node);
	}
    dlListFree(&chain);
    }
lineFileClose(&lf);

wordTreeSort(wt);   // ugly debug

return wt;
}

void wordTreeWrite(struct wordTree *wt, char *fileName)
/* Write out tree to file */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "#level\tuseCount\toutTarget\toutCount\tnormVal\tmonomers\n");
wordTreeDump(0, wt, f);
carefulClose(&f);
}

void alphaChain(char *inFile, char *outFile)
/* alphaChain - Create Markov chain of words and optionally output chain in two formats. */
{
struct wordTree *wt = wordTreeForChainsInFile(inFile, maxChainSize);
wordTreeNormalize(wt, outSize, 1.0);

if (optionExists("chain"))
    {
    char *fileName = optionVal("chain", NULL);
    wordTreeWrite(wt, fileName);
    }


wordTreeGenerateFaux(wt, maxChainSize, pickRandomOnOutTarget(wt->children), outSize, outFile);

uglyf("totUseZeroCount = %d\n", totUseZeroCount);
wordTreeWrite(wt, "ugly.chain");
}

int main(int argc, char *argv[])
/* Process command line. */
{
#ifdef SOON
/* Seed random number generator with time, so it doesn't always generate same sequence of #s */
srand( (unsigned)time(0) );
#endif /* SOON */
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
maxChainSize = optionInt("size", maxChainSize);
minUse = optionInt("minUse", minUse);
outSize = optionInt("outSize", outSize);
fullOnly = optionExists("fullOnly");
alphaChain(argv[1], argv[2]);
return 0;
}

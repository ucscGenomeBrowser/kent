/* alphaChain - Predicts faux centromere sequences using a probablistic model. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dlist.h"

/* Global vars - all of which can be set by command line options. */
int maxChainSize = 3;
int outSize = 10000;
boolean fullOnly = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "alphaChain - create a linear projection of alpha satellite arrays using the probablistic model\n"
  "of HuRef satellite graphs\n"
  "usage:\n"
  "   alphaChain alphaMonFile.txt monomerOrder.txt output.txt\n"
  "Where alphaMonFile is a list of reads with alpha chains monomers parsed out, one read per\n"
  "line, with the first word in the line being the read id and subsequent words the monomers\n"
  "within the read. The monomerOrder.txt has one line per major monomer type with a word for\n"
  "each variant. The lines are in the same order the monomers appear, with the last line\n"
  "being followed in order by the first since they repeat.   The output.txt contains one line\n"
  "for each monomer in the output, just containing the monomer symbol.\n"
  "options:\n"
  "   -size=N - Set max chain size, default %d\n"
  "   -fullOnly - Only output chains of size above\n"
  "   -chain=fileName - Write out word chain to file\n"
  "   -afterChain=fileName - Write out word chain after faux generation to file for debugging\n"
  "   -outSize=N - Output this many words.\n"
  "   -seed=N - Initialize random number with this seed for consistent results, otherwise\n"
  "             it will generate different results each time it's run.\n"
  , maxChainSize
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"size", OPTION_INT},
   {"chain", OPTION_STRING},
   {"afterChain", OPTION_STRING},
   {"fullOnly", OPTION_BOOLEAN},
   {"outSize", OPTION_INT},
   {"seed", OPTION_INT},
   {NULL, 0},
};

/* Some structures to keep track of words (which correspond to alpha satellight monomers)
 * seen in input. */

struct wordInfo
/* Basic information on a word including how many times it is seen in input and output
 * streams.  Unlike the wordTree, this is flat, and does not include predecessors. */
    {
    struct wordInfo *next;	/* Next in list of all words. */
    char *word;			/* The word itself.  Not allocated here. */
    int useCount;		/* Number of times used. */
    int outTarget;		/* Number of times want to output word. */
    int outCount;		/* Number of times have output word so far. */
    };

struct wordInfoRef
/* A reference to a word. */
    {
    struct wordInfoRef *next;	/* Next in list */
    struct wordInfo *val;	/* The word referred to. */
    };

struct wordType
/* A collection of words of the same type - or monomers of same type. */
    {
    struct wordType *next;   /* Next wordType */
    struct wordInfoRef *list;	    /* List of all words of that type */
    };

struct wordStore
/* Stores info on all words */
    {
    struct wordInfo *infoList;   /* List of words, fairly arbitrary order. */
    struct hash *infoHash;	     /* Hash of wordInfo, keyed by word. */
    struct wordTree *markovChains;   /* Tree of words that follow other words. */
    int maxChainSize;		/* Maximum depth of tree. */
    struct wordType *typeList;	/* List of all types. */
    struct hash *typeHash;	/* Hash with wordType values, keyed by all words. */
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
    struct wordInfo *info;	/* The info on the word itself. */
    int useCount;		/* Number of times word used in input with given predecessors. */
    int outTarget;              /* Number of times want to output word with given predecessors. */
    int outCount;		/* Number of times actually output with given predecessors. */
    double normVal;             /* value to place the normalization value */    
    };

struct wordTree *wordTreeNew(struct wordInfo *info)
/* Create and return new wordTree element. */
{
struct wordTree *wt;
AllocVar(wt);
wt->info = info;
return wt;
}

int wordTreeCmpWord(const void *va, const void *vb)
/* Compare two wordTree for slSort. */
{
const struct wordTree *a = *((struct wordTree **)va);
const struct wordTree *b = *((struct wordTree **)vb);
return cmpStringsWithEmbeddedNumbers(a->info->word, b->info->word);
}

struct wordTree *wordTreeFindInList(struct wordTree *list, struct wordInfo *info)
/* Return wordTree element in list that has given word, or NULL if none. */
{
struct wordTree *wt;
for (wt = list; wt != NULL; wt = wt->next)
    if (wt->info == info)
        break;
return wt;
}

struct wordTree *wordTreeAddFollowing(struct wordTree *wt, struct wordInfo *info)
/* Make word follow wt in tree.  If word already exists among followers
 * return it and bump use count.  Otherwise create new one. */
{
struct wordTree *child = wordTreeFindInList(wt->children, info);
if (child == NULL)
    {
    child = wordTreeNew(info);
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
    struct wordInfo *info = node->val;
    verbose(2, "  adding %s\n", info->word);
    wt = wordTreeAddFollowing(wt, info);
    }
}

void wordTreeNormalize(struct wordTree *wt, double outTarget, double normVal)
/* Recursively set wt->normVal  and wt->outTarget so each branch gets its share */
{
wt->normVal = normVal;
wt->outTarget = outTarget;
int childrenTotalUses = wordTreeSumUseCounts(wt->children);
struct wordTree *child;
for (child = wt->children; child != NULL; child = child->next)
    {
    double childRatio = (double)child->useCount / childrenTotalUses;
    wordTreeNormalize(child, childRatio*outTarget, childRatio*normVal);
    }
}

char *wordTreeString(struct wordTree *wt)
/* Return something like '(a b c)' where c would be the value at wt itself, and
 * a and b would be gotten by following parents. */
{
struct slName *list = NULL, *el;
for (;wt != NULL; wt = wt->parent)
    {
    char *word = wt->info->word;
    if (!isEmpty(word))   // Avoid blank great grandparent
	slNameAddHead(&list, word);
    }

struct dyString *dy = dyStringNew(0);
dyStringAppendC(dy, '(');
for (el = list; el != NULL; el = el->next)
   {
   dyStringPrintf(dy, "%s", el->name);
   if (el->next != NULL)
       dyStringAppendC(dy, ' ');
   }
dyStringAppendC(dy, ')');
slFreeList(&list);
return dyStringCannibalize(&dy);
}

char *dlListFragWords(struct dlNode *head)
/* Return string containing all words in list pointed to by head. */
{
struct dyString *dy = dyStringNew(0);
dyStringAppendC(dy, '{');
struct dlNode *node;
for (node = head; !dlEnd(node); node = node->next)
    {
    if (node != head)
       dyStringAppendC(dy, ' ');
    struct wordInfo *info = node->val;
    dyStringAppend(dy, info->word);
    }
dyStringAppendC(dy, '}');
return dyStringCannibalize(&dy);
}

void wordTreeDump(int level, struct wordTree *wt, int maxChainSize, FILE *f)
/* Write out wordTree to file. */
{
static char *words[64];
int i;
assert(level < ArraySize(words));

words[level] = wt->info->word;
if (!fullOnly || level == maxChainSize)
    {
    fprintf(f, "%d\t%d\t%d\t%d\t%f\t", 
	    level, wt->useCount, wt->outTarget, wt->outCount, wt->normVal);
    
    for (i=1; i<=level; ++i)
	{
	spaceOut(f, level*2);
	fprintf(f, "%s ", words[i]);
	}
    fprintf(f, "\n");
    }
struct wordTree *child;
for (child = wt->children; child != NULL; child = child->next)
    wordTreeDump(level+1, child, maxChainSize, f);
}

struct wordTree *pickRandomOnOutTarget(struct wordTree *list)
/* Pick word from list randomly, but so that words with higher outTargets
 * are picked more often. */
{
struct wordTree *picked = NULL;

/* Debug output. */
    {
    verbose(2, "   pickRandomOnOutTarget(");
    struct wordTree *wt;
    for (wt = list; wt != NULL; wt = wt->next)
        {
	if (wt != list)
	    verbose(2, " ");
	verbose(2, "%s:%d", wt->info->word, wt->outTarget);
	}
    verbose(2, ") total %d\n", wordTreeSumOutTargets(list));
    }

/* Figure out total number of outputs left, and a random number between 0 and that total. */
int total = wordTreeSumOutTargets(list);
if (total > 0)
    {
    int threshold = rand() % total; 
    verbose(2, "      threshold %d\n", threshold);

    /* Loop through list returning selection corresponding to random threshold. */
    int binStart = 0;
    struct wordTree *wt;
    for (wt = list; wt != NULL; wt = wt->next)
	{
	int size = wt->outTarget;
	int binEnd = binStart + size;
	verbose(2, "      %s size %d, binEnd %d\n", wt->info->word, size, binEnd);
	if (threshold < binEnd)
	    {
	    picked = wt;
	    verbose(2, "      picked %s\n", wt->info->word);
	    break;
	    }
	binStart = binEnd;
	}
    }
return picked;
}

struct wordTree *pickRandomOnUseCounts(struct wordTree *list)
/* Pick word from list randomly, but so that words with higher useCounts
 * are picked more often.  Much like above routine, but a little simple
 * since we know useCounts are non-zero. */
{
struct wordTree *picked = NULL;

/* Figure out total number and a random number between 0 and that total. */
int total = wordTreeSumUseCounts(list);
assert(total > 0);
int threshold = rand() % total; 

/* Loop through list returning selection corresponding to random threshold. */
int binStart = 0;
struct wordTree *wt;
for (wt = list; wt != NULL; wt = wt->next)
    {
    int size = wt->useCount;
    int binEnd = binStart + size;
    if (threshold < binEnd)
	{
	picked = wt;
	break;
	}
    binStart = binEnd;
    }
assert(picked != NULL);
return picked;
}

int totUseZeroCount = 0;

struct wordTree *pickRandom(struct wordTree *list)
/* Pick word from list randomly, but so that words more
 * commonly seen are picked more often. */
{
struct wordTree *picked = pickRandomOnOutTarget(list);

/* If did not find anything, that's ok. It can happen on legitimate input due to unevenness
 * of read coverage.  In this case we pick a random number based on original counts
 * rather than normalized/counted down counts. */
if (picked == NULL)
    {
    picked = pickRandomOnUseCounts(list);
    ++totUseZeroCount;
    }
return picked;
}

struct wordInfo *pickRandomFromType(struct wordType *type)
/* Pick a random word, weighted by outTarget, from all available of given type */
{
/* Figure out total on list */
int total = 0;
struct wordInfoRef *ref;
for (ref = type->list; ref != NULL; ref = ref->next)
    total += ref->val->outTarget;

if (total > 0)
    {
    /* Loop through list returning selection corresponding to random threshold. */
    int threshold = rand() % total; 
    int binStart = 0;
    for (ref = type->list; ref != NULL; ref = ref->next)
	{
	struct wordInfo *info = ref->val;
	int size = info->outTarget;
	int binEnd = binStart + size;
	if (threshold < binEnd)
	    return info;
	binStart = binEnd;
	}
    }

verbose(2, "Fell off end in pickRandomFromType\n");
return type->list->val;	    // Fall back position (necessary?) just first in list
}

struct wordTree *predictNextFromAllPredecessors(struct wordTree *wt, struct dlNode *list)
/* Predict next word given tree and recently used word list.  If tree doesn't
 * have statistics for what comes next given the words in list, then it returns
 * NULL. */
{
verbose(2, " predictNextFromAllPredecessors(%s, %s)\n", wordTreeString(wt), dlListFragWords(list));
struct dlNode *node;
for (node = list; !dlEnd(node); node = node->next)
    {
    struct wordInfo *info = node->val;
    wt = wordTreeFindInList(wt->children, info);
    verbose(2, "   wordTreeFindInList(%s) = %p %s\n", info->word, wt, wordTreeString(wt));
    if (wt == NULL || wt->children == NULL)
        break;
    }
struct wordTree *result = NULL;
if (wt != NULL && wt->children != NULL)
    result = pickRandom(wt->children);
return result;
}

struct wordTree *predictFromWordTree(struct wordTree *wt, struct dlNode *recent)
/* Predict next word given tree and recently used word list.  Will use all words in
 * recent list if can,  but if there is not data in tree, will back off, and use
 * progressively less previous words until ultimately it just picks a random
 * word. */
{
struct dlNode *node;

for (node = recent; !dlEnd(node); node = node->next)
    {
    struct wordTree *result = predictNextFromAllPredecessors(wt, node);
    if (result != NULL)
        return result;
    }
return NULL;
}

struct dlNode *nodesFromTail(struct dlList *list, int count)
/* Return count nodes from tail of list.  If list is shorter than count, it returns the
 * whole list. */
{
int i;
struct dlNode *node;
for (i=0, node = list->tail; i<count; ++i)
    {
    if (dlStart(node))
        return list->head;
    node = node->prev;
    }
return node;
}

struct wordType *advanceTypeWithWrap(struct wordType *typeList, struct wordType *startType,
    int amountToAdvance)
/* Skip forward amountToAdvance in typeList from startType.  If get to end of list start
 * again at the beginning */
{
int i;
struct wordType *type = startType;
for (i=0; i<amountToAdvance; ++i)
    {
    type = type->next;
    if (type == NULL)
	type = typeList;
    }
return type;
}

struct wordTree *predictFromPreviousTypes(struct wordStore *store, struct dlList *past)
/* Predict next based on general pattern of monomer order. */
{
int maxToLookBack = 3;
    { /* Debugging block */
    struct dlNode *node = nodesFromTail(past, maxToLookBack);
    verbose(3, "predictFromPreviousTypes(");
    for (; !dlEnd(node); node = node->next)
        {
	struct wordInfo *info = node->val;
	verbose(3, "%s, ", info->word);
	}
    verbose(3, ")\n");
    }
int pastDepth;
struct dlNode *node;
for (pastDepth = 1, node = past->tail; pastDepth <= maxToLookBack; ++pastDepth)
    {
    if (dlStart(node))
	{
	verbose(2, "predictFromPreviousTypes with empty past\n");
	return NULL;
	}
    struct wordInfo *info = node->val;
    struct wordType *prevType = hashFindVal(store->typeHash, info->word);
    if (prevType != NULL)
        {
	struct wordType *curType = advanceTypeWithWrap(store->typeList, prevType, pastDepth);
	struct wordInfo *curInfo = pickRandomFromType(curType);
	struct wordTree *curTree = wordTreeFindInList(store->markovChains->children, curInfo);
	verbose(3, "predictFromPreviousType pastDepth %d, curInfo %s, curTree %p %s\n", 
	    pastDepth, curInfo->word, curTree, curTree->info->word);
	return curTree;
	}
    node = node->prev;
    }
verbose(2, "predictFromPreviousTypes past all unknown types\n");
return NULL;
}


struct wordTree *predictNext(struct wordStore *store, struct dlList *past)
/* Given input data store and what is known from the past, predict the next word. */
{
struct dlNode *recent = nodesFromTail(past, store->maxChainSize);
struct wordTree *pick =  predictFromWordTree(store->markovChains, recent);
if (pick == NULL)
    pick = predictFromPreviousTypes(store, past);
if (pick == NULL)
    {
    pick = pickRandom(store->markovChains->children);
    verbose(2, "in predictNext() last resort pick of %s\n", pick->info->word);
    }
return pick;
}

void decrementOutputCountsInTree(struct wordTree *wt)
/* Decrement output count of self and parents. */
{
while (wt != NULL)
    {
    /* Decrement target count, but don't let it fall below sum of counts of all children. 
     * This can happen with incomplete data if we don't prevent it.  This
     * same code also prevents us from having negative outTarget. */
    int outTarget = wt->outTarget - 1;
    int kidSum = wordTreeSumOutTargets(wt->children);
    if (outTarget < kidSum)
        outTarget = kidSum;
    wt->outTarget = outTarget;

    /* Always bump outCount for debugging. */
    wt->outCount += 1;
    wt = wt->parent;
    }
}

static struct dlList *wordTreeGenerateList(struct wordStore *store, int maxSize, 
    struct wordTree *firstWord, int maxOutputWords)
/* Make a list of words based on probabilities in tree. */
{
struct dlList *ll = dlListNew();
int chainSize = 0;
int outputWords = 0;
struct dlNode *chainStartNode = NULL;

for (;;)
    {
    if (++outputWords > maxOutputWords)
        break;
    struct dlNode *node;
    struct wordTree *picked;

    /* Get next predicted word. */
    AllocVar(node);
    if (chainSize == 0)
        {
	chainStartNode = node;
	++chainSize;
	picked = firstWord;
	}
    else if (chainSize >= maxSize)
	{
	chainStartNode = chainStartNode->next;
	picked = predictNext(store, ll);
	}
    else
	{
	picked = predictNext(store, ll);
	++chainSize;
	}

    if (picked == NULL)
         break;


    /* Add word from whatever level we fetched back to our output chain. */
    struct wordInfo *info = picked->info;
    node->val = info;
    dlAddTail(ll, node);

    decrementOutputCountsInTree(picked);
    info->outTarget -= 1;
    info->outCount += 1;
    }
verbose(2, "totUseZeroCount = %d\n", totUseZeroCount);
return ll;
}

static void wordTreeGenerateFile(struct wordStore *store, int maxSize, struct wordTree *firstWord, 
	int maxOutputWords, char *fileName)
/* Create file containing words base on tree probabilities.  The wordTreeGenerateList does
 * most of work. */
{
struct dlList *ll = wordTreeGenerateList(store, maxSize, firstWord, maxOutputWords);
FILE *f = mustOpen(fileName, "w");
struct dlNode *node;
for (node = ll->head; !dlEnd(node); node = node->next)
    {
    struct wordInfo *info = node->val;
    fprintf(f, "%s\n", info->word);
    }
carefulClose(&f);
dlListFree(&ll);
}


static void wordTreeSort(struct wordTree *wt)
/* Sort all children lists in tree. */
{
slSort(&wt->children, wordTreeCmpWord);
struct wordTree *child;
for (child = wt->children; child != NULL; child = child->next)
    wordTreeSort(child);
}

struct wordStore *wordStoreNew(int maxChainSize)
/* Allocate and initialize a new word store. */
{
struct wordStore *store;
AllocVar(store);
store->infoHash = hashNew(0);
store->maxChainSize = maxChainSize;
return store;
}

struct wordInfo *wordStoreAdd(struct wordStore *store, char *word)
/* Add word to store,  incrementing it's useCount if it's already there, otherwise
 * making up a new record for it. */
{
struct wordInfo *info = hashFindVal(store->infoHash, word);
if (info == NULL)
    {
    AllocVar(info);
    hashAddSaveName(store->infoHash, word, info, &info->word);
    slAddHead(&store->infoList, info);
    }
info->useCount += 1;
return info;
}

struct wordStore *wordStoreForChainsInFile(char *fileName, int chainSize)
/* Return a wordStore containing all words, and also all chains-of-words of length 
 * chainSize seen in file.  */
{
/* Stuff for processing file a line at a time. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;

/* We'll build up the tree starting with an empty root node. */
struct wordStore *store = wordStoreNew(chainSize);
struct wordTree *wt = store->markovChains = wordTreeNew(wordStoreAdd(store, ""));	

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
	struct wordInfo *info = wordStoreAdd(store, word);
	 /* For the first few words in the file after ID, we'll just build up the chain,
	 * only adding it to the tree when we finally do get to the desired
	 * chain size.  Once past the initial section of the file we'll be
	 * getting rid of the first link in the chain as well as adding a new
	 * last link in the chain with each new word we see. */
	if (curSize < chainSize)
	    {
	    dlAddValTail(chain, info);
	    ++curSize;
	    if (curSize == chainSize)
		addChainToTree(wt, chain);
	    }
	else
	    {
	    /* Reuse doubly-linked-list node, but give it a new value, as we move
	     * it from head to tail of list. */
	    node = dlPopHead(chain);
	    node->val = info;
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
	freeMem(node);
	}
    dlListFree(&chain);
    }
lineFileClose(&lf);

wordTreeSort(wt);  // Make output of chain file prettier
return store;
}

void wordTreeWrite(struct wordTree *wt, int maxChainSize, char *fileName)
/* Write out tree to file */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "#level\tuseCount\toutTarget\toutCount\tnormVal\tmonomers\n");
wordTreeDump(0, wt, maxChainSize, f);
carefulClose(&f);
}

void wordStoreLoadMonomerOrder(struct wordStore *store, char *readsFile, char *fileName)
/* Read in a file with one line for each monomer type, containing a word for each
 * monomer variant.  Requires all variants already be in store.  The readsFile is passed
 * just for nicer error reporting. */
{
/* Stuff for processing file a line at a time. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;

/* Set up variables we'll put results in in store. */
store->typeHash = hashNew(0);
store->typeList = NULL;

while (lineFileNextReal(lf, &line))
    {
    struct wordType *type;
    AllocVar(type);
    slAddHead(&store->typeList, type);
    while ((word = nextWord(&line)) != NULL)
        {
	struct wordInfo *info = hashFindVal(store->infoHash, word);
	if (info == NULL)
	    errAbort("%s is in %s but not %s", word, lf->fileName, readsFile);
	struct wordInfoRef *ref;
	AllocVar(ref);
	ref->val = info;
	slAddHead(&type->list, ref);
	hashAddUnique(store->typeHash, word, type);
	}
    }
slReverse(&store->typeList);
lineFileClose(&lf);
verbose(2, "Added %d types containing %d words from %s\n", 
    slCount(store->typeList), store->typeHash->elCount, fileName);
}

void wordInfoListNormalise(struct wordInfo *list, int totalCount, int outputSize)
/* Set outTarget field in all of list to be normalized to outputSize */
{
struct wordInfo *info;
double scale = outputSize/totalCount;
for (info = list; info != NULL; info = info->next)
    info->outTarget = round(scale * info->useCount);
}

void wordStoreNormalize(struct wordStore *store, int outputSize)
/* Set up output counts on each word to make sure it gets it's share of output size */
{
struct wordTree *wt = store->markovChains;
wordTreeNormalize(wt, outputSize, 1.0);
wordInfoListNormalise(store->infoList, wt->useCount, outputSize);
}

void alphaChain(char *readsFile, char *monomerOrderFile, char *outFile)
/* alphaChain - Create Markov chain of words and optionally output chain in two formats. */
{
struct wordStore *store = wordStoreForChainsInFile(readsFile, maxChainSize);
struct wordTree *wt = store->markovChains;
wordStoreLoadMonomerOrder(store, readsFile, monomerOrderFile);
wordStoreNormalize(store, outSize);

if (optionExists("chain"))
    {
    char *fileName = optionVal("chain", NULL);
    wordTreeWrite(wt, store->maxChainSize, fileName);
    }

wordTreeGenerateFile(store, store->maxChainSize, pickRandom(wt->children), outSize, outFile);

if (optionExists("afterChain"))
    {
    char *fileName = optionVal("afterChain", NULL);
    wordTreeWrite(wt, store->maxChainSize, fileName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
warn("Program obsolete - try using alphaAsm instead");
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
maxChainSize = optionInt("size", maxChainSize);
outSize = optionInt("outSize", outSize);
fullOnly = optionExists("fullOnly");
int seed = optionInt("seed", (int)time(0));
srand(seed);
alphaChain(argv[1], argv[2], argv[3]);
return 0;
}

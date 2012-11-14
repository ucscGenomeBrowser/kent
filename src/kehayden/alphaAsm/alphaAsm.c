/* alphaAsm - assemble alpha repeat regions such as centromeres from reads that have
 * been parsed into various repeat monomer variants.  Cycles of these variants tend to
 * form higher order repeats. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dlist.h"

/* Global vars - all of which can be set by command line options. */
int pseudoCount = 1;
int maxChainSize = 3;
int outSize = 10000;
boolean fullOnly = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "alphaAsm - create a linear projection of alpha satellite arrays using the probablistic model\n"
  "of HuRef satellite graphs based on Sanger style reads.\n"
  "usage:\n"
  "   alphaAsm alphaMonFile.txt monomerOrder.txt output.txt\n"
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
  "   -outSize=N - Output this many words. Default %d\n"
  "   -pseudoCount=N - Add this number to observed quantities - helps compensate for what's not\n"
  "                observed.  Default %d\n"
  "   -seed=N - Initialize random number with this seed for consistent results, otherwise\n"
  "             it will generate different results each time it's run.\n"
  , maxChainSize, outSize, pseudoCount
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
   {"pseudoCount", OPTION_INT},
   {NULL, 0},
};

/* Some structures to keep track of words (which correspond to alpha satellight monomers)
 * seen in input. */

struct monomer
/* Basic information on a monomer including how many times it is seen in input and output
 * streams.  Unlike the wordTree, this is flat, and does not include predecessors. */
    {
    struct monomer *next;	/* Next in list of all words. */
    char *word;			/* The word used to represent monomer.  Not allocated here. */
    int useCount;		/* Number of times used. */
    int outTarget;		/* Number of times want to output word. */
    int outCount;		/* Number of times have output word so far. */
    struct monomerType *type;	/* The type of the monomer. */
    struct slRef *readList;	/* List of references to reads this is in. */
    };

struct monomerRef
/* A reference to a monomer. */
    {
    struct monomerRef *next;	/* Next in list */
    struct monomer *val;	/* The word referred to. */
    };

struct monomerType
/* A collection of words of the same type - or monomers of same type. */
    {
    struct monomerType *next;   /* Next monomerType */
    char *name;			/* Short name of type */
    struct monomerRef *list;	    /* List of all words of that type */
    };

struct alphaRead 
/* A read that's been parsed into alpha variants. */
    {
    struct alphaRead *next;	/* Next in list */
    char *name;			/* Id of read, not allocated here. */
    struct monomerRef *list;	/* List of alpha subunits */
    };

struct alphaStore
/* Stores info on all monomers */
    {
    struct monomer *monomerList;   /* List of words, fairly arbitrary order. */
    struct hash *monomerHash;       /* Hash of monomer, keyed by word. */
    struct alphaRead *readList;	 /* List of all reads. */
    struct hash *readHash;	 /* Hash of all reads keyed by read ID. */
    struct wordTree *markovChains;   /* Tree of words that follow other words. */
    int maxChainSize;		/* Maximum depth of tree. */
    struct monomerType *typeList;	/* List of all types. */
    struct hash *typeHash;	/* Hash with monomerType values, keyed by all words. */
    int typeCount;		/* Count of types. */
    struct monomerType **typeArray;  /* Array as opposed to list of types */
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
    struct monomer *monomer;	/* The info on the word itself. */
    int useCount;		/* Number of times word used in input with given predecessors. */
    int outTarget;              /* Number of times want to output word with given predecessors. */
    int outCount;		/* Number of times actually output with given predecessors. */
    double normVal;             /* value to place the normalization value */    
    };

struct wordTree *wordTreeNew(struct monomer *monomer)
/* Create and return new wordTree element. */
{
struct wordTree *wt;
AllocVar(wt);
wt->monomer = monomer;
return wt;
}

int wordTreeCmpWord(const void *va, const void *vb)
/* Compare two wordTree for slSort. */
{
const struct wordTree *a = *((struct wordTree **)va);
const struct wordTree *b = *((struct wordTree **)vb);
return cmpStringsWithEmbeddedNumbers(a->monomer->word, b->monomer->word);
}

static void wordTreeSort(struct wordTree *wt)
/* Sort all children lists in tree. */
{
slSort(&wt->children, wordTreeCmpWord);
struct wordTree *child;
for (child = wt->children; child != NULL; child = child->next)
    wordTreeSort(child);
}

struct wordTree *wordTreeFindInList(struct wordTree *list, struct monomer *monomer)
/* Return wordTree element in list that has given word, or NULL if none. */
{
struct wordTree *wt;
for (wt = list; wt != NULL; wt = wt->next)
    if (wt->monomer == monomer)
        break;
return wt;
}

struct wordTree *wordTreeAddFollowing(struct wordTree *wt, struct monomer *monomer)
/* Make word follow wt in tree.  If word already exists among followers
 * return it and bump use count.  Otherwise create new one. */
{
struct wordTree *child = wordTreeFindInList(wt->children, monomer);
if (child == NULL)
    {
    child = wordTreeNew(monomer);
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
    struct monomer *monomer = node->val;
    verbose(2, "  adding %s\n", monomer->word);
    wt = wordTreeAddFollowing(wt, monomer);
    }
}

int wordTreeAddPseudoCount(struct wordTree *wt, int pseudo)
/* Add pseudo to all leaves of tree and propagate counts up to parents. */
{
if (wt->children == NULL)
    {
    wt->useCount += pseudo;
    return wt->useCount;
    }
else
    {
    struct wordTree *child;
    int oldChildTotal = 0;
    for (child = wt->children; child != NULL; child = child->next)
	oldChildTotal += child->useCount;
    int oldDiff = wt->useCount - oldChildTotal;
    if (oldDiff < 0) oldDiff = 0;  // Necessary in rare cases, not sure why.

    int total = 0;
    for (child = wt->children; child != NULL; child = child->next)
	total += wordTreeAddPseudoCount(child, pseudo);
    wt->useCount = total + oldDiff + pseudo;
    return total;
    }
}

void wordTreeNormalize(struct wordTree *wt, double outTarget, double normVal)
/* Recursively set wt->normVal  and wt->outTarget so each branch gets its share */
{
if (pseudoCount > 0)
    wordTreeAddPseudoCount(wt, pseudoCount);
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
    char *word = wt->monomer->word;
    if (!isEmpty(word))   // Avoid blank great grandparent at root of tree.
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
    struct monomer *monomer = node->val;
    dyStringAppend(dy, monomer->word);
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

words[level] = wt->monomer->word;
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

struct wordTree *pickWeightedRandomFromList(struct wordTree *list)
/* Pick word from list randomly, but so that words with higher outTargets
 * are picked more often. */
{
struct wordTree *picked = NULL;

/* Debug output. */
    {
    verbose(2, "   pickWeightedRandomFromList(");
    struct wordTree *wt;
    for (wt = list; wt != NULL; wt = wt->next)
        {
	if (wt != list)
	    verbose(2, " ");
	verbose(2, "%s:%d", wt->monomer->word, wt->outTarget);
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
	verbose(2, "      %s size %d, binEnd %d\n", wt->monomer->word, size, binEnd);
	if (threshold < binEnd)
	    {
	    picked = wt;
	    verbose(2, "      picked %s\n", wt->monomer->word);
	    break;
	    }
	binStart = binEnd;
	}
    }
return picked;
}

int totUseZeroCount = 0;

struct monomer *pickRandomFromType(struct monomerType *type)
/* Pick a random word, weighted by outTarget, from all available of given type */
{
/* Figure out total on list */
int total = 0;
struct monomerRef *ref;
for (ref = type->list; ref != NULL; ref = ref->next)
    total += ref->val->outTarget;

/* Loop through list returning selection corresponding to random threshold. */
if (total > 0)
    {
    int threshold = rand() % total; 
    int binStart = 0;
    for (ref = type->list; ref != NULL; ref = ref->next)
	{
	struct monomer *monomer = ref->val;
	int size = monomer->outTarget;
	int binEnd = binStart + size;
	if (threshold < binEnd)
	    return monomer;
	binStart = binEnd;
	}
    }

verbose(2, "Backup plan for %s in pickRandomFromType\n", type->name);

/* Try again based on use counts. */
total = 0;
for (ref = type->list; ref != NULL; ref = ref->next)
    total += ref->val->useCount;
if (total > 0)
    {
    int threshold = rand() % total; 
    int binStart = 0;
    for (ref = type->list; ref != NULL; ref = ref->next)
	{
	struct monomer *monomer = ref->val;
	int size = monomer->useCount;
	int binEnd = binStart + size;
	if (threshold < binEnd)
	    return monomer;
	binStart = binEnd;
	}
    }

errAbort("Fell off end in pickRandomFromType on %s", type->name);
return NULL;
}

struct wordTree *predictNextFromAllPredecessors(struct wordTree *wt, struct dlNode *list)
/* Predict next word given tree and recently used word list.  If tree doesn't
 * have statistics for what comes next given the words in list, then it returns
 * NULL. */
{
verbose(2, "  predictNextFromAllPredecessors(%s, %s)\n", wordTreeString(wt), dlListFragWords(list));
struct dlNode *node;
for (node = list; !dlEnd(node); node = node->next)
    {
    struct monomer *monomer = node->val;
    wt = wordTreeFindInList(wt->children, monomer);
    verbose(2, "   wordTreeFindInList(%s) = %p %s\n", monomer->word, wt, wordTreeString(wt));
    if (wt == NULL || wt->children == NULL)
        break;
    }
struct wordTree *result = NULL;
if (wt != NULL && wt->children != NULL)
    result = pickWeightedRandomFromList(wt->children);
return result;
}

struct wordTree *predictFromWordTree(struct wordTree *wt, struct dlNode *recent)
/* Predict next word given tree and recently used word list.  Will use all words in
 * recent list if can,  but if there is not data in tree, will back off, and use
 * progressively less previous words. */
{
struct dlNode *node;
verbose(2, " predictNextFromWordTree(%s)\n", wordTreeString(wt));

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

struct monomerType *typeAfter(struct alphaStore *store, struct monomerType *oldType,
    int amount)
/* Skip forward amount in typeList from oldType.  If get to end of list start
 * again at the beginning */
{
int i;
struct monomerType *type = oldType;
for (i=0; i<amount; ++i)
    {
    type = type->next;
    if (type == NULL)
	type = store->typeList;
    }
return type;
}

struct monomerType *typeBefore(struct alphaStore *store, struct monomerType *oldType, int amount)
/* Pick type that comes amount before given type. */
{
int typeIx = ptArrayIx(oldType, store->typeArray, store->typeCount);
typeIx -= amount;
while (typeIx < 0)
    typeIx += store->typeCount;
return store->typeArray[typeIx];
}

struct wordTree *predictFromPreviousTypes(struct alphaStore *store, struct dlList *past)
/* Predict next based on general pattern of monomer order. */
{
/* This routine is now a bit more complex than necessary but it still works.  It
 * these days only needs to look back 1 because of the read-based type fill-in. */
int maxToLookBack = 3;
    { /* Debugging block */
    struct dlNode *node = nodesFromTail(past, maxToLookBack);
    verbose(3, "predictFromPreviousTypes(");
    for (; !dlEnd(node); node = node->next)
        {
	struct monomer *monomer = node->val;
	verbose(3, "%s, ", monomer->word);
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
    struct monomer *monomer = node->val;
    struct monomerType *prevType = hashFindVal(store->typeHash, monomer->word);
    if (prevType != NULL)
        {
	struct monomerType *curType = typeAfter(store, prevType, pastDepth);
	struct monomer *curInfo = pickRandomFromType(curType);
	struct wordTree *curTree = wordTreeFindInList(store->markovChains->children, curInfo);
	verbose(3, "predictFromPreviousType pastDepth %d, curInfo %s, curTree %p %s\n", 
	    pastDepth, curInfo->word, curTree, curTree->monomer->word);
	return curTree;
	}
    node = node->prev;
    }
verbose(2, "predictFromPreviousTypes past all unknown types\n");
return NULL;
}


struct wordTree *predictNext(struct alphaStore *store, struct dlList *past)
/* Given input data store and what is known from the past, predict the next word. */
{
struct dlNode *recent = nodesFromTail(past, store->maxChainSize);
struct wordTree *pick =  predictFromWordTree(store->markovChains, recent);
if (pick == NULL)
    pick = predictFromPreviousTypes(store, past);
if (pick == NULL)
    {
    pick = pickWeightedRandomFromList(store->markovChains->children);
    warn("in predictNext() last resort pick of %s", pick->monomer->word);
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

    /* Always bump outCount to help us debug (only real use of outCount). */
    wt->outCount += 1;
    wt = wt->parent;
    }
}

static struct dlList *wordTreeGenerateList(struct alphaStore *store, int maxSize, 
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
    struct monomer *monomer = picked->monomer;
    node->val = monomer;
    dlAddTail(ll, node);

    decrementOutputCountsInTree(picked);
    monomer->outTarget -= 1;
    monomer->outCount += 1;
    }
verbose(2, "totUseZeroCount = %d\n", totUseZeroCount);
return ll;
}

static void wordTreeGenerateFile(struct alphaStore *store, int maxSize, struct wordTree *firstWord, 
	int maxOutputWords, char *fileName)
/* Create file containing words base on tree probabilities.  The wordTreeGenerateList does
 * most of work. */
{
struct dlList *ll = wordTreeGenerateList(store, maxSize, firstWord, maxOutputWords);
FILE *f = mustOpen(fileName, "w");
struct dlNode *node;
for (node = ll->head; !dlEnd(node); node = node->next)
    {
    struct monomer *monomer = node->val;
    fprintf(f, "%s\n", monomer->word);
    }
carefulClose(&f);
dlListFree(&ll);
}


struct alphaStore *alphaStoreNew(int maxChainSize)
/* Allocate and initialize a new word store. */
{
struct alphaStore *store;
AllocVar(store);
store->maxChainSize = maxChainSize;
store->monomerHash = hashNew(0);
store->typeHash = hashNew(0);
store->readHash = hashNew(0);
return store;
}

struct monomer *alphaStoreAddMonomer(struct alphaStore *store, char *word)
/* Add word to store,  incrementing it's useCount if it's already there, otherwise
 * making up a new record for it. */
{
struct monomer *monomer = hashFindVal(store->monomerHash, word);
if (monomer == NULL)
    {
    AllocVar(monomer);
    hashAddSaveName(store->monomerHash, word, monomer, &monomer->word);
    slAddHead(&store->monomerList, monomer);
    }
monomer->useCount += 1;
monomer->outTarget = monomer->useCount;	    /* Set default value, may be renormalized. */
return monomer;
}

void connectReadsToMonomers(struct alphaStore *store)
/* Add each read monomer appears in to monomer's readList. */
{
struct alphaRead *read;
for (read = store->readList; read != NULL; read = read->next)
    {
    struct monomerRef *ref;
    for (ref = read->list; ref != NULL; ref = ref->next)
        {
	struct monomer *monomer = ref->val;
	refAdd(&monomer->readList, read);
	}
    }
}

void alphaReadListFromFile(char *fileName, struct alphaStore *store)
/* Read in file and turn it into a list of alphaReads. */
{
/* Stuff for processing file a line at a time. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;

/* Loop through each line of file making up a read for it. */
struct alphaRead *readList = NULL;
while (lineFileNextReal(lf, &line))
    {
    /* Parse out first word of line into name, and rest into list. */
    char *name = nextWord(&line);
    struct monomerRef *list = NULL;
    while ((word = nextWord(&line)) != NULL)
        {
	struct monomer *monomer = alphaStoreAddMonomer(store, word);
	struct monomerRef *ref;
	AllocVar(ref);
	ref->val = monomer;
	slAddHead(&list, ref);
	}
    slReverse(&list);
    if (list == NULL)
        errAbort("Line %d of %s has no alpha monomers.", lf->lineIx, lf->fileName);

    /* Create data structure and add read to list and hash */
    if (hashLookup(store->readHash, name))
        errAbort("Read ID %s is repeated line %d of %s", name, lf->lineIx, lf->fileName);
    struct alphaRead *read;
    AllocVar(read);
    read->list = list;
    hashAddSaveName(store->readHash, name, read, &read->name);
    slAddHead(&readList, read);
    }
slReverse(&readList);
store->readList = readList;
lineFileClose(&lf);
connectReadsToMonomers(store);
}

struct alphaOrphan
/* Information about an orphan - something without great joining information. */
    {
    struct alphaOrphan *next;	/* Next in list */
    struct monomer *monomer;	/* Pointer to orphan */
    boolean paired;		/* True if has been paired with somebody. */
    };

struct alphaOrphan *findOrphanStarts(struct alphaRead *readList, struct alphaStore *store)
/* Return list of monomers that are found only at start of reads. */
{
struct alphaOrphan *orphanList = NULL;

/* Build up hash of things seen later in reads. */
struct alphaRead *read;
struct hash *later = hashNew(0);    /* Hash of monomers found later. */
for (read = readList; read != NULL; read = read->next)
    {
    struct monomerRef *ref;
    for (ref = read->list->next; ref != NULL; ref = ref->next)
        {
	hashAdd(later, ref->val->word, NULL);
	}
    }

/* Pass through again collecting orphans. */
struct hash *orphanHash = hashNew(0);
for (read = readList; read != NULL; read = read->next)
    {
    struct monomer *monomer = read->list->val;
    char *word = monomer->word;
    if (!hashLookup(later, word))
        {
	struct alphaOrphan *orphan = hashFindVal(orphanHash, word);
	if (orphan == NULL)
	    {
	    AllocVar(orphan);
	    orphan->monomer = monomer;
	    slAddHead(&orphanList, orphan);
	    hashAdd(orphanHash, word, orphan);
	    verbose(2, "Adding orphan start %s\n", word);
	    }
	}
    }
hashFree(&later);
hashFree(&orphanHash);
slReverse(&orphanList);
return orphanList;
}

struct alphaOrphan *findOrphanEnds(struct alphaRead *readList, struct alphaStore *store)
/* Return list of monomers that are found only at end of reads. */
{
struct alphaOrphan *orphanList = NULL;

/* Build up hash of things seen sooner in reads. */
struct alphaRead *read;
struct hash *sooner = hashNew(0);    /* Hash of monomers found sooner. */
for (read = readList; read != NULL; read = read->next)
    {
    struct monomerRef *ref;
    /* Loop over all but last. */
    for (ref = read->list; ref->next != NULL; ref = ref->next)
        {
	hashAdd(sooner, ref->val->word, NULL);
	}
    }

/* Pass through again collecting orphans. */
struct hash *orphanHash = hashNew(0);
for (read = readList; read != NULL; read = read->next)
    {
    struct monomerRef *lastRef = slLastEl(read->list);
    struct monomer *monomer = lastRef->val;
    if (!hashLookup(sooner, monomer->word))
        {
	struct alphaOrphan *orphan = hashFindVal(orphanHash, monomer->word);
	if (orphan == NULL)
	    {
	    AllocVar(orphan);
	    orphan->monomer = monomer;
	    slAddHead(&orphanList, orphan);
	    hashAdd(orphanHash, monomer->word, orphan);
	    verbose(2, "Adding orphan end %s\n", monomer->word);
	    }
	}
    }
hashFree(&sooner);
slReverse(&orphanList);
hashFree(&orphanHash);
return orphanList;
}

struct alphaRead *addReadOfTwo(struct alphaStore *store, struct monomer *a, struct monomer *b)
/* Make up a fake read that goes from a to b and add it to store. */
{
/* Create fake name and start fake read with it. */
static int fakeId = 0;
char name[32];
safef(name, sizeof(name), "fake%d", ++fakeId);

/* Allocate fake read. */
struct alphaRead *read;
AllocVar(read);

/* Add a and b to the read. */
struct monomerRef *aRef, *bRef;
AllocVar(aRef);
aRef->val = a;
AllocVar(bRef);
bRef->val = b;
aRef->next = bRef;
read->list = aRef;

/* Add read to the store. */
slAddHead(&store->readList, read);
hashAddSaveName(store->readHash, name, read, &read->name);

/* Add read to the monomers. */
refAdd(&a->readList, read);
refAdd(&a->readList, read);

a->useCount += 1;
b->useCount += 1;
return read;
}

void integrateOrphans(struct alphaStore *store)
/* Make up fake reads that integrate orphans (monomers only found at beginning or end
 * of a read) better. */
{
struct alphaOrphan *orphanStarts = findOrphanStarts(store->readList, store);
struct alphaOrphan *orphanEnds = findOrphanEnds(store->readList, store);
verbose(2, "orphanStarts has %d items, orphanEnds %d\n", 
    slCount(orphanStarts), slCount(orphanEnds));

/* First look for the excellent situation where you can pair up orphans with each 
 * other.  For Y at least this happens half the time. */
struct alphaOrphan *start, *end;
for (start = orphanStarts; start != NULL; start = start->next)
    {
    struct monomerType *startType = start->monomer->type;
    if (startType == NULL)
        continue;
    for (end = orphanEnds; end != NULL && !start->paired; end = end->next)
        {
	struct monomerType *endType = end->monomer->type;
	if (endType == NULL)
	    continue;
	if (!end->paired)
	    {
	    struct monomerType *nextType = typeAfter(store, endType, 1);
	    if (nextType == startType)
	        {
		end->paired = TRUE;
		start->paired = TRUE;
		addReadOfTwo(store, end->monomer, start->monomer);
		verbose(2, "Pairing %s with %s\n", end->monomer->word, start->monomer->word);
		}
	    }
	}
    }

/* Ok, now have to just manufacture other side of orphan starts out of thin air. */
for (start = orphanStarts; start != NULL; start = start->next)
    {
    if (start->paired)
        continue;
    struct monomer *startMono = start->monomer;
    struct monomerType *startType = startMono->type;
    if (startType == NULL)
        continue;

    struct monomerType *newType = typeBefore(store, startType, 1);
    struct monomer *newMono = pickRandomFromType(newType);
    addReadOfTwo(store, newMono, startMono);
    verbose(2, "Pairing new %s with start %s\n", newMono->word, startMono->word);
    }

/* Ok, now have to just manufacture other side of orphan ends out of thin air. */
for (end = orphanEnds; end != NULL; end = end->next)
    {
    if (end->paired)
        continue;
    struct monomer *endMono = end->monomer;
    struct monomerType *endType = endMono->type;
    if (endType == NULL)
        continue;

    struct monomerType *newType = typeAfter(store, endType, 1);
    struct monomer *newMono = pickRandomFromType(newType);
    addReadOfTwo(store, endMono, newMono);
    verbose(2, "Pairing end %s with new %s\n", endMono->word, newMono->word);
    }
}

void makeMarkovChains(struct alphaStore *store)
/* Return a alphaStore containing all words, and also all chains-of-words of length 
 * chainSize seen in file.  */
{
/* We'll build up the tree starting with an empty root node. */
struct wordTree *wt = store->markovChains = wordTreeNew(alphaStoreAddMonomer(store, ""));	
int chainSize = store->maxChainSize;

/* Loop through each read. There's special cases at the beginning and end of read, and for 
 * short reads.  In the main case we'll be maintaining a chain (doubly linked list) of maxChainSize 
 * words, * popping off one word from the start, and adding one word to the end for each
 * new word we encounter. This list is added to the tree each iteration. */
struct alphaRead *read;
for (read = store->readList; read != NULL; read = read->next)
    {
    /* We'll keep a chain of three or so words in a doubly linked list. */
    struct dlNode *node;
    struct dlList *chain = dlListNew();
    int curSize = 0;
    int wordCount = 0;

    struct monomerRef *ref;
    for (ref = read->list; ref != NULL; ref = ref->next)
        {
	struct monomer *monomer = ref->val;
	 /* For the first few words in the file after ID, we'll just build up the chain,
	 * only adding it to the tree when we finally do get to the desired
	 * chain size.  Once past the initial section of the file we'll be
	 * getting rid of the first link in the chain as well as adding a new
	 * last link in the chain with each new word we see. */
	if (curSize < chainSize)
	    {
	    dlAddValTail(chain, monomer);
	    ++curSize;
	    if (curSize == chainSize)
		addChainToTree(wt, chain);
	    }
	else
	    {
	    /* Reuse doubly-linked-list node, but give it a new value, as we move
	     * it from head to tail of list. */
	    node = dlPopHead(chain);
	    node->val = monomer;
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
wordTreeSort(wt);  // Make output of chain file prettier
}

void wordTreeWrite(struct wordTree *wt, int maxChainSize, char *fileName)
/* Write out tree to file */
{
FILE *f = mustOpen(fileName, "w");
fprintf(f, "#level\tuseCount\toutTarget\toutCount\tnormVal\tmonomers\n");
wordTreeDump(0, wt, maxChainSize, f);
carefulClose(&f);
}

void alphaStoreLoadMonomerOrder(struct alphaStore *store, char *readsFile, char *fileName)
/* Read in a file with one line for each monomer type, where first word is type name,
 * and rest of words are all the actually variants of that type.
 * Requires all variants already be in store.  The readsFile is passed
 * just for nicer error reporting. */
{
/* Stuff for processing file a line at a time. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *word;
struct hash *uniq = hashNew(0);

/* Set up variables we'll put results in in store. */
store->typeList = NULL;

while (lineFileNextReal(lf, &line))
    {
    char *name = nextWord(&line);
    if (hashLookup(uniq, name) != NULL)
        errAbort("Type name '%s' repeated line %d of %s\n", name, lf->lineIx, lf->fileName);
    struct monomerType *type;
    AllocVar(type);
    type->name = cloneString(name);
    slAddHead(&store->typeList, type);
    while ((word = nextWord(&line)) != NULL)
        {
	struct monomer *monomer = hashFindVal(store->monomerHash, word);
	if (monomer == NULL)
	    errAbort("%s is in %s but not %s", word, lf->fileName, readsFile);
	struct monomerRef *ref;
	AllocVar(ref);
	ref->val = monomer;
	slAddHead(&type->list, ref);
	hashAddUnique(store->typeHash, word, type);
	}
    }
slReverse(&store->typeList);
lineFileClose(&lf);
hashFree(&uniq);
verbose(2, "Added %d types containing %d words from %s\n", 
    slCount(store->typeList), store->typeHash->elCount, fileName);

/* Create type array */
store->typeCount = slCount(store->typeList);
struct monomerType **types = AllocArray(store->typeArray, store->typeCount);
struct monomerType *type;
int i;
for (i=0, type = store->typeList; i<store->typeCount; ++i, type = type->next)
    types[i] = type;
}

void monomerListNormalise(struct monomer *list, int totalCount, int outputSize)
/* Set outTarget field in all of list to be normalized to outputSize */
{
struct monomer *monomer;
double scale = outputSize/totalCount;
for (monomer = list; monomer != NULL; monomer = monomer->next)
    monomer->outTarget = round(scale * monomer->useCount);
}

void alphaStoreNormalize(struct alphaStore *store, int outputSize)
/* Set up output counts on each word to make sure it gets it's share of output size */
{
struct wordTree *wt = store->markovChains;
wordTreeNormalize(wt, outputSize, 1.0);
monomerListNormalise(store->monomerList, wt->useCount, outputSize);
}

struct monomerType *fillInTypeFromReads(struct monomer *monomer, struct alphaStore *store)
/* Look through read list trying to find best supported type for this monomer. */
{
/* The algorithm here could be better.  It picks the closest nearby monomer that is typed
 * to fill in from.  When have information from both before and after the program arbitrarily 
 * uses before preferentially.  When have information that is equally close from multiple reads,
 * it just uses info from first read.  Fortunately the data, at least on Y, shows a lot of
 * consistency, so it's probably not actually worth a more sophisticated algorithm that would
 * instead take the most popular choice rather than the first one. */
verbose(2, "fillInTypeFromReads on %s from %d reads\n", monomer->word, slCount(monomer->readList));
struct monomerType *bestType = NULL;
int bestPos = 0;
boolean bestIsAfter = FALSE;
struct slRef *readRef;
for (readRef = monomer->readList; readRef != NULL; readRef = readRef->next)
    {
    struct alphaRead *read = readRef->val;
    struct monomerRef *ref;
	{
	verbose(2, "  read %s (%d els):", read->name, slCount(read->list));
	for (ref = read->list; ref != NULL; ref = ref->next)
	    {
	    struct monomer *m = ref->val;
	    verbose(2, " %s", m->word);
	    }
	verbose(2, "\n");
	}
    struct monomerType *beforeType = NULL;
    int beforePos = 0;

    /* Look before. */
    for (ref = read->list; ref != NULL; ref = ref->next)
        {
	struct monomer *m = ref->val;
	if (m == monomer)
	     break;
	if (m->type != NULL)
	    {
	    beforeType = m->type;
	    beforePos = 0;
	    }
	++beforePos;
	}

    /* Look after. */
    struct monomerType *afterType = NULL;
    int afterPos = 0;
    assert(ref != NULL && ref->val == monomer);   
    for (ref = ref->next; ref != NULL; ref = ref->next)
        {
	struct monomer *m = ref->val;
	++afterPos;
	if (m->type != NULL)
	    {
	    afterType = m->type;
	    break;
	    }
	}

    if (beforeType != NULL)
        {
	if (bestType == NULL || beforePos < bestPos)
	    {
	    bestType = beforeType;
	    bestPos = beforePos;
	    bestIsAfter = FALSE;
	    }
	}

    if (afterType != NULL)
        {
	if (bestType == NULL || afterPos < bestPos)
	    {
	    bestType = afterType;
	    bestPos = afterPos;
	    bestIsAfter = TRUE;
	    }
	}
    }

/* Now have found a type that is at a known position relative to ourselves.  From this
 * infer our own type. */
struct monomerType *chosenType = NULL;
if (bestType != NULL)
    {
    if (bestIsAfter)
	chosenType = typeBefore(store, bestType, bestPos);
    else
	chosenType = typeAfter(store, bestType, bestPos);
    }
return chosenType;
}

void fillInTypes(struct alphaStore *store)
/* We have types for most but not all monomers. Try and fill out types for most of
 * the rest by looking for position in reads they are in that do contain known types */
{
/* Do first pass on ones defined - just have to look them up in hash. */
struct monomer *monomer;
for (monomer = store->monomerList; monomer != NULL; monomer = monomer->next)
    monomer->type = hashFindVal(store->typeHash, monomer->word);

/* Do second pass on ones that are not yet defined */
for (monomer = store->monomerList; monomer != NULL; monomer = monomer->next)
    {
    if (monomer->type == NULL)
	{
	struct monomerType *type = monomer->type = fillInTypeFromReads(monomer, store);
	verbose(2, "Got %p %s\n", type, (type != NULL ? type->name : "n/a"));
	}
    }
}

void alphaAsm(char *readsFile, char *monomerOrderFile, char *outFile)
/* alphaAsm - assemble alpha repeat regions such as centromeres from reads that have
 * been parsed into various repeat monomer variants.  Cycles of these variants tend to
 * form higher order repeats. */
{
struct alphaStore *store = alphaStoreNew(maxChainSize);
alphaReadListFromFile(readsFile, store);
alphaStoreLoadMonomerOrder(store, readsFile, monomerOrderFile);
fillInTypes(store);
integrateOrphans(store);
makeMarkovChains(store);
struct wordTree *wt = store->markovChains;
alphaStoreNormalize(store, outSize);

if (optionExists("chain"))
    {
    char *fileName = optionVal("chain", NULL);
    wordTreeWrite(wt, store->maxChainSize, fileName);
    }

wordTreeGenerateFile(store, store->maxChainSize, 
    pickWeightedRandomFromList(wt->children), outSize, outFile);

if (optionExists("afterChain"))
    {
    char *fileName = optionVal("afterChain", NULL);
    wordTreeWrite(wt, store->maxChainSize, fileName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
maxChainSize = optionInt("size", maxChainSize);
outSize = optionInt("outSize", outSize);
fullOnly = optionExists("fullOnly");
pseudoCount = optionInt("pseudoCount", pseudoCount);
int seed = optionInt("seed", (int)time(0));
srand(seed);
alphaAsm(argv[1], argv[2], argv[3]);
return 0;
}

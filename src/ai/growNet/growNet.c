/* growNet - Grow network of associaters and classifiers. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "options.h"
#include "dlist.h"
#include "obscure.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "growNet - Grow network of associaters and classifiers\n"
  "usage:\n"
  "   growNet input.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct associatorSubscription 
/* Records a subscription between associator and classifier. */
    {
    struct dlNode *assocNode;	    /* Associator node in classifier. */
    struct classifier *classifier;  /* Classifier itself. */
    };

struct classifierSubscription
/* Records subscription between classifier and interleaver. */
    {
    struct dlNode *classNode;	        /* Classifier node in interleaver. */
    struct interleaver *interleaver;	/* Interleaver itself. */
    };

struct prediction
/* A predicted symbol and it's probability */
    {
    int symbol;	/* Symbol being predicted. */
    float p;	/* Probability. */
    };

struct associator
/* This looks at a section of the input stream and
 * produces a probability that it matches. */
    {
    struct associator *next;	/* Next in list. */
    struct dlNode *selfNode;	/* Node on gn->associators list. */
    int id;			/* Unique identifier. */
    float score;		/* Only top scoring associators survive.  It's brutal. */
    float pRecognized;		/* Probability we've recognized input. */
    float pPredicted;		/* Probability we've predicted input. */
    struct prediction *predictions; /* Predictions for next output. */
    struct prediction *predBuf; /* Some prediction space. */
    int predictionCount;	/* Number of used predictions. */
    int predictionAlloc;	/* Number of allocated predictions. */
    struct interleaver *input;	/* Points to input interleaver. */
    struct dlList *subscriptions; /* List of associatorSubscriptions. */
    int *symbols;		/* Array of symbols it recognizes. */
    short symbolCount;		/* Number of symbols it recognizes. */
    bool forceOrder;		/* Symbols must be in defined order. */
    bool forceBoundaries;	/* Force first/last symbols to be matched. */
    bool reserved;		/* Some future expansion. */
    short uniqSymCount;		/* Count of unique symbols.  (Only used if !forceOrder)*/
    };

struct classifier
/* This makes connections to various associators, and at each instant
 * picks the id of the highest scoring associator as the output symbol. */
    {
    struct classifier *next;	/* Next in list. */
    struct dlNode *selfNode;	/* Node on gn->classifiers list. */
    int id;			/* Unique identifier. */
    float score;		/* Only top scoring classifiers survive. */
    struct associator *output;  /* Associator that is source of output. */
    struct dlList *inList;      /* List of input associators. */
    struct dlList *inDone;	/* During propagation inList moved here. */
    struct dlList *subscriptions; /* List of classifierSubscriptions. */
    };

struct interleaver
/* This interleaves two or more symbol streams into a single one. */
    {
    struct interleaver *next;
    struct dlNode *selfNode;		/* Node on gn->interleavers list. */
    int id;				/* Unique identifier. */
    float score;			/* Only top scoring survive. */
    int history[20];			/* Output symbol. */
    struct dlList *inList;		/* List of all input classifiers. */
    struct dlList *inDone;		/* During propagation inList moved here. */
    struct dlList *subscriptions;	/* List of associators that subscribe. */
    bool compressRuns;			/* Compress runs of input into output. */
    };

struct growNet
/* A growing network of associaters and classifiers. */
    {
    struct growNet *next;
    int nextId;		/* Next ID to use. */
    struct dlList *associators;	 /* List of all associators. */
    struct dlList *classifiers;	 /* List of all classifiers. */
    struct dlList *interleavers; /* List of all interleavers.. */
    struct interleaver *inputInterleaver;  /* This brings input into system. */
    };

void dumpSymbols(int *symbols, int count)
/* Dump symbols - as ascii if possible. */
{
int i;
for (i=0; i<count; ++i)
    {
    int v = symbols[i];
    if (v == 0)
        break;
    if (v < 0x100)
	{
	if (v == 0)
	    printf("^0");
	else
	    printf("%c ", v);
	}
    else
        printf("%x ", v);
    }
}

void dumpAssociator(struct associator *assoc)
/* Dump out current state of associator */
{
char *type = "ordered";
if (!assoc->forceOrder)
    type = (assoc->forceBoundaries ? "boundaries" : "unordered");
printf("as %d: %s %f '", assoc->id,  type,
	assoc->score);
dumpSymbols(assoc->symbols, assoc->symbolCount);
printf("'\n");
}

void dumpInterleaver(struct interleaver *il)
/* Dump out current state of interleaver. */
{
printf("il %d: '", il->id);
dumpSymbols(il->history, ArraySize(il->history));
printf("'\n");
}

struct dlNode *randomCycleList(struct dlList *list)
/* Pick a random number from 1 to 15.  Move the head of the
 * list to the tail that number of times, and return the head. */
{
struct dlNode *node;
int count = (rand()&15) + 1;
if (list == NULL || dlEmpty(list))
    return NULL;
while (--count >= 0)
    {
    node = dlPopHead(list);
    dlAddTail(list, node);
    }
return list->head;
}


struct associator *associatorNewEmpty(struct growNet *gn, struct interleaver *il)
/* Create a new associator that subscribes to interleaver. */
{
struct associator *assoc;
AllocVar(assoc);
if (gn != NULL)
    assoc->selfNode = dlAddValTail(gn->associators, assoc);
assoc->id = gn->nextId++;
assoc->input = il;
dlAddValTail(il->subscriptions, assoc);
assoc->subscriptions = dlListNew();
return assoc;
}

void allocatePredictions(struct associator *assoc)
/* Figure out size of predictions this associator will make,
 * and allocate them. */
{
if (assoc->forceOrder)
    {
    assoc->predictionAlloc = 1;
    assoc->predictions = AllocArray(assoc->predBuf, assoc->predictionAlloc);
    }
else
    {
    int i, j;
    int count = 0;
    int *symbols = assoc->symbols;
    float invCount;

    /* Figure out how many distinct symbols. */
    for (i=0; i<assoc->symbolCount; ++i)
        {
	boolean already = FALSE;
	int sym = symbols[i];
	for (j=0; j<i; ++j)
	    {
	    if (symbols[j] == sym)
		{
	        already = TRUE;
		break;
		}
	    }
	if (!already)
	    ++count;
	}
    assoc->uniqSymCount = count;
    if (assoc->forceBoundaries)
	++count;	/* One for terminal prediction too. */
    assoc->predictionAlloc = count;

    /* Allocate array and fill it in with our symbols. */
    assoc->predictions = AllocArray(assoc->predBuf, count);
    count = 0;
    for (i=0; i<assoc->symbolCount; ++i)
        {
	boolean already = FALSE;
	int sym = symbols[i];
	for (j=0; j<i; ++j)
	    {
	    if (symbols[j] == sym)
		{
	        already = TRUE;
		break;
		}
	    }
	if (!already)
	    {
	    assoc->predictions[count].symbol = sym;
	    ++count;
	    }
	}

    /* Go through one more time and figure out how to weigh output
     * probabilities. */
    for (i=0; i<assoc->symbolCount; ++i)
        {
	int sym = symbols[i];
	for (j=0; j<count; ++j)
	    {
	    if (assoc->predictions[j].symbol == sym)
		{
	        assoc->predictions[j].p += 1;
		break;
		}
	    }
	}
    assoc->predictionCount = assoc->predictionAlloc;

    /* Normalize weights to add up to one. */
    invCount = 1.0/assoc->symbolCount;
    uglyf("   weights ");
    for (i=0; i<count; ++i)
	{
        assoc->predictions[i].p *= invCount;
	    { /* uglyf */
	    int sym = assoc->predictions[i].symbol; /* uglyf */
	    if (sym == 0)
	        uglyf("^0");
	    else
	        uglyf("%c", sym);
	    uglyf(" %f,", assoc->predictions[i].p);
	    }
	}
    uglyf("\n");
    }
}

struct associator *associatorNewRandom(struct growNet *gn)
/* Create a new random associator. */
{
/* We cycle through each of the arrays below to determine the
 * attributes for the next associator.  Since the size of the
 * arrays is coprime, we should in fact end up exploring all
 * possible combinations. */
static int symbolCounts[] = {1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static int symbolIx = 0;
static int orders[] = {1, 0, 0};
static int orderIx = 0;
static int boundaries[] = {1, 0};
static int boundaryIx = 0;
struct dlNode *ilNode = randomCycleList(gn->interleavers);
struct interleaver *il = ilNode->val;
struct associator *assoc = associatorNewEmpty(gn, il);
int i;

assert(ArraySize(symbolCounts) == 29);
assoc->symbolCount = symbolCounts[symbolIx];
if (++symbolIx >= ArraySize(symbolCounts)) symbolIx = 0;
assoc->forceOrder = orders[orderIx];
if (++orderIx >= ArraySize(orders)) orderIx = 0;
assoc->forceBoundaries = boundaries[boundaryIx];
if (++boundaryIx >= ArraySize(boundaries)) boundaryIx = 0;
if (assoc->symbolCount < 2)
    assoc->forceBoundaries = FALSE;
AllocArray(assoc->symbols, assoc->symbolCount);
assert(assoc->symbolCount <= ArraySize(il->history));
for (i=0; i<assoc->symbolCount; ++i)
    assoc->symbols[i] = il->history[i];
uglyf("Creating associator on %d: %d %d %d\n   ", il->id, assoc->symbolCount, assoc->forceOrder, assoc->forceBoundaries);
dumpAssociator(assoc);
allocatePredictions(assoc);
assoc->score = 0.5;
return assoc;
}

struct interleaver *interleaverNew(struct growNet *gn, boolean compressRuns)
/* Create new interleaver not attached to anything. */
{
struct interleaver *il;
int i;
AllocVar(il);
il->id = gn->nextId++;
il->selfNode = dlAddValTail(gn->interleavers, il);
il->inList = dlListNew();
il->inDone = dlListNew();
il->subscriptions = dlListNew();
il->compressRuns = compressRuns;
for (i=0; i < ArraySize(il->history); ++i)
    il->history[i] = ' ';
return il;
}

void interleaverAddStream(struct interleaver *il, struct classifier *cl)
/* Add classifier stream to interleaver. */
{
struct classifierSubscription *sub;
AllocVar(sub);
sub->classNode = dlAddValTail(il->inList, cl);
sub->interleaver = il;
dlAddValTail(cl->subscriptions, sub);
}


struct growNet *growNetNew()
/* Create new growNet structure */
{
struct growNet *gn;
struct interleaver *il;
AllocVar(gn);
gn->nextId = 0x100;
gn->associators = dlListNew();
gn->classifiers = dlListNew();
gn->interleavers = dlListNew();
gn->inputInterleaver = il = interleaverNew(gn, FALSE);
dlAddValTail(gn->interleavers, il);
return gn;
}


float pRecognizeUnordered(int *history, int histCount, int *symbols, int symCount)
/* Figure out probability of history from 0 to histCount 
 * (history is in reverse order) matching unordered symbols. */
{
int symIx, histIx;
int matchCount = 0, mismatchCount = 0;
float pRecognized = 0.0;

/* Figure out how many associator symbols are covered in 'sniff area' */
for (symIx=0; symIx < symCount; ++symIx)
    {
    int sym = symbols[symIx];
    for (histIx = 0; histIx < histCount; ++histIx)
	{
	if (history[histIx] == sym)
	    {
	    ++matchCount;
	    break;
	    }
	}
    }

/* No matches, we're out of here. */
if (matchCount == 0)
    return 0.0;

/* Some matches, start computing recognition probability. */
pRecognized = (double)matchCount / symCount;

/* Figured out in local area how much of history matches symbols */
matchCount = mismatchCount = 0;
for (histIx = 0; histIx < histCount; ++histIx)
    {
    int sym = history[histIx];
    boolean subMatch = FALSE;
    for (symIx = 0; symIx < symCount; ++symIx)
	{
	if (symbols[symIx] == sym)
	    {
	    subMatch = TRUE;
	    break;
	    }
	}
    if (subMatch)
	++matchCount;
    else
	++mismatchCount;
    if (matchCount >= symCount)  /* We may be sniffing more than we need. */
	break;
    }
pRecognized *= (double)matchCount / (matchCount + mismatchCount);
return pRecognized;
}

void associatorNext(struct associator *assoc, struct interleaver *il)
/* Have associator process next input from interleaver. */
{
int *symbols = assoc->symbols;
int *history = il->history;
if (assoc->forceOrder)
    {
    int startIx, i;
    boolean match = FALSE;

    for (startIx=0; startIx<assoc->symbolCount; ++startIx)
	{
	int i;
	boolean subMatch = TRUE;
	for (i=startIx; i<assoc->symbolCount; ++i)
	    {
	    if (symbols[i] != history[i - startIx])
		{
		subMatch = FALSE;
		break;
		}
	    }
	if (subMatch)
	    {
	    match = TRUE;
	    break;
	    }
	}
    if (match)
	{
	if (startIx == 0)
	    {
	    assoc->pPredicted = 0.0;
	    assoc->predictionCount = 0;
	    assoc->pRecognized = 1.0;
	    }
	else
	    {
	    assoc->predictionCount = 1;
	    assoc->predictions[0].p = 1.0;
	    assoc->predictions[0].symbol = assoc->symbols[startIx-1];
	    assoc->pRecognized = (double)(assoc->symbolCount - startIx)
		    /assoc->symbolCount;
	    assoc->pPredicted = assoc->pRecognized;
	    }
	}
    else
	{
	assoc->pRecognized = assoc->pPredicted = 0.0;
	}
    }
else if (assoc->forceBoundaries)
    {
    int maxToSniff = assoc->symbolCount*1.0;  /* Possibly make this another parameter */
    int startIx, bestStartIx = -1;
    int startSym = assoc->symbols[assoc->symbolCount-1];
    double pRecognized, pRecognizedBest = 0.0;
    double pRecogStart = 0.0;

    if (maxToSniff > ArraySize(il->history))
        maxToSniff = ArraySize(il->history);
    /* See if we have our beginning somewhere in history.  If not
     * just move on. */
    for (startIx = maxToSniff-1; startIx >= 0; --startIx)
        {
	if (history[startIx] != startSym)
	    continue;
	pRecognized = pRecognizeUnordered(history, startIx+1, symbols, assoc->symbolCount);
	if (pRecognized > pRecognizedBest)
	    {
	    pRecognizedBest = pRecognized;
	    bestStartIx = startIx;
	    }
	}
    assoc->pRecognized = pRecognizedBest;
    assoc->predictionCount = assoc->predictionAlloc;

    if (bestStartIx >= 0)
	{
	int histCount = bestStartIx;
	if (histCount > ArraySize(il->history))
	     histCount = ArraySize(il->history);
	pRecogStart = pRecognizeUnordered(history, histCount, symbols+1, assoc->symbolCount-1);
	}
    if (pRecogStart >= 0.9)
        {
	assoc->pPredicted = pRecogStart;
	assoc->predictions = assoc->predBuf + assoc->uniqSymCount;
	assoc->predictionCount = 1;
	assoc->predictions[0].symbol = symbols[0];
	assoc->predictions[0].p = 1.0;
	}
    else if (pRecognizedBest >= 0.95)
        {
	assoc->pPredicted = 0.0;
	assoc->predictionCount = 0;
	assoc->predictions = assoc->predBuf;
	}
    else
        {
	assoc->pPredicted = assoc->pRecognized;
	assoc->predictions = assoc->predBuf;
	assoc->predictionCount = assoc->uniqSymCount;
	}
    }
else  /* No boundaries, no order. */
    {
    int histIx, symIx, matchCount = 0, mismatchCount = 0;
    int maxToSniff = assoc->symbolCount*1.0;  /* Possibly make this another parameter */
    if (maxToSniff > ArraySize(il->history))
        maxToSniff = ArraySize(il->history);
    assoc->pRecognized *= (double)matchCount / (matchCount + mismatchCount);
    assoc->pRecognized = assoc->pPredicted =
        pRecognizeUnordered(history, maxToSniff, symbols, assoc->symbolCount);
    }
if (assoc->pPredicted > 0.0 || assoc->pRecognized > 0.0)
    {
    uglyf("rec/pred by\n   ");
    dumpAssociator(assoc);
    uglyf("   rec %f, pred %f:",
	assoc->pRecognized, assoc->pPredicted);
    if (assoc->predictionCount > 0)
        {
	int i;
	for (i=0; i<assoc->predictionCount; ++i)
	    uglyf(" %c %4.2f,", assoc->predictions[i].symbol, assoc->predictions[i].p);
	}
    uglyf("\n");
    }
}

void classifierNext(struct classifier *cl)
/* Process inputs and produce output. */
{
struct dlNode *node;
struct associator *bestAssoc = NULL;
double p, bestP = 0.0;
for (node = cl->inList->head; !dlEnd(node); node = node->next)
    {
    struct associator *assoc = node->val;
    p = assoc->pRecognized * assoc->predictions[0].p;
    if (p > 0.0)
	{
	if (bestAssoc == NULL || p > bestP)
	     {
	     bestAssoc = assoc;
	     bestP = p;
	     }
	}
    }
cl->output = bestAssoc;
if (bestAssoc != NULL)
    uglyf("classifier %d picked %d, prob %f\n", cl->id, bestAssoc->id, bestP);
else
    uglyf("classifier %d declines to pick\n", cl->id);
}

void interleaverRecordInput(struct interleaver *il, int input)
/* Record input and cycle input history. */
{
int i;
int *history = il->history;
int *pt = history + ArraySize(il->history) - 2;
while (pt >= history)
    {
    pt[1] = pt[0];
    --pt;
    }
*history = input;
}

void interleaverNext(struct interleaver *il)
/* Figure out next input stream to use, and copy it to output. */
{
struct dlNode *node;
int symsThisRound[16];
int i, roundCount = 0;
boolean copyOver = TRUE;
for (node = il->inList->head; !dlEnd(node); node = node->next)
    {
    struct classifier *cl = node->val;
    if (roundCount >= ArraySize(symsThisRound))
        errAbort("Too many classifiers hooked up to interleaver");
    if (cl->output != NULL)
	{
	symsThisRound[roundCount] = cl->output->id;
	++roundCount;
	}
    }
if (roundCount > 0)
    {
    if (il->compressRuns)
	{
	int *history = il->history;
	copyOver = FALSE;
	for (i=0; i<roundCount; ++i)
	    {
	    if (history[i] != symsThisRound[i])
		{
		copyOver = TRUE;
		break;
		}
	    }
	}
    if (copyOver)
	{
	for (i=roundCount-1; i>=0; --i)
	    interleaverRecordInput(il, symsThisRound[i]);
	}
    }
}

void swapLists(struct dlList **pA, struct dlList **pB)
/* Swap two lists a and b. */
{
struct dlList *temp = *pA;
*pA = *pB;
*pB = temp;
}

void propagateFromInterleaver(struct growNet *gn, struct interleaver *il);
/* Predeclare this function. */

void propagateFromClassifier(struct growNet *gn, struct classifier *cl)
/* Propagate from classifier to attached interleavers.  If we are the
 * interleaver's last input cause it to fire. */
{
struct dlNode *node;
for (node = cl->subscriptions->head; !dlEnd(node); node = node->next)
    {
    struct classifierSubscription  *sub = node->val;
    struct interleaver *il = sub->interleaver;
    dlRemove(sub->classNode);
    dlAddTail(il->inDone, sub->classNode);
    if (dlEmpty(il->inList))
        {
	swapLists(&il->inList, &il->inDone);
	interleaverNext(il);
	propagateFromInterleaver(gn, il);
	}
    }
}

void propagateFromAssociator(struct growNet *gn, struct associator *assoc)
/* Propagate from associator to attatched classifiers.  If we are the
 * last of the classifier's inputs, cause it to fire. */
{
struct dlNode *node;
for (node = assoc->subscriptions->head; !dlEnd(node); node = node->next)
    {
    struct associatorSubscription  *sub = node->val;
    struct classifier *cl = sub->classifier;
    dlRemove(sub->assocNode);
    dlAddTail(cl->inDone, sub->assocNode);
    if (dlEmpty(cl->inList))
        {
	swapLists(&cl->inList, &cl->inDone);
	classifierNext(cl);
	propagateFromClassifier(gn, cl);
	}
    }
}

void propagateFromInterleaver(struct growNet *gn, struct interleaver *il)
/* Propagate from interleaver to all attached associators */
{
struct dlNode *node;
for (node = il->subscriptions->head; !dlEnd(node); node = node->next)
    {
    struct associator *assoc = node->val;
    associatorNext(assoc, il);
    propagateFromAssociator(gn, assoc);
    }
}

struct classifier *classifierNewRandom(struct growNet *gn, int inputCount)
/* Create new classifier with inputCount random inputs. */
{
struct classifier *cl;
int i;

AllocVar(cl);
cl->id = gn->nextId++;
cl->selfNode = dlAddValTail(gn->classifiers, cl);
cl->inList = dlListNew();
cl->inDone = dlListNew();
cl->subscriptions = dlListNew();
uglyf("classifierNewRandom(%d) - id = %d connects to associators: ", inputCount, cl->id);

for (i=0; i<inputCount; ++i)
    {
    struct dlNode *gnNode = randomCycleList(gn->associators);
    struct associator *assoc = gnNode->val;
    struct dlNode *aNode;
    struct associatorSubscription *sub;

    aNode = dlAddValTail(cl->inList, assoc);
    AllocVar(sub);
    sub->assocNode = aNode;
    sub->classifier = cl;
    dlAddValTail(assoc->subscriptions, sub);
    uglyf(" %d", assoc->id); 
    }
uglyf("\n");
return cl;
}


void growNetNext(struct growNet *gn, int sym)
/* Feed next symbol to grow net. */
{
struct associator *assoc;
struct classifier *cl;
struct interleaver *il;
int i;
interleaverRecordInput(gn->inputInterleaver, sym);
dumpInterleaver(gn->inputInterleaver);
propagateFromInterleaver(gn, gn->inputInterleaver);
for (i=0; i<5; ++i)
    assoc = associatorNewRandom(gn);
cl = classifierNewRandom(gn, 3);
il = interleaverNew(gn, TRUE);
interleaverAddStream(il, cl);
il = interleaverNew(gn, FALSE);
interleaverAddStream(il, cl);
}

void growNet(char *inFile)
/* growNet - Grow network of associaters and classifiers. */
{
char *inBuf;
size_t i, inSize;
struct growNet *gn = growNetNew();
readInGulp(inFile, &inBuf, &inSize);
for (i=0; i<inSize; ++i)
    growNetNext(gn, inBuf[i]);
verbose(1, "At end %d associators, %d classifiers, %d interleavers\n",
	dlCount(gn->associators), dlCount(gn->classifiers), dlCount(gn->interleavers));
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(100000000);
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
growNet(argv[1]);
carefulCheckHeap();
return 0;
}
